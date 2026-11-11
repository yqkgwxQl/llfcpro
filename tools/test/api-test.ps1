#requires -Version 5.1
<#
.SYNOPSIS
    接口级「写库」测试：注册接口 + 数据备份 + 精确清理恢复。

.DESCRIPTION
    这是**唯一会写数据库**的测试脚本，采用「先备份、只删自己造的数据、最后校验行数」的策略：

      1. 记录基线：user 行数、user 表 AUTO_INCREMENT、user_id 发号器当前值
      2. 备份：mysqldump 导出 user / user_id 两张表到 %TEMP%\llfc-api-backup\<时间戳>\
      3. 造数据：在 Redis 写入验证码 code_<测试邮箱>，POST /user_register
      4. 校验：响应 error==0，且库里能查到该邮箱
      5. 清理：DELETE 测试用户 → 恢复 user_id 发号器 → 重置 AUTO_INCREMENT → 删除 Redis 验证码
      6. 复查：user 行数与基线一致；不一致时可用 -RestoreOnMismatch 从备份还原

    默认是 **dry-run**（只打印计划，不写任何东西），必须显式加 -AllowWrite 才会执行。

    前置条件：GateServer、MySQL、Redis 已启动（可先跑
    `tools\test\service-run.ps1 -KeepRunning` 保留现场）。

.PARAMETER AllowWrite
    真正执行写库测试。没有这个开关时只做 dry-run。

.PARAMETER RestoreOnMismatch
    清理后行数仍与基线不一致时，用备份文件还原 user / user_id 表。

.PARAMETER KeepTestUser
    不删除测试用户（调试用；会打印该用户信息，需自行清理）。

.PARAMETER Root
    仓库根目录，缺省为脚本所在目录的上两级。

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\api-test.ps1
    # dry-run：只打印计划

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\api-test.ps1 -AllowWrite
#>
[CmdletBinding()]
param(
    [switch]$AllowWrite,
    [switch]$RestoreOnMismatch,
    [switch]$KeepTestUser,
    [string]$Root = ""
)

