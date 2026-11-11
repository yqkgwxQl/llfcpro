#requires -Version 5.1
<#
.SYNOPSIS
    后台栈控制：启动/停止/查看 Redis + 三个服务，且不拖住当前命令行作业。

.DESCRIPTION
    为什么需要它：
      service-run.ps1 用 Start-Process 启动服务时，子进程会留在当前作业（job）里，
      在 CI / 后台作业中表现为“作业一直 running，直到服务退出”。
      本脚本改用 WMI 的 Win32_Process.Create 启动服务：进程由系统服务创建，
      不属于当前作业，因此命令可以立即返回，服务继续在后台运行。

    用法：
      stack-ctl.ps1 -Action Start   启动 Redis + StatusServer + GateServer + ChatServer
      stack-ctl.ps1 -Action Stop    按端口找到进程并停止（只停本脚本配置里涉及的端口）
      stack-ctl.ps1 -Action Status  查看端口与进程

    启动的服务日志写到 %TEMP%\llfc-stack\；状态记录在 %TEMP%\llfc-stack\state.json，
    供 Stop 判断 Redis 是否需要一起停。

.PARAMETER Action
    Start | Stop | Status

.PARAMETER Root
    仓库根目录，缺省为脚本所在目录的上两级。

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\stack-ctl.ps1 -Action Start

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\stack-ctl.ps1 -Action Stop
#>
[CmdletBinding()]
param(
    [ValidateSet("Start", "Stop", "Status")]
    [string]$Action = "Status",

    [string]$Root = ""
)

$ErrorActionPreference = "Continue"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$Root = (Resolve-Path -LiteralPath $Root).Path

$runDir = Join-Path $env:TEMP "llfc-stack"
$stateFile = Join-Path $runDir "state.json"
$infraScript = Join-Path $PSScriptRoot "infra-ctl.ps1"
$psExe = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

function Read-Ini {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    $ini = @{}; $section = $null
    foreach ($raw in Get-Content -LiteralPath $Path -Encoding UTF8) {
        $line = $raw.Trim()
        if ($line -eq "" -or $line.StartsWith(";") -or $line.StartsWith("#")) { continue }
        if ($line -match '^\[(.+)\]$') { $section = $Matches[1].Trim(); if (-not $ini.ContainsKey($section)) { $ini[$section] = @{} }; continue }
        $idx = $line.IndexOf('=')
        if ($idx -lt 1) { continue }
        if ($section) { $ini[$section][$line.Substring(0, $idx).Trim()] = $line.Substring($idx + 1).Trim() }
    }
    return $ini
}

function Test-Listening {
    param([int]$Port)
    try { $c = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction Stop; return (($c | Measure-Object).Count -gt 0) } catch { return $false }
}

function Wait-Port {
    param([int]$Port, [int]$Timeout = 20)
    $deadline = (Get-Date).AddSeconds($Timeout)
    while ((Get-Date) -lt $deadline) { if (Test-Listening $Port) { return $true }; Start-Sleep -Milliseconds 250 }
    return (Test-Listening $Port)
}

function Get-PortOwner {
    param([int]$Port)
    $c = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($c) { return [int]$c.OwningProcess }
    return 0
}

# WMI 创建进程：不在当前作业里，命令可以立即返回
function Start-Detached {
    param([string]$Exe, [string[]]$Arguments, [string]$WorkDir, [string]$LogPath)

    $argLine = if ($Arguments -and $Arguments.Count -gt 0) { " " + ($Arguments -join " ") } else { "" }
    $cmdline = 'cmd.exe /c ""' + $Exe + '"' + $argLine + ' > "' + $LogPath + '" 2>&1"'
    $result = ([wmiclass]"Win32_Process").Create($cmdline, $WorkDir)
    if ($result.ReturnValue -ne 0) {
        Write-Host ("  [!] WMI 创建失败 ReturnValue={0}: {1}" -f $result.ReturnValue, $Exe)
        return $false
    }
    return $true
}

# ---------- 配置 -----------------------------------------------------------

$gateIni = Read-Ini (Join-Path $Root "server/GateServer/build/config.ini")
$statusIni = Read-Ini (Join-Path $Root "server/StatusServer/build/config.ini")
$chatIni = Read-Ini (Join-Path $Root "server/ChatServer/nodes/chatserver1/config.ini")

$gatePort   = if ($gateIni) { [int]$gateIni["GateServer"]["Port"] } else { 0 }
$statusPort = if ($statusIni) { [int]$statusIni["StatusServer"]["Port"] } else { 0 }
$chatPort   = if ($chatIni) { [int]$chatIni["SelfServer"]["Port"] } else { 0 }
$chatRpc    = if ($chatIni) { [int]$chatIni["SelfServer"]["RPCPort"] } else { 0 }
$redisPort  = if ($chatIni) { [int]$chatIni["Redis"]["Port"] } else { 0 }

# ---------- 动作 -----------------------------------------------------------

