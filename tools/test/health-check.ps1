#requires -Version 5.1
<#
.SYNOPSIS
    LLFCChatPro 自动化健康检查脚本。

.DESCRIPTION
    在不需要启动 Qt 客户端的前提下，尽量自动化地检查项目当前状态：
      L0 仓库结构（构建产物 / 关键文件）
      L1 配置解析与一致性（必填项、端口、节点唯一性、对端段）
      L2 依赖连通性（MySQL / Redis，TCP + 可选 ping）
      L3 服务监听与进程（GateServer / StatusServer / ChatServer）
      L4 日志健康（ERROR 数量、连接抖动迹象）

    只读：不修改任何配置、不写数据库、不注册用户。

.PARAMETER Root
    仓库根目录。缺省为脚本所在目录的上一级（docs 的父目录）。

.PARAMETER TimeoutMs
    单个 TCP 连接检查的超时毫秒数，默认 1500。

.PARAMETER NoNetwork
    跳过所有网络检查（L2/L3 的端口探测），只做静态检查。

.PARAMETER Json
    以 JSON 输出结果，便于接入 CI 或其它脚本。

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\health-check.ps1

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\health-check.ps1 -NoNetwork

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\health-check.ps1 -Json | Out-File report.json -Encoding utf8
#>
[CmdletBinding()]
param(
    [string]$Root = "",
    [int]$TimeoutMs = 1500,
    [switch]$NoNetwork,
    [switch]$Json
)

$ErrorActionPreference = "Continue"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$Root = (Resolve-Path -LiteralPath $Root).Path

$script:results = New-Object System.Collections.ArrayList

function Add-Result {
    param(
        [string]$Area,
        [string]$Check,
        [ValidateSet("PASS", "WARN", "FAIL", "SKIP")]
        [string]$Status,
        [string]$Detail = ""
    )
    [void]$script:results.Add([pscustomobject]@{
        Area   = $Area
        Check  = $Check
        Status = $Status
        Detail = $Detail
    })
}

# ---------- 通用工具 -------------------------------------------------------

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
        $key = $line.Substring(0, $idx).Trim()
        $val = $line.Substring($idx + 1).Trim()
        if ($section) { $ini[$section][$key] = $val }
    }
    return $ini
}

function Test-Tcp {
    param([string]$TargetHost, [int]$Port, [int]$Timeout = $TimeoutMs)
    $client = New-Object System.Net.Sockets.TcpClient
    try {
        $iar = $client.BeginConnect($TargetHost, $Port, $null, $null)
        if (-not $iar.AsyncWaitHandle.WaitOne($Timeout)) {
            return [pscustomobject]@{ Ok = $false; Error = "timeout ${Timeout}ms" }
        }
        $client.EndConnect($iar)
        return [pscustomobject]@{ Ok = $true; Error = "" }
    } catch {
        $msg = $_.Exception.Message
        if ($_.Exception.InnerException) { $msg = $_.Exception.InnerException.Message }
        return [pscustomobject]@{ Ok = $false; Error = $msg }
    } finally {
        $client.Close()
    }
}

function Test-Listening {
    param([int]$Port)
    try {
        $conn = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction Stop
        return (($conn | Measure-Object).Count -gt 0)
    } catch {
        return $false
    }
}

function Test-PortNumber {
    param([string]$Value)
    if ([string]::IsNullOrWhiteSpace($Value)) { return $false }
    $n = 0
    if (-not [int]::TryParse($Value, [ref]$n)) { return $false }
    return ($n -gt 0 -and $n -le 65535)
}

function Get-LogStats {
    param([string]$Dir)
    if (-not (Test-Path -LiteralPath $Dir)) { return $null }
    $latest = Get-ChildItem -LiteralPath $Dir -Filter *.log -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $latest) { return $null }

    $tail = Get-Content -LiteralPath $latest.FullName -Tail 200 -Encoding UTF8 -ErrorAction SilentlyContinue
    if (-not $tail) { $tail = @() }

    # “认证成功”只在启动建池（5 条）和补位时出现；
    # 统计近 10 分钟的条数，避免把一天内多次启动的累计值误判成连接抖动。
    $recentAuth = 0
    $cutoff = (Get-Date).AddMinutes(-10)
    foreach ($line in $tail) {
        if ($line -notmatch '认证成功') { continue }
        if ($line -match '^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})') {
            $t = [datetime]::MinValue
            if ([datetime]::TryParseExact($Matches[1], 'yyyy-MM-dd HH:mm:ss', $null, [System.Globalization.DateTimeStyles]::None, [ref]$t)) {
                if ($t -ge $cutoff) { $recentAuth++ }
            }
        }
    }

    return [pscustomobject]@{
        File         = $latest.Name
        AgeMinutes   = [int]((Get-Date) - $latest.LastWriteTime).TotalMinutes
        Lines        = $tail.Count
        ErrorCount   = ($tail | Where-Object { $_ -match '\[ERROR\]|\[FATAL\]' }).Count
        AuthOkCount  = ($tail | Where-Object { $_ -match '认证成功' }).Count
        AuthOkRecent = $recentAuth
        RefillFail   = ($tail | Where-Object { $_ -match '补连接失败' }).Count
        TimeoutHits  = ($tail | Where-Object { $_ -match '连接池等待超时' }).Count
    }
}

