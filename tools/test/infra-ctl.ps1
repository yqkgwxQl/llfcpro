#requires -Version 5.1
<#
.SYNOPSIS
    LLFCChatPro 基础设施控制脚本（MySQL / Redis 的查看、启动、停止）。

.DESCRIPTION
    供测试使用：让测试脚本可以自己把 MySQL / Redis 拉起来，而不是只能“检测到没起”。

    - MySQL：通过 Windows 服务（自动发现名字含 mysql 的服务）启停；**需要管理员权限**。
    - Redis：通过 redis-server.exe + 临时配置启动（端口/密码取自节点的 config.ini），
      脚本会记录自己启动的进程 PID，之后只停止“自己启动的”实例。

    只读原则：不修改数据库内容、不删除数据文件；Redis 以 --save "" 启动，不落盘。

.PARAMETER Action
    Status（默认）| Start | Stop

.PARAMETER Target
    All（默认）| MySql | Redis

.PARAMETER Root
    仓库根目录，缺省为脚本所在目录的上一级。

.PARAMETER Force
    对“不是本脚本启动的”Redis 进程，允许强制停止（默认拒绝，避免误杀你手工启动的实例）。

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\infra-ctl.ps1

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\infra-ctl.ps1 -Action Start -Target Redis

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\infra-ctl.ps1 -Action Stop -Target All -Force
#>
[CmdletBinding()]
param(
    [ValidateSet("Status", "Start", "Stop")]
    [string]$Action = "Status",

    [ValidateSet("All", "MySql", "Redis")]
    [string]$Target = "All",

    [string]$Root = "",

    [switch]$Force
)

$ErrorActionPreference = "Continue"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$Root = (Resolve-Path -LiteralPath $Root).Path

$pidFile = Join-Path $env:TEMP "llfc-infra-redis.pid"
$confFile = Join-Path $env:TEMP "llfc-infra-redis.conf"
$dataDir = Join-Path $env:TEMP "llfc-infra-redis-data"

function Read-Ini {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    $ini = @{}
    $section = $null
    foreach ($raw in Get-Content -LiteralPath $Path -Encoding UTF8) {
        $line = $raw.Trim()
        if ($line -eq "" -or $line.StartsWith(";") -or $line.StartsWith("#")) { continue }
        if ($line -match '^\[(.+)\]$') {
            $section = $Matches[1].Trim()
            if (-not $ini.ContainsKey($section)) { $ini[$section] = @{} }
            continue
        }
        $idx = $line.IndexOf('=')
        if ($idx -lt 1) { continue }
        if ($section) { $ini[$section][$line.Substring(0, $idx).Trim()] = $line.Substring($idx + 1).Trim() }
    }
    return $ini
}

function Test-Listening {
    param([int]$Port)
    try {
        $c = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction Stop
        return (($c | Measure-Object).Count -gt 0)
    } catch { return $false }
}

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $pr = New-Object Security.Principal.WindowsPrincipal($id)
    return $pr.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# ---------- 发现配置与可执行文件 ------------------------------------------

$nodeConfigs = @()
$nodesDir = Join-Path $Root "server/ChatServer/nodes"
if (Test-Path -LiteralPath $nodesDir) {
    $nodeConfigs = @(Get-ChildItem -LiteralPath $nodesDir -Recurse -Filter config.ini -ErrorAction SilentlyContinue)
}
$cfg = $null
foreach ($f in $nodeConfigs) {
    $candidate = Read-Ini $f.FullName
    if ($candidate -and $candidate["Redis"] -and $candidate["Mysql"]) { $cfg = $candidate; break }
}

$redisHost = "127.0.0.1"
$redisPort = 0
$redisPwd = ""
$mysqlHost = "127.0.0.1"
$mysqlPort = 0

if ($cfg) {
    if ($cfg["Redis"]["Host"])  { $redisHost = $cfg["Redis"]["Host"] }
    if ($cfg["Redis"]["Port"])  { $redisPort = [int]$cfg["Redis"]["Port"] }
    if ($cfg["Redis"]["Passwd"]) { $redisPwd = $cfg["Redis"]["Passwd"] }
    if ($cfg["Mysql"]["Host"])  { $mysqlHost = $cfg["Mysql"]["Host"] }
    if ($cfg["Mysql"]["Port"])  { $mysqlPort = [int]$cfg["Mysql"]["Port"] }
}