if ($Action -eq "Status") {
    foreach ($item in @(
        @{ Name = "Redis";        Port = $redisPort },
        @{ Name = "StatusServer"; Port = $statusPort },
        @{ Name = "GateServer";   Port = $gatePort },
        @{ Name = "ChatServer";   Port = $chatPort },
        @{ Name = "ChatServerRpc";Port = $chatRpc })) {
        if ($item.Port -le 0) { continue }
        $owner = Get-PortOwner $item.Port
        $state = if ($owner -gt 0) { "LISTEN pid=$owner" } else { "未监听" }
        Write-Host ("  {0,-14} {1,6}  {2}" -f $item.Name, $item.Port, $state)
    }
    if (Test-Path $stateFile) { Write-Host ("  状态文件: " + $stateFile); Get-Content $stateFile }
    exit 0
}

if ($Action -eq "Stop") {
    $targets = @($statusPort, $gatePort, $chatPort, $chatRpc) | Where-Object { $_ -gt 0 } | Sort-Object -Unique
    foreach ($port in $targets) {
        $owner = Get-PortOwner $port
        if ($owner -gt 0) {
            try { Stop-Process -Id $owner -Force -ErrorAction Stop; Write-Host ("  已停止端口 {0} 的进程 pid={1}" -f $port, $owner) }
            catch { Write-Host ("  [!] 停止端口 {0} 失败: {1}" -f $port, $_.Exception.Message) }
        }
    }

    $redisStarted = $false
    if (Test-Path $stateFile) {
        try {
            $state = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
            $redisStarted = [bool]$state.redisStarted
        } catch { }
    }
    if ($redisStarted) {
        $out = & $psExe -NoProfile -ExecutionPolicy Bypass -File $infraScript -Root $Root -Action Stop -Target Redis 2>&1
        $out | ForEach-Object { Write-Host ("  " + $_) }
    }

    Remove-Item -LiteralPath $stateFile -Force -ErrorAction SilentlyContinue
    exit 0
}

# Start
$redisWasRunning = if ($redisPort -gt 0) { Test-Listening $redisPort } else { $false }
Write-Host ("仓库: {0}" -f $Root)
Write-Host "== 启动 Redis =="
$r = & $psExe -NoProfile -ExecutionPolicy Bypass -File $infraScript -Root $Root -Action Start -Target Redis 2>&1
$r | ForEach-Object { Write-Host ("  " + $_) }
if (-not (Wait-Port $redisPort 15)) { Write-Host "  [!] Redis 未就绪" }

Write-Host "== 启动 StatusServer / GateServer / ChatServer（脱离当前作业） =="
$ok = $true
$services = @(
    @{ Name = "StatusServer"; Exe = (Join-Path $Root "server/StatusServer/build/STATUSSERVER.exe"); WorkDir = (Join-Path $Root "server/StatusServer/build"); Args = @(); Port = $statusPort },
    @{ Name = "GateServer";   Exe = (Join-Path $Root "server/GateServer/build/GATESERVER.exe");     WorkDir = (Join-Path $Root "server/GateServer/build");   Args = @(); Port = $gatePort },
    @{ Name = "ChatServer";   Exe = (Join-Path $Root "server/ChatServer/build/CHATSERVER.exe");     WorkDir = (Join-Path $Root "server/ChatServer/build");   Args = @("--config", "..\nodes\chatserver1\config.ini", "--logdir", "..\nodes\chatserver1\logs"); Port = $chatPort }
)

foreach ($s in $services) {
    if (-not (Test-Path -LiteralPath $s.Exe)) { Write-Host ("  [!] 缺少 " + $s.Exe); $ok = $false; continue }
    $log = Join-Path $runDir ($s.Name + ".log")
    Remove-Item $log -Force -ErrorAction SilentlyContinue
    if (Start-Detached -Exe $s.Exe -Arguments $s.Args -WorkDir $s.WorkDir -LogPath $log) {
        if (Wait-Port $s.Port 25) { Write-Host ("  [{0}] 已就绪，端口 {1}" -f $s.Name, $s.Port) }
        else { Write-Host ("  [!] {0} 端口 {1} 未就绪，日志尾部：" -f $s.Name, $s.Port); Get-Content $log -Tail 6 -ErrorAction SilentlyContinue | ForEach-Object { Write-Host ("      " + $_) }; $ok = $false }
    } else { $ok = $false }
}

if (-not (Wait-Port $chatRpc 20)) { Write-Host ("  [!] ChatServer gRPC {0} 未就绪" -f $chatRpc); $ok = $false }

$state = @{ redisStarted = (-not $redisWasRunning); startedAt = (Get-Date).ToString("s"); ports = @($redisPort, $statusPort, $gatePort, $chatPort, $chatRpc) }
$state | ConvertTo-Json -Compress | Set-Content -LiteralPath $stateFile -Encoding UTF8

Write-Host ""
Write-Host ("结论: {0}（状态文件: {1}）" -f $(if ($ok) { "后台栈已启动" } else { "部分服务未就绪" }), $stateFile)
if ($ok) { exit 0 } else { exit 1 }