# ---------- L0 仓库结构 ---------------------------------------------------

$exeMap = [ordered]@{
    "client/llfcchat/build/llfcchat.exe"          = "WARN"
    "server/GateServer/build/GATESERVER.exe"      = "FAIL"
    "server/StatusServer/build/STATUSSERVER.exe"  = "FAIL"
    "server/ChatServer/build/CHATSERVER.exe"      = "FAIL"
}

foreach ($rel in $exeMap.Keys) {
    $full = Join-Path $Root $rel
    if (Test-Path -LiteralPath $full) {
        Add-Result "L0 结构" $rel "PASS" ("{0:N0} KB" -f ((Get-Item $full).Length / 1KB))
    } else {
        Add-Result "L0 结构" $rel $exeMap[$rel] "未找到（未构建？）"
    }
}

foreach ($rel in @(
        "server/sql/schema.sql",
        "docs/测试指南.md", "docs/改造记录.md", "docs/文档同步清单.md",
        "tools/test/health-check.ps1", "tools/test/infra-ctl.ps1", "tools/test/test-run.ps1",
        "tools/test/service-run.ps1", "tools/test/stack-ctl.ps1", "tools/test/api-test.ps1", "tools/test/doc-sync.ps1")) {
    $full = Join-Path $Root $rel
    if (Test-Path -LiteralPath $full) {
        Add-Result "L0 结构" $rel "PASS" ""
    } else {
        Add-Result "L0 结构" $rel "WARN" "未找到"
    }
}

# ---------- L1 配置 -------------------------------------------------------

$nodesDir = Join-Path $Root "server/ChatServer/nodes"
$nodeConfigs = @()
if (Test-Path -LiteralPath $nodesDir) {
    $nodeConfigs = @(Get-ChildItem -LiteralPath $nodesDir -Recurse -Filter config.ini -ErrorAction SilentlyContinue)
}

if ($nodeConfigs.Count -eq 0) {
    Add-Result "L1 配置" "ChatServer 节点配置" "WARN" "nodes 下没有找到 config.ini（按模板重建）"
} else {
    Add-Result "L1 配置" "ChatServer 节点配置" "PASS" ("找到 {0} 份" -f $nodeConfigs.Count)
}