function Find-RedisServer {
    $candidates = @()
    if ($env:REDIS_HOME) { $candidates += (Join-Path $env:REDIS_HOME "redis-server.exe") }
    $candidates += @(
        "F:\YingYongRuanJian3\redis\redis-server.exe",
        "C:\Program Files\Redis\redis-server.exe",
        "C:\Program Files (x86)\Redis\redis-server.exe"
    )
    $cmd = Get-Command redis-server -ErrorAction SilentlyContinue
    if ($cmd) { $candidates += $cmd.Source }
    foreach ($p in $candidates) { if (Test-Path -LiteralPath $p) { return $p } }
    return $null
}

function Find-MySqlService {
    $svcs = @(Get-Service -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -match 'mysql' -or $_.DisplayName -match 'mysql'
    })
    if ($svcs.Count -eq 0) { return $null }
    # 优先选择正在运行的
    $running = $svcs | Where-Object Status -eq "Running" | Select-Object -First 1
    if ($running) { return $running }
    return ($svcs | Select-Object -First 1)
}

# ---------- MySQL ----------------------------------------------------------

function Show-MySql {
    $svc = Find-MySqlService
    if (-not $svc) {
        Write-Host "MySQL : 未发现 Windows 服务（可能用其它方式运行）"
        return
    }
    $listen = if ($mysqlPort -gt 0) { Test-Listening $mysqlPort } else { $false }
    Write-Host ("MySQL : 服务={0} 状态={1} 监听{2}={3}" -f $svc.Name, $svc.Status, $mysqlPort, $listen)
}

function Start-MySql {
    $svc = Find-MySqlService
    if (-not $svc) { Write-Host "MySQL : 未发现 Windows 服务，无法自动启动"; return $false }
    if ($svc.Status -eq "Running") {
        Write-Host ("MySQL : 已在运行（{0}）" -f $svc.Name)
        return $true
    }
    if (-not (Test-Admin)) {
        Write-Host ("MySQL : 启动服务 {0} 需要管理员权限，请以管理员身份运行本脚本" -f $svc.Name)
        return $false
    }
    try {
        Start-Service -Name $svc.Name -ErrorAction Stop
        Write-Host ("MySQL : 已启动 {0}" -f $svc.Name)
        return $true
    } catch {
        Write-Host ("MySQL : 启动失败 - {0}" -f $_.Exception.Message)
        return $false
    }
}

function Stop-MySql {
    $svc = Find-MySqlService
    if (-not $svc) { Write-Host "MySQL : 未发现 Windows 服务，无法自动停止"; return $false }
    if ($svc.Status -ne "Running") { Write-Host ("MySQL : 未运行（{0}）" -f $svc.Name); return $true }
    if (-not (Test-Admin)) {
        Write-Host ("MySQL : 停止服务 {0} 需要管理员权限，请以管理员身份运行本脚本" -f $svc.Name)
        return $false
    }
    try {
        Stop-Service -Name $svc.Name -Force -ErrorAction Stop
        Write-Host ("MySQL : 已停止 {0}" -f $svc.Name)
        return $true
    } catch {
        Write-Host ("MySQL : 停止失败 - {0}" -f $_.Exception.Message)
        return $false
    }
}

# ---------- Redis ----------------------------------------------------------

function Show-Redis {
    $listen = if ($redisPort -gt 0) { Test-Listening $redisPort } else { $false }
    $owned = Test-Path -LiteralPath $pidFile
    $pidText = if ($owned) { (Get-Content -LiteralPath $pidFile -ErrorAction SilentlyContinue) } else { "" }
    Write-Host ("Redis : {0}:{1} 监听={2} 由本脚本启动={3}{4}" -f $redisHost, $redisPort, $listen, $owned, $(if ($pidText) { " (pid $pidText)" } else { "" }))
}

