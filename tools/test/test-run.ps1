#requires -Version 5.1
<#
.SYNOPSIS
    LLFCChatPro 测试场景运行器：按场景自动准备环境、跑检查、恢复现场。

.DESCRIPTION
    把 tools/test/health-check.ps1（检查）和 tools/test/infra-ctl.ps1（启停 MySQL/Redis）组合成可重复的场景：

      offline        离线测试：不联网，只校验仓库结构、配置、日志
      ready          在线测试（基础设施就绪）：期望健康检查全部通过
      redis-down     在线测试（Redis 未启动）：停掉 Redis → 期望检查失败 → 恢复 Redis → 期望检查通过
      mysql-down     在线测试（MySQL 未启动）：同上，但需要管理员权限

    每个场景结束时都会尽量把环境恢复到运行前的状态：
      - 如果 Redis 是脚本自己启动的，结束时会被关掉；
      - 如果 MySQL 是脚本停止的，结束时会被重新启动。

.PARAMETER Scenario
    offline | ready | redis-down | mysql-down | all（依次执行 offline 与 ready）

.PARAMETER StartInfra
    ready / redis-down 场景开始前，先尝试启动 MySQL 与 Redis。

.PARAMETER Force
    允许停止“不是本脚本启动的” Redis 实例（谨慎使用）。

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\test-run.ps1 -Scenario offline

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\test-run.ps1 -Scenario ready -StartInfra

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\test-run.ps1 -Scenario redis-down
#>
[CmdletBinding()]
param(
    [ValidateSet("offline", "ready", "redis-down", "mysql-down", "all")]
    [string]$Scenario = "ready",

    [switch]$StartInfra,

    [switch]$Force,

    [string]$Root = ""
)

$ErrorActionPreference = "Continue"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$Root = (Resolve-Path -LiteralPath $Root).Path

$healthScript = Join-Path $PSScriptRoot "health-check.ps1"
$infraScript  = Join-Path $PSScriptRoot "infra-ctl.ps1"
$psExe = Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0\powershell.exe"

$script:assertions = New-Object System.Collections.ArrayList

function Add-Assertion {
    param([string]$Name, [bool]$Ok, [string]$Detail = "")
    [void]$script:assertions.Add([pscustomobject]@{
        Name   = $Name
        Result = $(if ($Ok) { "PASS" } else { "FAIL" })
        Detail = $Detail
    })
}

function Invoke-HealthCheck {
    param([switch]$NoNetwork)

    $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $healthScript, "-Root", $Root)
    if ($NoNetwork) { $arguments += "-NoNetwork" }

    $output = & $psExe @arguments 2>&1
    $code = $LASTEXITCODE
    return [pscustomobject]@{
        ExitCode = $code
        Output   = ($output | Out-String)
    }
}

function Invoke-Infra {
    param([string]$Action, [string]$Target)

    $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $infraScript, "-Root", $Root, "-Action", $Action, "-Target", $Target)
    if ($Force) { $arguments += "-Force" }

    $output = & $psExe @arguments 2>&1
    $code = $LASTEXITCODE
    return [pscustomobject]@{ ExitCode = $code; Output = ($output | Out-String) }
}

function Get-RedisListening {
    param([int]$Port)
    try {
        $c = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction Stop
        return (($c | Measure-Object).Count -gt 0)
    } catch { return $false }
}

function Get-RedisPort {
    $iniPath = Get-ChildItem -LiteralPath (Join-Path $Root "server/ChatServer/nodes") -Recurse -Filter config.ini -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $iniPath) { return 0 }
    $section = $null
    foreach ($raw in Get-Content -LiteralPath $iniPath.FullName -Encoding UTF8) {
        $line = $raw.Trim()
        if ($line -match '^\[(.+)\]$') { $section = $Matches[1].Trim(); continue }
        if ($section -eq "Redis" -and $line -match '^\s*Port\s*=\s*(\d+)\s*$') { return [int]$Matches[1] }
    }
    return 0
}

function Write-HealthTail {
    param([string]$Output, [int]$Lines = 12)
    ($Output -split "`r?`n" | Select-Object -Last $Lines) | ForEach-Object { "    " + $_ }
}

# ---------- 场景实现 -------------------------------------------------------

function Run-Offline {
    Write-Output "== 场景: 离线测试（不联网） =="
    $r = Invoke-HealthCheck -NoNetwork
    Add-Assertion "离线检查通过（无 FAIL）" ($r.ExitCode -eq 0) ("exit=" + $r.ExitCode)
    Write-HealthTail $r.Output
}