$nodeInfo = @()
foreach ($cfgFile in $nodeConfigs) {
    $label = $cfgFile.Directory.Name
    $ini = Read-Ini $cfgFile.FullName
    if (-not $ini) {
        Add-Result "L1 配置" "$label 解析" "FAIL" "读取失败"
        continue
    }

    $self = $ini["SelfServer"]
    $missing = @()
    foreach ($k in @("Name", "Host", "Port", "RPCPort")) {
        if (-not $self -or [string]::IsNullOrWhiteSpace($self[$k])) { $missing += $k }
    }
    if ($missing.Count -gt 0) {
        Add-Result "L1 配置" "$label [SelfServer]" "FAIL" ("缺少: " + ($missing -join ", "))
    } else {
        Add-Result "L1 配置" "$label [SelfServer]" "PASS" ("Name={0} Port={1} RPCPort={2}" -f $self.Name, $self.Port, $self.RPCPort)
    }

    $portOk = (Test-PortNumber $self.Port) -and (Test-PortNumber $self.RPCPort)
    Add-Result "L1 配置" "$label 端口合法性" $(if ($portOk) { "PASS" } else { "FAIL" }) ("TCP={0} gRPC={1}" -f $self.Port, $self.RPCPort)

    foreach ($dep in @("Mysql", "Redis")) {
        $sec = $ini[$dep]
        $bad = @()
        if (-not $sec) {
            $bad += $dep
        } else {
            if ([string]::IsNullOrWhiteSpace($sec["Host"])) { $bad += "Host" }
            if (-not (Test-PortNumber $sec["Port"])) { $bad += "Port" }
        }
        if ($bad.Count -gt 0) {
            Add-Result "L1 配置" "$label [$dep]" "FAIL" ("缺/非法: " + ($bad -join ", "))
        } else {
            Add-Result "L1 配置" "$label [$dep]" "PASS" ("{0}:{1}" -f $sec.Host, $sec.Port)
        }
    }

    $peers = @()
    if ($ini["PeerServer"] -and $ini["PeerServer"]["Servers"]) {
        $peers = @($ini["PeerServer"]["Servers"].Split(",") | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
    }
    if ($peers.Count -eq 0) {
        Add-Result "L1 配置" "$label [PeerServer]" "WARN" "未配置对端（单节点可忽略）"
    } else {
        $missingPeer = @($peers | Where-Object { -not $ini[$_] -or [string]::IsNullOrWhiteSpace($ini[$_]["Host"]) -or -not (Test-PortNumber $ini[$_]["Port"]) })
        if ($missingPeer.Count -gt 0) {
            Add-Result "L1 配置" "$label 对端段" "FAIL" ("缺少对应段/端口: " + ($missingPeer -join ", "))
        } else {
            Add-Result "L1 配置" "$label 对端段" "PASS" ("对端: " + ($peers -join ", "))
        }
    }

    $nodeInfo += [pscustomobject]@{
        Label    = $label
        Name     = $self.Name
        TcpPort  = if (Test-PortNumber $self.Port) { [int]$self.Port } else { 0 }
        RpcPort  = if (Test-PortNumber $self.RPCPort) { [int]$self.RPCPort } else { 0 }
        Mysql    = $ini["Mysql"]
        Redis    = $ini["Redis"]
        LogDir   = Join-Path $cfgFile.Directory.FullName "logs"
    }
}

if ($nodeInfo.Count -ge 2) {
    $dupName = @($nodeInfo | Group-Object Name | Where-Object { $_.Count -gt 1 })
    $dupTcp  = @($nodeInfo | Where-Object { $_.TcpPort -gt 0 } | Group-Object TcpPort | Where-Object { $_.Count -gt 1 })
    $dupRpc  = @($nodeInfo | Where-Object { $_.RpcPort -gt 0 } | Group-Object RpcPort | Where-Object { $_.Count -gt 1 })

    Add-Result "L1 配置" "节点 Name 唯一" $(if ($dupName.Count -eq 0) { "PASS" } else { "FAIL" }) $(if ($dupName.Count) { "重复: " + (($dupName | ForEach-Object { $_.Name }) -join ", ") } else { "" })
    Add-Result "L1 配置" "节点 TCP 端口唯一" $(if ($dupTcp.Count -eq 0) { "PASS" } else { "FAIL" }) $(if ($dupTcp.Count) { "重复: " + (($dupTcp | ForEach-Object { $_.Name }) -join ", ") } else { "" })
    Add-Result "L1 配置" "节点 gRPC 端口唯一" $(if ($dupRpc.Count -eq 0) { "PASS" } else { "FAIL" }) $(if ($dupRpc.Count) { "重复: " + (($dupRpc | ForEach-Object { $_.Name }) -join ", ") } else { "" })
}

# 其它服务端口（有 config.ini 才检查）
$gateIni = Read-Ini (Join-Path $Root "server/GateServer/config.ini")
$statusIni = Read-Ini (Join-Path $Root "server/StatusServer/config.ini")

$gatePort = 0
if ($gateIni -and (Test-PortNumber $gateIni["GateServer"]["Port"])) {
    $gatePort = [int]$gateIni["GateServer"]["Port"]
    Add-Result "L1 配置" "GateServer 端口" "PASS" "Port=$gatePort"
} elseif ($gateIni) {
    Add-Result "L1 配置" "GateServer 端口" "FAIL" "Port 非法或缺失"
}

$statusPort = 0
if ($statusIni -and (Test-PortNumber $statusIni["StatusServer"]["Port"])) {
    $statusPort = [int]$statusIni["StatusServer"]["Port"]
    Add-Result "L1 配置" "StatusServer 端口" "PASS" "Port=$statusPort"
} elseif ($statusIni) {
    Add-Result "L1 配置" "StatusServer 端口" "FAIL" "Port 非法或缺失"
}

# ---------- L2 依赖连通性 -------------------------------------------------

$primary = $nodeInfo | Select-Object -First 1

if ($NoNetwork) {
    Add-Result "L2 依赖" "MySQL 连通性" "SKIP" "-NoNetwork"
    Add-Result "L2 依赖" "Redis 连通性" "SKIP" "-NoNetwork"
} elseif ($primary) {
    $my = $primary.Mysql
    if ($my -and (Test-PortNumber $my["Port"])) {
        $t = Test-Tcp -TargetHost $my.Host -Port ([int]$my["Port"])
        Add-Result "L2 依赖" "MySQL 连通性" $(if ($t.Ok) { "PASS" } else { "FAIL" }) ("{0}:{1} {2}" -f $my.Host, $my["Port"], $t.Error)
    } else {
        Add-Result "L2 依赖" "MySQL 连通性" "SKIP" "配置缺失，无法探测"
    }

    $rd = $primary.Redis
    if ($rd -and (Test-PortNumber $rd["Port"])) {
        $t = Test-Tcp -TargetHost $rd.Host -Port ([int]$rd["Port"])
        Add-Result "L2 依赖" "Redis 连通性" $(if ($t.Ok) { "PASS" } else { "FAIL" }) ("{0}:{1} {2}" -f $rd.Host, $rd["Port"], $t.Error)

        $redisCli = $null
        $cmd = Get-Command redis-cli -ErrorAction SilentlyContinue
        if ($cmd) { $redisCli = $cmd.Source }
        if (-not $redisCli) {
            foreach ($cand in @(
                "F:\YingYongRuanJian3\redis\redis-cli.exe",
                "C:\Program Files\Redis\redis-cli.exe",
                "C:\Program Files (x86)\Redis\redis-cli.exe")) {
                if (Test-Path -LiteralPath $cand) { $redisCli = $cand; break }
            }
        }

        if ($redisCli -and $t.Ok) {
            # 用环境变量传密码，避免出现在命令行里
            if ($rd["Passwd"]) { $env:REDISCLI_AUTH = $rd["Passwd"] }
            try {
                $pong = & $redisCli -h $rd["Host"] -p $rd["Port"] --no-auth-warning PING 2>&1
                if ($LASTEXITCODE -eq 0 -and ($pong -join "") -match "PONG") {
                    Add-Result "L2 依赖" "Redis PING" "PASS" "PONG"
                } else {
                    Add-Result "L2 依赖" "Redis PING" "WARN" (($pong -join " ") -replace '\s+', ' ')
                }
            } catch {
                Add-Result "L2 依赖" "Redis PING" "WARN" $_.Exception.Message
            } finally {
                Remove-Item Env:\REDISCLI_AUTH -ErrorAction SilentlyContinue
            }
        } else {
            Add-Result "L2 依赖" "Redis PING" "SKIP" "未找到 redis-cli 或 TCP 不通"
        }
    } else {
        Add-Result "L2 依赖" "Redis 连通性" "SKIP" "配置缺失，无法探测"
    }
} else {
    Add-Result "L2 依赖" "MySQL 连通性" "SKIP" "没有可用的节点配置"
    Add-Result "L2 依赖" "Redis 连通性" "SKIP" "没有可用的节点配置"
}

# ---------- L3 服务监听 / 进程 -------------------------------------------

$listenTargets = @()
foreach ($n in $nodeInfo) {
    if ($n.TcpPort -gt 0) { $listenTargets += [pscustomobject]@{ Label = "$($n.Label) TCP"; Port = $n.TcpPort } }
    if ($n.RpcPort -gt 0) { $listenTargets += [pscustomobject]@{ Label = "$($n.Label) gRPC"; Port = $n.RpcPort } }
}
if ($gatePort -gt 0)   { $listenTargets += [pscustomobject]@{ Label = "GateServer"; Port = $gatePort } }
if ($statusPort -gt 0) { $listenTargets += [pscustomobject]@{ Label = "StatusServer"; Port = $statusPort } }

if ($NoNetwork) {
    Add-Result "L3 服务" "端口监听" "SKIP" "-NoNetwork"
} else {
    foreach ($t in $listenTargets) {
        if (Test-Listening $t.Port) {
            Add-Result "L3 服务" $t.Label "PASS" ("监听 {0}" -f $t.Port)
        } else {
            Add-Result "L3 服务" $t.Label "WARN" ("{0} 未监听（服务未启动？）" -f $t.Port)
        }
    }
}

foreach ($name in @("GATESERVER", "STATUSSERVER", "CHATSERVER", "llfcchat")) {
    $p = Get-Process -Name $name -ErrorAction SilentlyContinue
    if ($p) {
        Add-Result "L3 服务" "进程 $name" "PASS" ("Pid " + (($p | ForEach-Object { $_.Id }) -join ","))
    } else {
        Add-Result "L3 服务" "进程 $name" "WARN" "未运行"
    }
}

# GateServer HTTP 探活（只取根路径，不触发业务写操作）
if (-not $NoNetwork -and $gatePort -gt 0 -and (Test-Listening $gatePort)) {
    try {
        $resp = Invoke-WebRequest -Uri ("http://127.0.0.1:{0}/" -f $gatePort) -TimeoutSec 3 -UseBasicParsing -ErrorAction Stop
        Add-Result "L3 服务" "GateServer HTTP" "PASS" ("HTTP " + [int]$resp.StatusCode)
    } catch {
        $we = $_.Exception
        if ($we.Response) {
            Add-Result "L3 服务" "GateServer HTTP" "PASS" ("HTTP 响应（" + [int]$we.Response.StatusCode + "）")
        } else {
            Add-Result "L3 服务" "GateServer HTTP" "WARN" $we.Message
        }
    }
} elseif (-not $NoNetwork) {
    Add-Result "L3 服务" "GateServer HTTP" "SKIP" "未监听"
}

# ---------- L4 日志健康 ---------------------------------------------------

if ($nodeInfo.Count -eq 0) {
    Add-Result "L4 日志" "ChatServer 日志" "SKIP" "无节点配置"
} else {
    foreach ($n in $nodeInfo) {
        $stats = Get-LogStats $n.LogDir
        if (-not $stats) {
            Add-Result "L4 日志" "$($n.Label) 日志" "WARN" "没有找到日志文件（服务没跑过？）"
            continue
        }

        $detail = "{0}（{1} 分钟前，末 {2} 行）" -f $stats.File, $stats.AgeMinutes, $stats.Lines
        Add-Result "L4 日志" "$($n.Label) 日志" "PASS" $detail

        Add-Result "L4 日志" "$($n.Label) ERROR 数" $(if ($stats.ErrorCount -eq 0) { "PASS" } elseif ($stats.ErrorCount -le 3) { "WARN" } else { "FAIL" }) ("末 200 行内 {0} 条" -f $stats.ErrorCount)

        if ($stats.RefillFail -gt 0) {
            Add-Result "L4 日志" "$($n.Label) 连接补位" "WARN" ("末 200 行内 {0} 次补连接失败（Redis 抖动？）" -f $stats.RefillFail)
        }

        if ($stats.TimeoutHits -gt 0) {
            Add-Result "L4 日志" "$($n.Label) 连接池超时" "WARN" ("末 200 行内 {0} 次" -f $stats.TimeoutHits)
        }

        # 正常情况只应在启动/补位时出现少量“认证成功”；
        # 只有近 10 分钟内就超过一次启动建池量（5 条）才算抖动。
        if ($stats.AuthOkRecent -ge 10) {
            Add-Result "L4 日志" "$($n.Label) 连接抖动" "WARN" ("近 10 分钟 {0} 次 '认证成功'（可能反复重建连接）" -f $stats.AuthOkRecent)
        }
    }
}

# ---------- 汇总 ----------------------------------------------------------

$fail = @($script:results | Where-Object { $_.Status -eq "FAIL" }).Count
$warn = @($script:results | Where-Object { $_.Status -eq "WARN" }).Count
$skip = @($script:results | Where-Object { $_.Status -eq "SKIP" }).Count
$pass = @($script:results | Where-Object { $_.Status -eq "PASS" }).Count

if ($Json) {
    $script:results | ConvertTo-Json -Depth 4
} else {
    Write-Output ""
    Write-Output ("仓库: {0}" -f $Root)
    Write-Output ("时间: {0}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"))
    Write-Output ""
    $script:results |
        Select-Object Area, Check, Status, Detail |
        Format-Table -AutoSize -Wrap

    Write-Output ("汇总: PASS={0}  WARN={1}  FAIL={2}  SKIP={3}" -f $pass, $warn, $fail, $skip)
    if ($fail -gt 0) {
        Write-Output "结论: 存在 FAIL，请按上表逐项修复。"
    } elseif ($warn -gt 0) {
        Write-Output "结论: 无 FAIL，存在 WARN（通常是服务未启动或日志有告警）。"
    } else {
        Write-Output "结论: 全部通过。"
    }
}

if ($fail -gt 0) { exit 1 } else { exit 0 }