function Start-Redis {
    if ($redisPort -le 0) { Write-Host "Redis : 配置里没有合法端口，无法启动"; return $false }
    if (Test-Listening $redisPort) {
        Write-Host ("Redis : {0} 已在监听，跳过启动" -f $redisPort)
        return $true
    }
    $exe = Find-RedisServer
    if (-not $exe) {
        Write-Host "Redis : 找不到 redis-server.exe（可设置 REDIS_HOME 或加入 PATH）"
        return $false
    }

    if (-not (Test-Path -LiteralPath $dataDir)) {
        New-Item -ItemType Directory -Force -Path $dataDir | Out-Null
    }

    # 临时配置：只监听本机、绑定配置里的端口和密码、不落盘
    $lines = @(
        "bind 127.0.0.1",
        ("port {0}" -f $redisPort),
        ("dir {0}" -f $dataDir.Replace('\', '/')),
        'save ""',
        "appendonly no"
    )
    if ($redisPwd) { $lines += ("requirepass {0}" -f $redisPwd) }
    $lines | Set-Content -LiteralPath $confFile -Encoding ASCII

    try {
        $proc = Start-Process -FilePath $exe -ArgumentList ("`"{0}`"" -f $confFile) -PassThru -WindowStyle Hidden
    } catch {
        Write-Host ("Redis : 启动失败 - {0}" -f $_.Exception.Message)
        return $false
    }

    Set-Content -LiteralPath $pidFile -Value $proc.Id -Encoding ASCII

    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 200
        if (Test-Listening $redisPort) {
            Write-Host ("Redis : 已启动（pid {0}，端口 {1}）" -f $proc.Id, $redisPort)
            return $true
        }
        if ($proc.HasExited) { break }
    }

    Write-Host ("Redis : 启动后端口 {0} 未监听（进程可能立即退出，检查配置/端口占用）" -f $redisPort)
    return $false
}

function Stop-Redis {
    if (-not (Test-Path -LiteralPath $pidFile)) {
        if (-not (Test-Listening $redisPort)) {
            Write-Host ("Redis : {0} 未监听，无需停止" -f $redisPort)
            return $true
        }
        if (-not $Force) {
            Write-Host ("Redis : {0} 上的实例不是本脚本启动的（无 pid 记录），默认不停止；确需停止请加 -Force" -f $redisPort)
            return $false
        }
        $conn = Get-NetTCPConnection -State Listen -LocalPort $redisPort -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $conn) { Write-Host "Redis : 未找到监听进程"; return $false }
        try {
            Stop-Process -Id $conn.OwningProcess -Force -ErrorAction Stop
            Write-Host ("Redis : 已强制停止 pid {0}（-Force）" -f $conn.OwningProcess)
            return $true
        } catch {
            Write-Host ("Redis : 停止失败 - {0}" -f $_.Exception.Message)
            return $false
        }
    }

    $processId = 0
    [int]::TryParse((Get-Content -LiteralPath $pidFile -ErrorAction SilentlyContinue), [ref]$processId) | Out-Null
    if ($processId -gt 0) {
        $p = Get-Process -Id $processId -ErrorAction SilentlyContinue
        if ($p) {
            try { Stop-Process -Id $processId -Force -ErrorAction Stop; Write-Host ("Redis : 已停止 pid {0}" -f $processId) }
            catch { Write-Host ("Redis : 停止失败 - {0}" -f $_.Exception.Message); return $false }
        }
    }
    Remove-Item -LiteralPath $pidFile -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $confFile -Force -ErrorAction SilentlyContinue
    return $true
}

# ---------- 执行 -----------------------------------------------------------

$ok = $true

switch ($Action) {
    "Status" {
        if ($Target -in @("All", "MySql")) { Show-MySql }
        if ($Target -in @("All", "Redis")) { Show-Redis }
    }
    "Start" {
        if ($Target -in @("All", "MySql")) { if (-not (Start-MySql)) { $ok = $false } }
        if ($Target -in @("All", "Redis")) { if (-not (Start-Redis)) { $ok = $false } }
    }
    "Stop" {
        if ($Target -in @("All", "Redis")) { if (-not (Stop-Redis)) { $ok = $false } }
        if ($Target -in @("All", "MySql")) { if (-not (Stop-MySql)) { $ok = $false } }
    }
}

if (-not $ok) { exit 1 } else { exit 0 }