$ErrorActionPreference = "Continue"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$Root = (Resolve-Path -LiteralPath $Root).Path

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$testEmail = "llfctest_$stamp@example.com"
$testUser = "llfctest_$stamp"
$testPwd = "Test_$stamp"
$testCode = "654321"
$backupDir = Join-Path $env:TEMP ("llfc-api-backup\" + $stamp)

$script:asserts = New-Object System.Collections.ArrayList

function Add-Assert {
    param([string]$Name, [bool]$Ok, [string]$Detail = "")
    [void]$script:asserts.Add([pscustomobject]@{ Name = $Name; Result = $(if ($Ok) { "PASS" } else { "FAIL" }); Detail = $Detail })
}

function Read-Ini {
    param([string]$Path)
    $ini = @{}
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
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

# ---------- 读取配置 -------------------------------------------------------

$ini = Read-Ini (Join-Path $Root "server/GateServer/build/config.ini")
if (-not $ini) { $ini = Read-Ini (Join-Path $Root "server/ChatServer/nodes/chatserver1/config.ini") }
if (-not $ini) { Write-Host "找不到配置：server/GateServer/build/config.ini"; exit 1 }

$gatePort = if ($ini["GateServer"]["Port"]) { [int]$ini["GateServer"]["Port"] } else { 8080 }
$myHost = $ini["Mysql"]["Host"]; $myPort = [int]$ini["Mysql"]["Port"]
$myUser = $ini["Mysql"]["User"]; $myPwd = $ini["Mysql"]["Passwd"]; $myDb = $ini["Mysql"]["Schema"]
$rdHost = $ini["Redis"]["Host"]; $rdPort = [int]$ini["Redis"]["Port"]; $rdPwd = $ini["Redis"]["Passwd"]

$mysqlExe = "F:\YingYongRuanJian3\mysql\mysql-8.4.7-winx64\bin\mysql.exe"
$dumpExe  = "F:\YingYongRuanJian3\mysql\mysql-8.4.7-winx64\bin\mysqldump.exe"
$redisCli = "F:\YingYongRuanJian3\redis\redis-cli.exe"

$gateUrl = "http://127.0.0.1:$gatePort/user_register"

# ---------- 工具 -----------------------------------------------------------

function Invoke-MySql {
    param([string]$Sql)
    $env:MYSQL_PWD = $myPwd
    try {
        $out = & $mysqlExe --host=$myHost --port=$myPort --user=$myUser --database=$myDb --protocol=TCP --batch --skip-column-names -e $Sql 2>&1
        if ($LASTEXITCODE -ne 0) { throw (($out | Out-String).Trim()) }
        return (($out | Out-String).Trim())
    } finally {
        Remove-Item Env:\MYSQL_PWD -ErrorAction SilentlyContinue
    }
}

function Invoke-Redis {
    param([string[]]$RedisArgs)
    $env:REDISCLI_AUTH = $rdPwd
    try {
        $out = & $redisCli -h $rdHost -p $rdPort --no-auth-warning @RedisArgs 2>&1
        return (($out | Out-String).Trim())
    } finally {
        Remove-Item Env:\REDISCLI_AUTH -ErrorAction SilentlyContinue
    }
}

function Get-Count {
    param([string]$Sql)
    $v = Invoke-MySql $Sql
    $n = 0
    [int]::TryParse(($v -split "`r?`n")[0], [ref]$n) | Out-Null
    return $n
}

# ---------- 前置检查 -------------------------------------------------------

$preOk = $true
if (-not (Test-Listening $gatePort)) { Write-Host ("[!] GateServer 端口 {0} 未监听，请先运行 tools\test\service-run.ps1 -KeepRunning" -f $gatePort); $preOk = $false }
if (-not (Test-Listening $myPort))   { Write-Host ("[!] MySQL {0}:{1} 未监听" -f $myHost, $myPort); $preOk = $false }
if (-not (Test-Listening $rdPort))   { Write-Host ("[!] Redis {0}:{1} 未监听" -f $rdHost, $rdPort); $preOk = $false }
foreach ($f in @($mysqlExe, $dumpExe, $redisCli)) { if (-not (Test-Path $f)) { Write-Host ("[!] 缺少工具: " + $f); $preOk = $false } }
if (-not $preOk) { exit 1 }

Write-Host ("测试用户: {0} / {1}" -f $testUser, $testEmail)
Write-Host ("目标接口: {0}" -f $gateUrl)

if (-not $AllowWrite) {
    Write-Host ""
    Write-Host "== DRY-RUN（未加 -AllowWrite，不会写任何数据） =="
    Write-Host "将执行："
    Write-Host "  1) 记录基线：user 行数 / AUTO_INCREMENT / user_id 发号器"
    Write-Host "  2) 备份 user、user_id 到 $backupDir"
    Write-Host "  3) Redis SET code_<email> = $testCode"
    Write-Host "  4) POST /user_register"
    Write-Host "  5) 校验用户名/邮箱入库"
    Write-Host "  6) 清理：DELETE 测试用户、恢复 user_id、重置 AUTO_INCREMENT、DEL 验证码"
    Write-Host "  7) 复查行数与基线一致"
    exit 0
}

# ---------- 基线 + 备份 -----------------------------------------------------

New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
$dumpFile = Join-Path $backupDir "user.sql"

$baselineUserCount = Get-Count "SELECT COUNT(*) FROM user;"
$baselineMaxId     = Get-Count "SELECT COALESCE(MAX(id),0) FROM user;"
$baselineUserId    = Get-Count "SELECT id FROM user_id;"
$baselineAutoInc   = Get-Count "SELECT AUTO_INCREMENT FROM information_schema.TABLES WHERE TABLE_SCHEMA='$myDb' AND TABLE_NAME='user';"

Write-Host ("基线: user 行数={0} 最大id={1} 发号器={2} AUTO_INCREMENT={3}" -f $baselineUserCount, $baselineMaxId, $baselineUserId, $baselineAutoInc)

$env:MYSQL_PWD = $myPwd
try {
    & $dumpExe --host=$myHost --port=$myPort --user=$myUser --protocol=TCP --single-transaction --add-drop-table $myDb user user_id 2>&1 |
        Set-Content -LiteralPath $dumpFile -Encoding UTF8
    $dumpOk = ($LASTEXITCODE -eq 0) -and ((Get-Item $dumpFile).Length -gt 0)
} finally {
    Remove-Item Env:\MYSQL_PWD -ErrorAction SilentlyContinue
}
Add-Assert "备份 user / user_id" $dumpOk ("文件: " + $dumpFile)

# ---------- 执行测试 -------------------------------------------------------

$created = $false
$registerOk = $false
try {
    Write-Host ""
    Write-Host "== 1) 写入验证码并调用注册接口 =="
    Invoke-Redis @("SET", "code_$testEmail", $testCode, "EX", "600") | Out-Null

    $body = @{ user = $testUser; email = $testEmail; passwd = $testPwd; confirm = $testPwd; varifycode = $testCode; icon = "" } | ConvertTo-Json -Compress
    $resp = Invoke-RestMethod -Uri $gateUrl -Method Post -ContentType "application/json" -Body $body -TimeoutSec 10
    $created = ($resp.error -eq 0)
    $registerOk = $created
    Add-Assert "注册接口返回 error==0" $created ("error=" + $resp.error + " uid=" + $resp.uid)

    $dbCount = Get-Count ("SELECT COUNT(*) FROM user WHERE email='{0}';" -f $testEmail)
    Add-Assert "数据库能查到测试用户" ($dbCount -eq 1) ("email 命中 {0} 行" -f $dbCount)

    $uidIssued = Get-Count ("SELECT uid FROM user WHERE email='{0}';" -f $testEmail)
    Add-Assert "发号器确实递增（uid>0）" ($uidIssued -gt 0) ("uid=" + $uidIssued)
}
catch {
    Add-Assert "注册接口调用" $false $_.Exception.Message
}
finally {
    Write-Host ""
    Write-Host "== 2) 清理测试数据 =="

    if ($KeepTestUser) {
        Write-Host ("[!] -KeepTestUser：保留测试用户 {0}（uid 见上），需手工清理" -f $testEmail)
    } else {
        try {
            Invoke-MySql ("DELETE FROM user WHERE email='{0}' OR name='{1}';" -f $testEmail, $testUser) | Out-Null
            Invoke-MySql ("UPDATE user_id SET id={0};" -f $baselineUserId) | Out-Null
            Invoke-MySql ("ALTER TABLE user AUTO_INCREMENT={0};" -f $baselineAutoInc) | Out-Null
            Invoke-Redis @("DEL", "code_$testEmail") | Out-Null
            Write-Host "  已删除测试用户、恢复发号器与 AUTO_INCREMENT、删除验证码"
        } catch {
            Write-Host ("  [!] 清理失败: " + $_.Exception.Message)
        }
    }

    Write-Host ""
    Write-Host "== 3) 复查基线 =="
    $afterCount = Get-Count "SELECT COUNT(*) FROM user;"
    $afterUserId = Get-Count "SELECT id FROM user_id;"
    Add-Assert "user 行数恢复" ($afterCount -eq $baselineUserCount) ("{0} -> {1}" -f $baselineUserCount, $afterCount)
    Add-Assert "user_id 发号器恢复" ($afterUserId -eq $baselineUserId) ("{0} -> {1}" -f $baselineUserId, $afterUserId)

    if ($afterCount -ne $baselineUserCount) {
        if ($RestoreOnMismatch) {
            Write-Host "  [i] 行数不一致，使用备份还原 user / user_id"
            $env:MYSQL_PWD = $myPwd
            try {
                Get-Content -LiteralPath $dumpFile -Raw | & $mysqlExe --host=$myHost --port=$myPort --user=$myUser --protocol=TCP $myDb 2>&1 | Out-Null
                $restored = Get-Count "SELECT COUNT(*) FROM user;"
                Add-Assert "从备份还原" ($restored -eq $baselineUserCount) ("行数=" + $restored)
            } finally {
                Remove-Item Env:\MYSQL_PWD -ErrorAction SilentlyContinue
            }
        } else {
            Write-Host "  [!] 行数不一致；可加 -RestoreOnMismatch 从备份还原"
        }
    }
}

Write-Host ""
Write-Host "== 断言汇总 =="
$script:asserts | Select-Object Name, Result, Detail | Format-Table -AutoSize -Wrap

$failed = @($script:asserts | Where-Object { $_.Result -eq "FAIL" }).Count
Write-Host ("总计: {0} 条断言，失败 {1} 条；备份目录: {2}" -f $script:asserts.Count, $failed, $backupDir)
if ($failed -gt 0) { exit 1 } else { exit 0 }