function Run-Ready {
    Write-Output "== 场景: 在线测试（基础设施就绪） =="
    if ($StartInfra) {
        Write-Output "-- 启动基础设施 --"
        $i = Invoke-Infra -Action Start -Target All
        Write-HealthTail $i.Output 6
    }

    $r = Invoke-HealthCheck
    Add-Assertion "在线检查通过（无 FAIL）" ($r.ExitCode -eq 0) ("exit=" + $r.ExitCode)
    Write-HealthTail $r.Output
}

function Run-RedisDown {
    Write-Output "== 场景: 在线测试（Redis 未启动） =="

    $port = Get-RedisPort
    $wasListening = if ($port -gt 0) { Get-RedisListening $port } else { $false }

    try {
        # 确保有一个“可被本脚本停止”的 Redis：已运行且非本脚本启动时会拒绝，除非 -Force
        $start = Invoke-Infra -Action Start -Target Redis
        Write-HealthTail $start.Output 4

        $stop = Invoke-Infra -Action Stop -Target Redis
        Write-HealthTail $stop.Output 4
        Add-Assertion "能够停止 Redis" ($stop.ExitCode -eq 0) ("exit=" + $stop.ExitCode)

        Start-Sleep -Seconds 1
        $down = Invoke-HealthCheck
        $redisFail = ($down.Output -match "Redis 连通性\s+FAIL")
        Add-Assertion "Redis 停止后检查应报 FAIL" (($down.ExitCode -ne 0) -and $redisFail) ("exit=" + $down.ExitCode)
        Write-HealthTail $down.Output

        $restart = Invoke-Infra -Action Start -Target Redis
        Write-HealthTail $restart.Output 4
        Start-Sleep -Seconds 1

        $up = Invoke-HealthCheck
        Add-Assertion "Redis 恢复后检查应通过" ($up.ExitCode -eq 0) ("exit=" + $up.ExitCode)
        Write-HealthTail $up.Output
    }
    finally {
        # 恢复现场：如果开始时 Redis 本来没在跑，就把它停掉
        if (-not $wasListening) {
            Write-Output "-- 恢复现场：停止测试期间启动的 Redis --"
            $cleanup = Invoke-Infra -Action Stop -Target Redis
            Write-HealthTail $cleanup.Output 3
        }
    }
}

function Run-MySqlDown {
    Write-Output "== 场景: 在线测试（MySQL 未启动） =="

    $stop = Invoke-Infra -Action Stop -Target MySql
    Write-HealthTail $stop.Output 4
    if ($stop.ExitCode -ne 0) {
        Add-Assertion "能够停止 MySQL" $false "需要管理员权限，场景跳过"
        Write-Output "（MySQL 场景需要以管理员身份运行本脚本，已跳过）"
        return
    }

    try {
        $down = Invoke-HealthCheck
        $mysqlFail = ($down.Output -match "MySQL 连通性\s+FAIL")
        Add-Assertion "MySQL 停止后检查应报 FAIL" (($down.ExitCode -ne 0) -and $mysqlFail) ("exit=" + $down.ExitCode)
        Write-HealthTail $down.Output
    }
    finally {
        Write-Output "-- 恢复现场：重新启动 MySQL --"
        $restart = Invoke-Infra -Action Start -Target MySql
        Write-HealthTail $restart.Output 4

        $up = Invoke-HealthCheck
        Add-Assertion "MySQL 恢复后检查应通过" ($up.ExitCode -eq 0) ("exit=" + $up.ExitCode)
    }
}

# ---------- 调度与汇总 -----------------------------------------------------

Write-Output ("仓库: {0}" -f $Root)
Write-Output ("时间: {0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
Write-Output ""

$scenarios = if ($Scenario -eq "all") { @("offline", "ready") } else { @($Scenario) }

foreach ($s in $scenarios) {
    switch ($s) {
        "offline"    { Run-Offline }
        "ready"      { Run-Ready }
        "redis-down" { Run-RedisDown }
        "mysql-down" { Run-MySqlDown }
    }
    Write-Output ""
}

Write-Output "== 断言汇总 =="
$script:assertions | Select-Object Name, Result, Detail | Format-Table -AutoSize -Wrap

$failed = @($script:assertions | Where-Object { $_.Result -eq "FAIL" }).Count
Write-Output ("总计: {0} 条断言，失败 {1} 条" -f $script:assertions.Count, $failed)

if ($failed -gt 0) { exit 1 } else { exit 0 }
