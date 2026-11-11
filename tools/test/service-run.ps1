#requires -Version 5.1
<#
.SYNOPSIS
    启动整套服务（Redis / StatusServer / GateServer / ChatServer，可选 Qt 客户端）并做冒烟检查。

.DESCRIPTION
    把「手工开四个窗口」的流程自动化：
      1. 启动 Redis（复用 infra-ctl.ps1，记录是否由本脚本启动）
      2. 启动 StatusServer、GateServer、ChatServer(chatserver1)
      3. 等待端口监听（TCP/gRPC）
      4. 跑 health-check.ps1 并汇总
      5. 可选 -WithClient：启动 Qt 客户端，确认主窗口出现
      6. 结束时停止本脚本启动的进程（-KeepRunning 可保留现场）

    客户端注意：本机 llfcchat.exe 需要 **MinGW 15.1 的运行时**先于 Qt 的 bin 被加载，
    否则会以 0xC0000139（找不到 DLL 入口点）立即退出。脚本已按此顺序拼 PATH。

.PARAMETER WithClient
    同时启动 Qt 客户端并检查主窗口。

.PARAMETER KeepRunning
    结束后不停止服务/客户端，方便继续手工测试。

.PARAMETER WaitSeconds
    每个端口等待就绪的最长时间，默认 15 秒。

.PARAMETER Root
    仓库根目录，缺省为脚本所在目录的上两级。

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\service-run.ps1

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\service-run.ps1 -WithClient
#>
[CmdletBinding()]
param(
    [switch]$WithClient,
    [switch]$KeepRunning,
    [int]$WaitSeconds = 15,
    [string]$Root = ""
)

$ErrorActionPreference = "Continue"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$Root = (Resolve-Path -LiteralPath $Root).Path

$psExe = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"
$infraScript = Join-Path $PSScriptRoot "infra-ctl.ps1"
$healthScript = Join-Path $PSScriptRoot "health-check.ps1"
$runDir = Join-Path $env:TEMP "llfc-service-run"
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$script:procs = New-Object System.Collections.ArrayList
$script:redisWasRunning = $false

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

function Wait-Port {
    param([int]$Port, [int]$Timeout = $WaitSeconds)
    $deadline = (Get-Date).AddSeconds($Timeout)
    while ((Get-Date) -lt $deadline) {
        if (Test-Listening $Port) { return $true }
        Start-Sleep -Milliseconds 250
    }
    return (Test-Listening $Port)
}

function Start-ServiceExe {
    param([string]$Name, [string]$Exe, [string]$WorkDir, [string[]]$Arguments = @(), [switch]$Show)

    if (-not (Test-Path -LiteralPath $Exe)) {
        Write-Host ("  [{0}] 缺少可执行文件: {1}" -f $Name, $Exe)
        return $null
    }

    $out = Join-Path $runDir ("{0}.out.log" -f $Name)
    $err = Join-Path $runDir ("{0}.err.log" -f $Name)
    Remove-Item $out, $err -Force -ErrorAction SilentlyContinue

    $p = $null
    $params = @{
        FilePath               = $Exe
        WorkingDirectory       = $WorkDir
        PassThru               = $true
        RedirectStandardOutput = $out
        RedirectStandardError  = $err
    }
    # 控制台服务用隐藏窗口；GUI 客户端必须显示，否则主窗口不会被创建为可见窗口
    if (-not $Show) { $params["WindowStyle"] = "Hidden" }
    if ($Arguments -and $Arguments.Count -gt 0) { $params["ArgumentList"] = $Arguments }

    try {
        $p = Start-Process @params
    } catch {
        Write-Host ("  [{0}] 启动失败: {1}" -f $Name, $_.Exception.Message)
        return $null
    }

    [void]$script:procs.Add([pscustomobject]@{ Name = $Name; Proc = $p; Out = $out; Err = $err })
    Write-Host ("  [{0}] 已启动 pid={1}" -f $Name, $p.Id)
    return $p
}

function Show-LogTail {
    param([string]$Name, [int]$Lines = 8)
    $item = $script:procs | Where-Object { $_.Name -eq $Name } | Select-Object -First 1
    if (-not $item) { return }
    foreach ($f in @($item.Out, $item.Err)) {
        $tail = Get-Content -LiteralPath $f -Tail $Lines -ErrorAction SilentlyContinue
        if ($tail) {
            Write-Host ("  --- {0} ({1}) ---" -f $Name, (Split-Path $f -Leaf))
            $tail | ForEach-Object { Write-Host ("    " + $_) }
        }
    }
}

function Stop-Stack {
    for ($i = $script:procs.Count - 1; $i -ge 0; $i--) {
        $item = $script:procs[$i]
        $p = $item.Proc
        if ($p -and -not $p.HasExited) {
            try { Stop-Process -Id $p.Id -Force -ErrorAction Stop; Write-Host ("  [{0}] 已停止 pid={1}" -f $item.Name, $p.Id) }
            catch { Write-Host ("  [{0}] 停止失败: {1}" -f $item.Name, $_.Exception.Message) }
        }
    }
    if (-not $script:redisWasRunning) {
        $stop = & $psExe -NoProfile -ExecutionPolicy Bypass -File $infraScript -Root $Root -Action Stop -Target Redis 2>&1
        $stop | ForEach-Object { Write-Host ("  " + $_) }
    }
}

# ---------- 配置与端口 -----------------------------------------------------

$gateIni = Read-Ini (Join-Path $Root "server/GateServer/build/config.ini")
$statusIni = Read-Ini (Join-Path $Root "server/StatusServer/build/config.ini")
$chatIni = Read-Ini (Join-Path $Root "server/ChatServer/nodes/chatserver1/config.ini")

