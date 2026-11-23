#requires -Version 5.1
<#
.SYNOPSIS
    文档同步检查：确认 README / AGENTS / docs 之间的链接、路径、端口、脚本引用一致。

.DESCRIPTION
    检查项：
      D1 文档文件名：docs 下的 md 应使用中文名（允许专有名词前缀）
      D2 相对链接：README / AGENTS / docs/*.md 里的 [文本](相对路径) 目标存在
      D3 目录地图：AGENTS 表格里的反引号路径存在
      D4 脚本引用：文档里提到的 tools/test/*.ps1 都存在，且每个脚本都被文档提到
      D5 端口一致：节点配置里的 TCP/gRPC 端口在 README / 测试指南里都有出现
      D6 文档清单：docs/文档同步清单.md 存在
      D7 公共库唯一来源：数据层 + const.h 共 12 个文件只应存在于 server/common；
                    三个服务目录里不允许再出现同名副本，且必须链接 llfc_common
      D8 proto 单一来源：message.proto 只在 server/proto/ 一份，生成物只在 generated/ 一套；
      D9 README 目录树：cmake/、server/common/、server/proto/ 必须出现在 README §5
                    四个服务目录里不允许出现 message.proto / message.pb.* 副本

    只读检查，不修改任何文件。

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\doc-sync.ps1
#>
[CmdletBinding()]
param(
    [string]$Root = ""
)

$ErrorActionPreference = "Continue"

if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
}
$Root = (Resolve-Path -LiteralPath $Root).Path

$script:results = New-Object System.Collections.ArrayList

function Add-Result {
    param([string]$Item, [string]$Status, [string]$Detail = "")
    [void]$script:results.Add([pscustomobject]@{ Item = $Item; Status = $Status; Detail = $Detail })
}

$readme = Join-Path $Root "README.md"
$agents = Join-Path $Root "AGENTS.md"
$docsDir = Join-Path $Root "docs"

# ---------- D1 文档命名 ----------------------------------------------------

$mdFiles = @(Get-ChildItem -LiteralPath $docsDir -Filter *.md -File -ErrorAction SilentlyContinue)
foreach ($f in $mdFiles) {
    if ($f.Name -match '[\u4e00-\u9fa5]') {
        Add-Result ("D1 文档命名 " + $f.Name) "PASS" "含中文"
    } else {
        Add-Result ("D1 文档命名 " + $f.Name) "WARN" "纯英文名，建议中文命名"
    }
}

# ---------- D2 相对链接 ----------------------------------------------------

$mdAll = @()
if (Test-Path $readme) { $mdAll += Get-Item $readme }
if (Test-Path $agents) { $mdAll += Get-Item $agents }
$mdAll += $mdFiles

$linkBad = 0
foreach ($md in $mdAll) {
    $text = Get-Content -LiteralPath $md.FullName -Raw -Encoding UTF8
    foreach ($m in [regex]::Matches($text, '\]\(([^)]+)\)')) {
        $target = $m.Groups[1].Value.Trim()
        if ($target -match '^(https?:|mailto:|#)') { continue }
        $target = ($target -split '#')[0]
        if ([string]::IsNullOrWhiteSpace($target)) { continue }
        $resolved = Join-Path $md.DirectoryName $target
        if (-not (Test-Path -LiteralPath $resolved)) {
            $linkBad++
            Add-Result ("D2 链接 " + $md.Name) "FAIL" ("失效: " + $target)
        }
    }
}
if ($linkBad -eq 0) { Add-Result "D2 相对链接" "PASS" ("检查 {0} 份文档" -f $mdAll.Count) }

# ---------- D3 AGENTS 目录地图 --------------------------------------------

if (Test-Path $agents) {
    $bad = 0
    foreach ($line in Get-Content -LiteralPath $agents -Encoding UTF8) {
        if ($line -notmatch '^\|\s*`([^`]+)`') { continue }
        $p = $Matches[1]
        if ($p -match '^<') { continue }          # 形如 <模块>/build/ 的占位
        # 只检查“像路径”的条目：含目录分隔符，或以 .md/.ps1/.sql 结尾
        if ($p -notmatch '[/\\]' -and $p -notmatch '\.(md|ps1|sql)$') { continue }
        $path = Join-Path $Root $p
        if (-not (Test-Path -LiteralPath $path)) {
            $bad++
            Add-Result ("D3 目录地图 " + $p) "WARN" "路径不存在（可能是约定名）"
        }
    }
    if ($bad -eq 0) { Add-Result "D3 目录地图" "PASS" "AGENTS 表格路径均存在" }
}

# ---------- D4 脚本引用 ----------------------------------------------------

$scriptFiles = @(Get-ChildItem -LiteralPath (Join-Path $Root "tools/test") -Filter *.ps1 -File -ErrorAction SilentlyContinue)
$docText = ($mdAll | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8 }) -join "`n"

foreach ($s in $scriptFiles) {
    if ($docText -match [regex]::Escape($s.Name)) {
        Add-Result ("D4 脚本被文档引用 " + $s.Name) "PASS" ""
    } else {
        Add-Result ("D4 脚本被文档引用 " + $s.Name) "WARN" "没有任何文档提到它"
    }
}

foreach ($m in [regex]::Matches($docText, 'tools[/\\]test[/\\][A-Za-z0-9_\-]+\.ps1')) {
    $rel = $m.Value -replace '/', '\'
    if (-not (Test-Path -LiteralPath (Join-Path $Root $rel))) {
        Add-Result ("D4 文档引用的脚本 " + $rel) "FAIL" "文件不存在"
    }
}

# ---------- D5 端口一致 ----------------------------------------------------

$nodeConfigs = @(Get-ChildItem -LiteralPath (Join-Path $Root "server/ChatServer/nodes") -Recurse -Filter config.ini -ErrorAction SilentlyContinue)
$ports = @()
foreach ($cfg in $nodeConfigs) {
    $section = $null
    foreach ($raw in Get-Content -LiteralPath $cfg.FullName -Encoding UTF8) {
        $line = $raw.Trim()
        if ($line -match '^\[(.+)\]$') { $section = $Matches[1].Trim(); continue }
        if ($section -eq "SelfServer" -and $line -match '^\s*(Port|RPCPort)\s*=\s*(\d+)') { $ports += [int]$Matches[2] }
    }
}
$ports = @($ports | Sort-Object -Unique)

$readmeText = if (Test-Path $readme) { Get-Content -LiteralPath $readme -Raw -Encoding UTF8 } else { "" }
$guideText  = if (Test-Path (Join-Path $docsDir "测试指南.md")) { Get-Content -LiteralPath (Join-Path $docsDir "测试指南.md") -Raw -Encoding UTF8 } else { "" }

foreach ($p in $ports) {
    $inReadme = $readmeText -match ("\b" + $p + "\b")
    $inGuide  = $guideText  -match ("\b" + $p + "\b")
    if ($inReadme -or $inGuide) {
        Add-Result ("D5 端口 " + $p) "PASS" ("README={0} 指南={1}" -f $inReadme, $inGuide)
    } else {
        Add-Result ("D5 端口 " + $p) "WARN" "节点配置使用该端口，但 README/测试指南都没提到"
    }
}

# ---------- D6 清单存在 ----------------------------------------------------

$checklist = Join-Path $docsDir "文档同步清单.md"
if (Test-Path -LiteralPath $checklist) {
    Add-Result "D6 文档同步清单" "PASS" "docs/文档同步清单.md"
} else {
    Add-Result "D6 文档同步清单" "FAIL" "缺少 docs/文档同步清单.md"
}

# ---------- D7 公共库唯一来源（server/common） -------------------------------
# 2026-11-22 起，数据层与公共常量统一放在 server/common：
#   · 头文件 → server/common/include
#   · 实现   → server/common/src
# 三个服务目录里不允许再出现同名副本；且每个服务都必须链接 llfc_common。

$commonLayout = [ordered]@{
    "MysqlDao.cc"     = "src"
    "MysqlDao.h"      = "include"
    "MysqlMgr.cc"     = "src"
    "MysqlMgr.h"      = "include"
    "MysqlPool.cc"    = "src"
    "MysqlPool.h"     = "include"
    "RedisConPool.cc" = "src"
    "RedisConPool.h"  = "include"
    "RedisMgr.cc"     = "src"
    "RedisMgr.h"      = "include"
    "data.h"          = "include"
    "const.h"         = "include"
}
$commonDir = Join-Path (Join-Path $Root "server") "common"

$missing = @()
foreach ($name in $commonLayout.Keys) {
    $fp = Join-Path (Join-Path $commonDir $commonLayout[$name]) $name
    if (-not (Test-Path -LiteralPath $fp)) { $missing += $name }
}
if ($missing.Count -gt 0) {
    Add-Result "D7 公共库 server/common" "FAIL" ("缺少: " + ($missing -join ", "))
} else {
    Add-Result "D7 公共库 server/common" "PASS" ("{0} 个公共文件都在（include/ 与 src/）" -f $commonLayout.Count)
}

foreach ($svc in @("ChatServer", "GateServer", "StatusServer")) {
    $svcDir = Join-Path (Join-Path $Root "server") $svc

    $copies = @()
    foreach ($name in $commonLayout.Keys) {
        if (Test-Path -LiteralPath (Join-Path $svcDir $name)) { $copies += $name }
    }
    if ($copies.Count -gt 0) {
        Add-Result ("D7 " + $svc + " 禁止副本") "FAIL" ("已迁到 server/common，服务目录里不应再有: " + ($copies -join ", "))
    } else {
        Add-Result ("D7 " + $svc + " 禁止副本") "PASS" "服务目录下无数据层副本"
    }

    $cm = Join-Path $svcDir "CMakeLists.txt"
    if ((Test-Path -LiteralPath $cm) -and ((Get-Content -LiteralPath $cm -Raw) -match 'llfc_common')) {
        Add-Result ("D7 " + $svc + " 链接公共库") "PASS" "链接了 llfc_common"
    } else {
        Add-Result ("D7 " + $svc + " 链接公共库") "FAIL" "CMakeLists.txt 里没有 llfc_common"
    }
}

# ---------- D8 proto 单一来源（server/proto） --------------------------------
# 2026-11-22 起：协议定义只有 server/proto/message.proto 一份；
# 生成物只有 server/proto/generated/ 一套（入库，构建不需要装 protoc）；
# 四个服务目录里不允许再有 message.proto / message.pb.* 副本。

$protoRoot = Join-Path (Join-Path $Root "server") "proto"
$protoFile = Join-Path $protoRoot "message.proto"
$genDir    = Join-Path $protoRoot "generated"
$genFiles  = @("message.pb.cc", "message.pb.h", "message.grpc.pb.cc", "message.grpc.pb.h")

if (Test-Path -LiteralPath $protoFile) {
    Add-Result "D8 proto 真源" "PASS" "server/proto/message.proto"
} else {
    Add-Result "D8 proto 真源" "FAIL" "缺少 server/proto/message.proto"
}

$genMissing = @()
foreach ($gf in $genFiles) { if (-not (Test-Path -LiteralPath (Join-Path $genDir $gf))) { $genMissing += $gf } }
if ($genMissing.Count -gt 0) {
    Add-Result "D8 生成物" "FAIL" ("缺少: " + ($genMissing -join ", ") + "（跑 regen_proto）")
} else {
    Add-Result "D8 生成物" "PASS" "server/proto/generated/ 下 4 个文件齐全"
}

foreach ($svc in @("ChatServer", "GateServer", "StatusServer", "VarifyServer")) {
    $svcDir = Join-Path (Join-Path $Root "server") $svc
    $copies = @()
    if (Test-Path -LiteralPath (Join-Path $svcDir "message.proto")) { $copies += "message.proto" }
    foreach ($gf in $genFiles) { if (Test-Path -LiteralPath (Join-Path $svcDir $gf)) { $copies += $gf } }
    if ($copies.Count -gt 0) {
        Add-Result ("D8 " + $svc + " 禁止副本") "FAIL" ("已迁到 server/proto，服务目录里不应再有: " + ($copies -join ", "))
    } else {
        Add-Result ("D8 " + $svc + " 禁止副本") "PASS" "服务目录下无 proto/pb 副本"
    }
}

# ---------- D9 README 目录树包含「跨模块目录」 ------------------------------
# 2026-11-23 起仓库有 cmake/、server/common/、server/proto/ 这三个跨模块目录。
# 经验：抽 common / proto 时只更新了 AGENTS 的目录地图，README §5 的树忘了改，
#       而 D1~D8 都不检查 README 正文，所以一直没被发现 —— 这里补上。

$readmePath = Join-Path $Root "README.md"
if (Test-Path -LiteralPath $readmePath) {
    $readmeText = Get-Content -LiteralPath $readmePath -Raw
    $tree = $readmeText
    if ($readmeText -match '(?ms)^## 5\. 目录结构(.*?)^## ') { $tree = $Matches[1] }

    $tokens = @("cmake/", "common/", "proto/")
    $missDoc = @()
    foreach ($tk in $tokens) {
        if ($tree -notmatch [regex]::Escape($tk)) { $missDoc += $tk }
    }
    if ($missDoc.Count -gt 0) {
        Add-Result "D9 README 目录树" "FAIL" ("README §5 缺少: " + ($missDoc -join ", ") + "（新增/移动顶层目录后要同步目录树）")
    } else {
        Add-Result "D9 README 目录树" "PASS" "cmake/、server/common/、server/proto/ 都已列出"
    }

    $dirs = @("cmake", "server/common", "server/proto")
    $missDisk = @()
    foreach ($d in $dirs) {
        if (-not (Test-Path -LiteralPath (Join-Path $Root $d))) { $missDisk += $d }
    }
    if ($missDisk.Count -gt 0) {
        Add-Result "D9 跨模块目录存在性" "FAIL" ("磁盘上缺少: " + ($missDisk -join ", "))
    } else {
        Add-Result "D9 跨模块目录存在性" "PASS" "cmake / server/common / server/proto 都在"
    }
} else {
    Add-Result "D9 README 目录树" "FAIL" "缺少 README.md"
}

# ---------- 汇总 -----------------------------------------------------------

Write-Host ("仓库: {0}" -f $Root)
Write-Host ""
$script:results | Format-Table -AutoSize -Wrap

$fail = @($script:results | Where-Object { $_.Status -eq "FAIL" }).Count
$warn = @($script:results | Where-Object { $_.Status -eq "WARN" }).Count
$pass = @($script:results | Where-Object { $_.Status -eq "PASS" }).Count
Write-Host ("汇总: PASS={0}  WARN={1}  FAIL={2}" -f $pass, $warn, $fail)

if ($fail -gt 0) { exit 1 } else { exit 0 }
