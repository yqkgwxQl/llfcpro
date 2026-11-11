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

# ---------- 汇总 -----------------------------------------------------------

Write-Host ("仓库: {0}" -f $Root)
Write-Host ""
$script:results | Format-Table -AutoSize -Wrap

$fail = @($script:results | Where-Object { $_.Status -eq "FAIL" }).Count
$warn = @($script:results | Where-Object { $_.Status -eq "WARN" }).Count
$pass = @($script:results | Where-Object { $_.Status -eq "PASS" }).Count
Write-Host ("汇总: PASS={0}  WARN={1}  FAIL={2}" -f $pass, $warn, $fail)

if ($fail -gt 0) { exit 1 } else { exit 0 }