$gatePort = if ($gateIni) { [int]$gateIni["GateServer"]["Port"] } else { 0 }
$statusPort = if ($statusIni) { [int]$statusIni["StatusServer"]["Port"] } else { 0 }
$chatPort = if ($chatIni) { [int]$chatIni["SelfServer"]["Port"] } else { 0 }
$chatRpc = if ($chatIni) { [int]$chatIni["SelfServer"]["RPCPort"] } else { 0 }
$redisPort = if ($chatIni) { [int]$chatIni["Redis"]["Port"] } else { 0 }

Write-Host ("仓库: {0}" -f $Root)
Write-Host ("端口: Gate={0} Status={1} Chat={2}/{3} Redis={4}" -f $gatePort, $statusPort, $chatPort, $chatRpc, $redisPort)
Write-Host ""

# ---------- 启动 -----------------------------------------------------------

$ok = $true

try {
    Write-Host "== 1/4 启动 Redis =="
    $script:redisWasRunning = if ($redisPort -gt 0) { Test-Listening $redisPort } else { $false }
    $r = & $psExe -NoProfile -ExecutionPolicy Bypass -File $infraScript -Root $Root -Action Start -Target Redis 2>&1
    $r | ForEach-Object { Write-Host ("  " + $_) }
    if (-not (Wait-Port $redisPort 10)) { Write-Host "  [!] Redis 端口未就绪"; $ok = $false }

    Write-Host "== 2/4 启动 StatusServer / GateServer =="
    $null = Start-ServiceExe "StatusServer" (Join-Path $Root "server/StatusServer/build/STATUSSERVER.exe") (Join-Path $Root "server/StatusServer/build")
    $null = Start-ServiceExe "GateServer" (Join-Path $Root "server/GateServer/build/GATESERVER.exe") (Join-Path $Root "server/GateServer/build")

    if (-not (Wait-Port $statusPort)) { Write-Host ("  [!] StatusServer 端口 {0} 未就绪" -f $statusPort); $ok = $false; Show-LogTail "StatusServer" }
    if (-not (Wait-Port $gatePort))   { Write-Host ("  [!] GateServer 端口 {0} 未就绪" -f $gatePort);   $ok = $false; Show-LogTail "GateServer" }

    Write-Host "== 3/4 启动 ChatServer（chatserver1） =="
    $chatExe = Join-Path $Root "server/ChatServer/build/CHATSERVER.exe"
    $chatArgs = @("--config", "..\nodes\chatserver1\config.ini", "--logdir", "..\nodes\chatserver1\logs")
    $null = Start-ServiceExe "ChatServer" $chatExe (Join-Path $Root "server/ChatServer/build") $chatArgs

    if (-not (Wait-Port $chatPort)) { Write-Host ("  [!] ChatServer TCP {0} 未就绪" -f $chatPort); $ok = $false; Show-LogTail "ChatServer" }
    if (-not (Wait-Port $chatRpc))  { Write-Host ("  [!] ChatServer gRPC {0} 未就绪" -f $chatRpc); $ok = $false; Show-LogTail "ChatServer" }

    Write-Host ""
    Write-Host "== health-check（在线） =="
    $health = & $psExe -NoProfile -ExecutionPolicy Bypass -File $healthScript -Root $Root 2>&1
    $healthCode = $LASTEXITCODE
    ($health | Select-Object -Last 6) | ForEach-Object { Write-Host ("  " + $_) }
    if ($healthCode -ne 0) { $ok = $false }

    if ($WithClient) {
        Write-Host ""
        Write-Host "== 4/4 启动客户端 =="
        $mingwBin = "E:\YingYongRuanjian\mingw64\bin"
        $qtBin = "F:\YingYongRuanJian3\Qt\qita\6.11.0\mingw_64\bin"
        if (Test-Path $mingwBin) { $env:PATH = $mingwBin + ";" + $env:PATH }
        if (Test-Path $qtBin)    { $env:PATH = $qtBin + ";" + $env:PATH }

        $clientExe = Join-Path $Root "client/llfcchat/build/llfcchat.exe"
        $client = Start-ServiceExe "llfcchat" $clientExe (Join-Path $Root "client/llfcchat/build") -Show

        $windowOk = $false
        if ($client) {
            # 首次启动可能要加载 Qt 插件/资源，给足 30 秒
            for ($i = 0; $i -lt 120; $i++) {
                Start-Sleep -Milliseconds 250
                $client.Refresh()
                if ($client.HasExited) { break }
                if ($client.MainWindowHandle -ne 0) { $windowOk = $true; break }
            }
        }

        if ($client -and $client.HasExited) {
            Write-Host ("  [!] 客户端已退出，exitcode={0}（常见原因：缺少 DLL 或 PATH 中 Qt/MinGW 版本不匹配）" -f $client.ExitCode)
            $ok = $false
        } elseif (-not $windowOk) {
            Write-Host "  [!] 客户端进程在运行，但未检测到主窗口"
            $ok = $false
        } else {
            Write-Host ("  [llfcchat] 主窗口已出现，标题='{0}'" -f $client.MainWindowTitle)
        }
    }

    Write-Host ""
    if ($ok) { Write-Host "结论: 整套服务启动正常。" } else { Write-Host "结论: 有检查项未通过，见上方日志。" }
}
finally {
    if ($KeepRunning) {
        Write-Host ""
        Write-Host "== -KeepRunning：保留现场（Ctrl+C 不会自动清理） =="
        $script:procs | ForEach-Object { Write-Host ("  {0} pid={1}" -f $_.Name, $_.Proc.Id) }
    } else {
        Write-Host ""
        Write-Host "== 清理：停止本脚本启动的进程 =="
        Stop-Stack
    }
}

if ($ok) { exit 0 } else { exit 1 }
