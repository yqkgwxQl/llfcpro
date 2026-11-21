<#
.SYNOPSIS
    消息链路协议级回归测试（TCP 二进制帧 + MySQL 断言）。

.DESCRIPTION
    覆盖「消息落库 / 离线投递 / 接收方 ACK / 分页游标 / 授权」这条链路。
    与 api-test.ps1 的分工：
      - api-test.ps1   走 HTTP，测注册/登录这类网关接口；
      - msg-proto.ps1  走 TCP，直接说 ChatServer 的二进制协议，测消息状态机。

    断言源只用两个：**协议响应** + **MySQL 里的 message 表**。
    不断言日志文本（措辞一改就会误报）。

    数据隔离：本脚本产生的消息内容一律以 `__MSGP_` 开头，结束时按前缀删除，
    并对比 message 表行数基线，保证「只删自己造的」。

.PARAMETER AllowWrite
    真正执行（会发消息、会写 message 表）。不加时只打印计划（dry-run）。

.PARAMETER Case
    要跑的用例组：all / send / pull / ack / authz。可多选。

.PARAMETER KeepData
    保留测试数据（排查用）。默认结束时删除。

.PARAMETER UidA
    发送方 uid。默认自动取 user 表里最大的两个 uid 中的较大者。

.PARAMETER UidB
    接收方 uid。默认取较小者。

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\msg-proto.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\msg-proto.ps1 -AllowWrite
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\test\msg-proto.ps1 -AllowWrite -Case ack,pull

.NOTES
    前置：MySQL / Redis / ChatServer 就绪（可先跑 stack-ctl.ps1 -Action Start）。
    退出码：0 = 无 FAIL；1 = 有 FAIL。
#>
[CmdletBinding()]
param(
    [switch]$AllowWrite,
    [ValidateSet('all','send','pull','ack','authz')]
    [string[]]$Case = @('all'),
    [switch]$KeepData,
    [int]$UidA = 0,
    [int]$UidB = 0,
    [int]$TimeoutMs = 8000
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------- 路径 / 配置
$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$NodeIni  = Join-Path $RepoRoot 'server\ChatServer\nodes\chatserver1\config.ini'
$GateIni  = Join-Path $RepoRoot 'server\GateServer\config.ini'
$MysqlExe = 'F:\YingYongRuanJian3\mysql\mysql-8.4.7-winx64\bin\mysql.exe'
$RedisCli = 'F:\YingYongRuanJian3\redis\redis-cli.exe'
$ContentPrefix = '__MSGP_'

function Get-IniValue {
    param([string]$Path, [string]$Section, [string]$Key)
    if(-not (Test-Path $Path)){ return $null }
    $current = $null
    foreach($line in Get-Content $Path){
        if($line -match '^\s*\[(.+?)\]'){ $current = $Matches[1]; continue }
        if($current -eq $Section -and $line -match "^\s*$([regex]::Escape($Key))\s*=\s*(.+?)\s*$"){
            return $Matches[1]
        }
    }
    return $null
}

if(-not (Test-Path $NodeIni)){ throw "找不到节点配置: $NodeIni" }

$ChatHost = Get-IniValue $NodeIni 'SelfServer' 'Host'
$ChatPort = [int](Get-IniValue $NodeIni 'SelfServer' 'Port')
$DbHost   = Get-IniValue $NodeIni 'Mysql' 'Host'
$DbPort   = Get-IniValue $NodeIni 'Mysql' 'Port'
$DbUser   = Get-IniValue $NodeIni 'Mysql' 'User'
$DbPass   = Get-IniValue $NodeIni 'Mysql' 'Passwd'
$DbSchema = Get-IniValue $NodeIni 'Mysql' 'Schema'
$RdHost   = Get-IniValue $NodeIni 'Redis' 'Host'
$RdPort   = Get-IniValue $NodeIni 'Redis' 'Port'
$RdPass   = Get-IniValue $NodeIni 'Redis' 'Passwd'
$GatePort = [int](Get-IniValue $GateIni 'GateServer' 'Port')
$GateUrl  = "http://127.0.0.1:$GatePort"

if($ChatHost -eq '0.0.0.0'){ $ChatHost = '127.0.0.1' }

foreach($exe in @($MysqlExe, $RedisCli)){
    if(-not (Test-Path $exe)){ throw "找不到工具: $exe" }
}

# 密码只走环境变量，不出现在命令行 / 输出里
$env:MYSQL_PWD    = $DbPass
$env:REDISCLI_AUTH = $RdPass

# ---------------------------------------------------------------- 基础工具
function Invoke-Sql {
    param([string]$Query)
    $out = & $MysqlExe -h $DbHost -P $DbPort -u $DbUser -N -B -e $Query 2>&1
    return (($out | Out-String).Trim())
}

function Get-RedisValue {
    param([string]$Key)
    $out = & $RedisCli -h $RdHost -p $RdPort --no-auth-warning GET $Key 2>&1
    return (($out | Out-String).Trim())
}

$script:Checks = @()
function Add-Check {
    param([string]$Id, [string]$Name, [bool]$Ok, [string]$Detail = '', [switch]$Known)
    $script:Checks += [pscustomobject]@{ Id = $Id; Name = $Name; Ok = $Ok; Detail = $Detail; Known = [bool]$Known }
    $tag = if($Ok){ 'PASS' } elseif($Known){ 'KNOWN' } else { 'FAIL' }
    $color = if($Ok){ 'Green' } elseif($Known){ 'Yellow' } else { 'Red' }
    Write-Host ("  [{0}] {1,-22} {2}" -f $tag, $Id, $Name) -ForegroundColor $color
    if($Detail){ Write-Host ("        {0}" -f $Detail) -ForegroundColor DarkGray }
}

# ---------------------------------------------------------------- 协议帧
# 帧格式（与 server/ChatServer/MsgNode.cc 一致）：
#   [2 字节 msg_id，网络序][2 字节 body 长度，网络序][body = UTF-8 JSON]
function Send-Frame {
    param($Stream, [int]$MsgId, [string]$Json)
    $payload = [Text.Encoding]::UTF8.GetBytes($Json)
    $header = [byte[]]@(
        [byte](($MsgId -shr 8) -band 0xFF),
        [byte]($MsgId -band 0xFF),
        [byte](($payload.Length -shr 8) -band 0xFF),
        [byte]($payload.Length -band 0xFF)
    )
    $Stream.Write($header, 0, 4)
    $Stream.Write($payload, 0, $payload.Length)
    $Stream.Flush()
}

function Read-Frame {
    param($Stream)
    try{
        $hdr = New-Object byte[] 4
        $got = 0
        while($got -lt 4){
            $n = $Stream.Read($hdr, $got, 4 - $got)
            if($n -le 0){ return $null }
            $got += $n
        }
        $msgId = ([int]$hdr[0] * 256) + [int]$hdr[1]
        $len   = ([int]$hdr[2] * 256) + [int]$hdr[3]
        $buf = New-Object byte[] $len
        $got = 0
        while($got -lt $len){
            $n = $Stream.Read($buf, $got, $len - $got)
            if($n -le 0){ break }
            $got += $n
        }
        return [pscustomobject]@{ MsgId = $msgId; Body = [Text.Encoding]::UTF8.GetString($buf) }
    }catch{
        return $null
    }
}

function Open-ChatSession {
    param([int]$Uid, [string]$Token)
    $client = New-Object System.Net.Sockets.TcpClient
    $client.Connect($ChatHost, $ChatPort)
    $client.ReceiveTimeout = $TimeoutMs
    $stream = $client.GetStream()
    Send-Frame -Stream $stream -MsgId 1005 -Json ("{{`"uid`":{0},`"token`":`"{1}`"}}" -f $Uid, $Token)
    $rsp = Read-Frame $stream
    if($null -eq $rsp){ $client.Close(); throw "登录超时（uid=$Uid）" }
    $obj = $rsp.Body | ConvertFrom-Json
    if($obj.error -ne 0){ $client.Close(); throw "登录失败（uid=$Uid, error=$($obj.error)）" }
    return [pscustomobject]@{ Client = $client; Stream = $stream; Uid = $Uid }
}

function Close-ChatSession {
    param($Session)
    if($null -eq $Session){ return }
    try{ $Session.Client.Close() }catch{ }
}

function Get-Token {
    param([int]$Uid, [string]$Email, [string]$Pwd)
    # 优先走网关登录（顺带覆盖 /user_login），失败再退回 Redis 里已有的 token
    try{
        $body = @{ email = $Email; passwd = $Pwd } | ConvertTo-Json -Compress
        $rsp = Invoke-RestMethod -Uri "$GateUrl/user_login" -Method Post -Body $body -ContentType 'application/json' -TimeoutSec 5
        if($rsp.error -eq 0 -and $rsp.token){ return $rsp.token }
    }catch{ }
    return (Get-RedisValue "utoken_$Uid")
}

# ---------------------------------------------------------------- 业务封装
function Send-ChatMsg {
    param($Session, [int]$ToUid, [string]$UniqueId, [string]$Content, [string]$ChatTime, [int]$ForgedFromUid = 0)
    $fromPart = if($ForgedFromUid -gt 0){ "`"from_uid`":$ForgedFromUid," } else { '' }
    $json = '{' + $fromPart + '"to_uid":' + $ToUid + ',"text_array":[{"unique_id":"' + $UniqueId + '","content":"' + $Content + '","chat_time":"' + $ChatTime + '"}]}'
    Send-Frame -Stream $Session.Stream -MsgId 1017 -Json $json
    $rsp = Read-Frame $Session.Stream
    if($null -eq $rsp){ return $null }
    return ($rsp.Body | ConvertFrom-Json)
}

function Pull-OfflineMsg {
    param($Session, [int]$OtherUid, [long]$AfterId = 0, [int]$Limit = 10)
    $json = '{"other_uid":' + $OtherUid + ',"after_id":' + $AfterId + ',"limit":' + $Limit + '}'
    Send-Frame -Stream $Session.Stream -MsgId 1044 -Json $json
    $rsp = Read-Frame $Session.Stream
    if($null -eq $rsp){ return $null }
    return [pscustomobject]@{ MsgId = $rsp.MsgId; Body = ($rsp.Body | ConvertFrom-Json) }
}

function Confirm-OfflineMsg {
    param($Session, [int]$OtherUid, [string[]]$UniqueIds)
    $arr = ($UniqueIds | ForEach-Object { '"' + $_ + '"' }) -join ','
    $json = '{"other_uid":' + $OtherUid + ',"unique_ids":[' + $arr + ']}'
    Send-Frame -Stream $Session.Stream -MsgId 1046 -Json $json
    $rsp = Read-Frame $Session.Stream
    if($null -eq $rsp){ return $null }
    return [pscustomobject]@{ MsgId = $rsp.MsgId; Body = ($rsp.Body | ConvertFrom-Json) }
}

function Confirm-TextMsg {
    param($Session, [int]$OtherUid, [string[]]$UniqueIds)
    $arr = ($UniqueIds | ForEach-Object { '"' + $_ + '"' }) -join ','
    $json = '{"other_uid":' + $OtherUid + ',"unique_ids":[' + $arr + ']}'
    Send-Frame -Stream $Session.Stream -MsgId 1020 -Json $json
    $rsp = Read-Frame $Session.Stream
    if($null -eq $rsp){ return $null }
    return [pscustomobject]@{ MsgId = $rsp.MsgId; Body = ($rsp.Body | ConvertFrom-Json) }
}

function Get-Delivered {
    param([string]$UniqueId)
    $v = Invoke-Sql ("select delivered from {0}.message where unique_id='{1}'" -f $DbSchema, $UniqueId)
    if([string]::IsNullOrWhiteSpace($v)){ return -1 }
    return [int]($v -split "`n")[0]
}

function Get-MsgRowCount {
    param([string]$UniqueId)
    $v = Invoke-Sql ("select count(*) from {0}.message where unique_id='{1}'" -f $DbSchema, $UniqueId)
    return [int]($v -split "`n")[0]
}

function New-UniqueId { return [guid]::NewGuid().ToString() }

# 服务端在没有数据时不会输出 chat_msg_list 字段；
# 直接 @($null) 会得到「1 个元素」——这是 PowerShell 的经典坑，必须过滤掉 $null。
function Get-MsgList {
    param($Body)
    return @($Body.chat_msg_list | Where-Object { $null -ne $_ })
}

# 把指定会话里所有未投递消息拉完并 ACK（用于把状态归零）
function Clear-Pending {
    param($Session, [int]$OtherUid)
    $total = 0
    for($i = 0; $i -lt 20; $i++){
        $r = Pull-OfflineMsg -Session $Session -OtherUid $OtherUid -AfterId 0 -Limit 10
        if($null -eq $r -or $r.Body.error -ne 0){ break }
        $list = @(Get-MsgList $r.Body)
        if($list.Count -eq 0){ break }
        $ids = @(); foreach($m in $list){ $ids += $m.unique_id }
        $null = Confirm-OfflineMsg -Session $Session -OtherUid $OtherUid -UniqueIds $ids
        $total += $list.Count
        if(-not $r.Body.has_more){ break }
    }
    return $total
}

# ---------------------------------------------------------------- 用例开关
$RunAll = $Case -contains 'all'
function Want([string]$Group){ return ($RunAll -or ($Case -contains $Group)) }

function Invoke-Case {
    param([string]$Group, [string]$Name, [scriptblock]$Body)
    if(-not (Want $Group)){ return }
    Write-Host ""
    Write-Host ("### {0}  [{1}]" -f $Name, $Group) -ForegroundColor Cyan
    try{
        & $Body
    }catch{
        Add-Check -Id 'EXC' -Name $Name -Ok $false -Detail $_.Exception.Message
    }
}

# ---------------------------------------------------------------- 前置检查
Write-Host "仓库: $RepoRoot"
Write-Host ("ChatServer : {0}:{1}" -f $ChatHost, $ChatPort)
Write-Host ("MySQL      : {0}:{1}/{2}" -f $DbHost, $DbPort, $DbSchema)
Write-Host ("GateServer : {0}" -f $GateUrl)

$portOk = $false
try{
    $c = New-Object System.Net.Sockets.TcpClient
    $c.Connect($ChatHost, $ChatPort); $portOk = $c.Connected; $c.Close()
}catch{ }
if(-not $portOk){
    Write-Host "ChatServer 未监听 $ChatPort，请先启动（stack-ctl.ps1 -Action Start）" -ForegroundColor Red
    exit 1
}

# 选账号：默认取 user 表里的两个 uid（较大者=A 发送方，较小者=B 接收方）
if($UidA -le 0 -or $UidB -le 0){
    $rows = Invoke-Sql ("select uid, ifnull(email,''), ifnull(pwd,'') from {0}.user order by uid" -f $DbSchema)
    $users = @()
    foreach($line in ($rows -split "`n")){
        if([string]::IsNullOrWhiteSpace($line)){ continue }
        $f = $line -split "`t"
        if($f.Count -ge 3){ $users += [pscustomobject]@{ Uid = [int]$f[0]; Email = $f[1]; Pwd = $f[2] } }
    }
    if($users.Count -lt 2){ Write-Host "user 表里少于 2 个账号，无法配对测试" -ForegroundColor Red; exit 1 }
    if($UidB -le 0){ $UidB = $users[0].Uid }
    if($UidA -le 0){ $UidA = $users[$users.Count - 1].Uid }
}
$userA = @($users | Where-Object { $_.Uid -eq $UidA })[0]
$userB = @($users | Where-Object { $_.Uid -eq $UidB })[0]
if($null -eq $userA -or $null -eq $userB){ Write-Host "找不到 uid $UidA / $UidB 对应的账号" -ForegroundColor Red; exit 1 }

Write-Host ("账号       : A={0}(uid={1})  B={2}(uid={3})" -f $userA.Email, $UidA, $userB.Email, $UidB)

$tokenA = Get-Token -Uid $UidA -Email $userA.Email -Pwd $userA.Pwd
$tokenB = Get-Token -Uid $UidB -Email $userB.Email -Pwd $userB.Pwd
if([string]::IsNullOrWhiteSpace($tokenA) -or [string]::IsNullOrWhiteSpace($tokenB)){
    Write-Host "拿不到 token（网关登录失败且 Redis 无 utoken_<uid>）" -ForegroundColor Red
    exit 1
}

if(-not $AllowWrite){
    Write-Host ""
    Write-Host "== DRY-RUN（未加 -AllowWrite，不会发送任何消息）==" -ForegroundColor Yellow
    Write-Host "将执行："
    if(Want 'send') { Write-Host "  send  : M1 在线投递 / M2 离线投递 / M3 非好友拒发 / M4 同 id 幂等 / M5 from_uid 伪造被忽略" }
    if(Want 'pull') { Write-Host "  pull  : M6 空拉取 / M7 单条 / M8 恰好 10 条 / M9 11 条分两页 / M10 after_id 游标" }
    if(Want 'ack')  { Write-Host "  ack   : M12 ACK 生效 / M13 重复 ACK 幂等 / M15 非法参数 / M16 ACK 后不再重拉" }
    if(Want 'authz'){ Write-Host "  authz : M14 越权 ACK / M17 好友列表忽略报文 uid" }
    Write-Host "  结束时按内容前缀 '$ContentPrefix' 清理，并与 message 行数基线对账"
    Write-Host "exited (dry-run)"
    exit 0
}

# ---------------------------------------------------------------- 基线
$baselineMsgs = [int]((Invoke-Sql ("select count(*) from {0}.message" -f $DbSchema)) -split "`n")[0]
$stamp = (Get-Date).ToString('yyyyMMdd_HHmmss')
$tagPrefix = "$ContentPrefix$stamp"
Write-Host ("基线       : message 行数={0}；本次测试前缀={1}" -f $baselineMsgs, $tagPrefix)

$sessionA = $null
$sessionB = $null

try{
    $sessionA = Open-ChatSession -Uid $UidA -Token $tokenA
    Write-Host "A 已登录"

    # ============================================================ send
    Invoke-Case 'send' 'M1 对方在线：1018 online=true → 1019 → 1020 → delivered=1' {
        $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB
        $uid1 = New-UniqueId
        $rsp = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $uid1 -Content "${tagPrefix}/M1" -ChatTime '2026-01-01 00:00:01'
        Add-Check 'M1.1' '发送返回 online=true' ($null -ne $rsp -and $rsp.online -eq $true) "online=$($rsp.online)"

        $notify = Read-Frame $script:sessionB.Stream
        $okNotify = $false; $gotUid = ''
        if($null -ne $notify){
            $n = $notify.Body | ConvertFrom-Json
            $gotUid = @($n.text_array)[0].unique_id
            $okNotify = ($notify.MsgId -eq 1019) -and ($gotUid -eq $uid1)
        }
        Add-Check 'M1.2' 'B 收到 1019 且含该 unique_id' $okNotify "msgId=$($notify.MsgId) uid=$gotUid"

        $ack = Confirm-TextMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds @($uid1)
        $okAck = ($null -ne $ack) -and ($ack.MsgId -eq 1021) -and ($ack.Body.error -eq 0)
        Add-Check 'M1.3' '1020 回包为 1021 且 error=0' $okAck "msgId=$($ack.MsgId) error=$($ack.Body.error)"
        Add-Check 'M1.4' 'ACK 后 delivered=1' ((Get-Delivered $uid1) -eq 1) "delivered=$(Get-Delivered $uid1)"
    }

    Invoke-Case 'send' 'M2 对方离线：1018 online=false → delivered=0' {
        Close-ChatSession $script:sessionB; $script:sessionB = $null
        Start-Sleep -Milliseconds 500
        $uid2 = New-UniqueId
        $rsp = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $uid2 -Content "${tagPrefix}/M2" -ChatTime '2026-01-01 00:00:02'
        Add-Check 'M2.1' '发送返回 online=false' ($null -ne $rsp -and $rsp.online -eq $false) "online=$($rsp.online)"
        Add-Check 'M2.2' '落库 delivered=0' ((Get-Delivered $uid2) -eq 0) "delivered=$(Get-Delivered $uid2)"
    }

    Invoke-Case 'send' 'M3 非好友：拒发且不落库' {
        $uid3 = New-UniqueId
        $rsp = Send-ChatMsg -Session $sessionA -ToUid 999999 -UniqueId $uid3 -Content "${tagPrefix}/M3" -ChatTime '2026-01-01 00:00:03'
        # 2026-11-22：BL-27 已修复（Defer 提前、错误包带 from_uid/to_uid），去掉 -Known 转为硬断言。
        Add-Check 'M3.1' '返回 error≠0（必须有回包）' ($null -ne $rsp -and $rsp.error -ne 0) $(if($null -eq $rsp){ '服务端未回包（超时）' } else { "error=$($rsp.error)" })
        Add-Check 'M3.2' '库里没有该行' ((Get-MsgRowCount $uid3) -eq 0) "rows=$(Get-MsgRowCount $uid3)"
    }

    Invoke-Case 'send' 'M4 同一 unique_id 发两次：库里只有 1 行' {
        $uid4 = New-UniqueId
        $null = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $uid4 -Content "${tagPrefix}/M4" -ChatTime '2026-01-01 00:00:04'
        $null = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $uid4 -Content "${tagPrefix}/M4" -ChatTime '2026-01-01 00:00:04'
        Add-Check 'M4.1' 'INSERT IGNORE 幂等（1 行）' ((Get-MsgRowCount $uid4) -eq 1) "rows=$(Get-MsgRowCount $uid4)"
    }

    Invoke-Case 'send' 'M5 报文伪造 from_uid：落库仍用 session uid' {
        $uid5 = New-UniqueId
        $null = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $uid5 -Content "${tagPrefix}/M5" -ChatTime '2026-01-01 00:00:05' -ForgedFromUid 999999
        $from = Invoke-Sql ("select from_uid from {0}.message where unique_id='{1}'" -f $DbSchema, $uid5)
        Add-Check 'M5.1' 'from_uid = A 的 uid' ([int]($from -split "`n")[0] -eq $UidA) "from_uid=$from（期望 $UidA）"
    }

    # ============================================================ pull
    Invoke-Case 'pull' 'M6 空拉取：0 条 / has_more=false / max_id=0' {
        if($null -eq $script:sessionB){ $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB }
        $null = Clear-Pending -Session $script:sessionB -OtherUid $UidA
        $r = Pull-OfflineMsg -Session $script:sessionB -OtherUid $UidA -AfterId 0 -Limit 10
        $cnt = @(Get-MsgList $r.Body).Count
        $left = ((Get-MsgList $r.Body | ForEach-Object { $_.content }) -join '|')
        Add-Check 'M6.1' '返回 0 条' ($cnt -eq 0) "count=$cnt leftover=$left"
        Add-Check 'M6.2' 'has_more=false' ($r.Body.has_more -eq $false) "has_more=$($r.Body.has_more)"
        Add-Check 'M6.3' 'max_id=0' ([long]$r.Body.max_id -eq 0) "max_id=$($r.Body.max_id)"
    }

    Invoke-Case 'pull' 'M7 单条：拉得到 → ACK → 再拉为空（M16）' {
        Close-ChatSession $script:sessionB; $script:sessionB = $null
        Start-Sleep -Milliseconds 400
        $uid7 = New-UniqueId
        $null = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $uid7 -Content "${tagPrefix}/M7" -ChatTime '2026-01-01 00:00:07'
        $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB
        $r = Pull-OfflineMsg -Session $script:sessionB -OtherUid $UidA -AfterId 0 -Limit 10
        $ids = @(Get-MsgList $r.Body | ForEach-Object { $_.unique_id })
        Add-Check 'M7.1' '拉取包含该条' ($ids -contains $uid7) "count=$($ids.Count)"
        $ack = Confirm-OfflineMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds @($uid7)
        Add-Check 'M7.2' '1046 回包 1047 且 error=0' (($null -ne $ack) -and ($ack.MsgId -eq 1047) -and ($ack.Body.error -eq 0)) "msgId=$($ack.MsgId)"
        Add-Check 'M7.3' 'delivered=1' ((Get-Delivered $uid7) -eq 1) "delivered=$(Get-Delivered $uid7)"
        $r2 = Pull-OfflineMsg -Session $script:sessionB -OtherUid $UidA -AfterId 0 -Limit 10
        $ids2 = @(Get-MsgList $r2.Body | ForEach-Object { $_.unique_id })
        Add-Check 'M16.1' 'ACK 后不再重拉' (-not ($ids2 -contains $uid7)) "count=$($ids2.Count)"
    }

    Invoke-Case 'pull' 'M8 恰好 10 条：has_more=true，下一页为空' {
        $null = Clear-Pending -Session $script:sessionB -OtherUid $UidA
        Close-ChatSession $script:sessionB; $script:sessionB = $null
        Start-Sleep -Milliseconds 400
        $ids8 = @()
        for($i = 1; $i -le 10; $i++){
            $u = New-UniqueId; $ids8 += $u
            $null = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $u -Content ("{0}/M8_{1:D2}" -f $tagPrefix, $i) -ChatTime '2026-01-01 00:00:10'
        }
        $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB
        $r = Pull-OfflineMsg -Session $script:sessionB -OtherUid $UidA -AfterId 0 -Limit 10
        $page1 = @(Get-MsgList $r.Body | ForEach-Object { $_.unique_id })
        Add-Check 'M8.1' '首页 10 条' ($page1.Count -eq 10) "count=$($page1.Count)"
        Add-Check 'M8.2' 'has_more=true' ($r.Body.has_more -eq $true) "has_more=$($r.Body.has_more)"
        $r2 = Pull-OfflineMsg -Session $script:sessionB -OtherUid $UidA -AfterId $r.Body.max_id -Limit 10
        $page2 = @(Get-MsgList $r2.Body)
        $left2 = (@($page2 | ForEach-Object { $_.content }) -join '|')
        Add-Check 'M8.3' '第二页为空' ($page2.Count -eq 0) "count=$($page2.Count) leftover=$left2"
        Add-Check 'M8.4' '第二页 has_more=false（不会死循环）' ($r2.Body.has_more -eq $false) "has_more=$($r2.Body.has_more)"
        $null = Confirm-OfflineMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds $page1
    }

    Invoke-Case 'pull' 'M9 11 条：两页、老→新、不重不漏' {
        $null = Clear-Pending -Session $script:sessionB -OtherUid $UidA
        Close-ChatSession $script:sessionB; $script:sessionB = $null
        Start-Sleep -Milliseconds 400
        $sent = @()
        for($i = 1; $i -le 11; $i++){
            $u = New-UniqueId; $sent += $u
            $null = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $u -Content ("{0}/M9_{1:D2}" -f $tagPrefix, $i) -ChatTime '2026-01-01 00:00:20'
        }
        $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB
        $all = @()
        $after = 0
        for($p = 0; $p -lt 5; $p++){
            $r = Pull-OfflineMsg -Session $script:sessionB -OtherUid $UidA -AfterId $after -Limit 10
            $page = @(Get-MsgList $r.Body)
            $all += $page
            $after = [long]$r.Body.max_id
            if($page.Count -gt 0){
                $ids = @($page | ForEach-Object { $_.unique_id })
                $null = Confirm-OfflineMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds $ids
            }
            if(-not $r.Body.has_more){ break }
        }
        $got = @($all | ForEach-Object { $_.unique_id })
        Add-Check 'M9.1' '两页共 11 条' ($got.Count -eq 11) "count=$($got.Count)"
        Add-Check 'M9.2' '无重复' (($got | Select-Object -Unique).Count -eq 11) "unique=$(($got | Select-Object -Unique).Count)"
        Add-Check 'M9.3' '不重不漏' (((@($sent | Where-Object { $got -contains $_ })).Count) -eq 11) "命中=$((@($sent | Where-Object { $got -contains $_ })).Count)"
        $contents = @($all | ForEach-Object { $_.content })
        $ordered = $true
        for($i = 1; $i -lt $contents.Count; $i++){ if([string]::CompareOrdinal($contents[$i-1], $contents[$i]) -gt 0){ $ordered = $false } }
        Add-Check 'M9.4' '顺序为老→新（content 递增）' $ordered "首=$($contents[0]) 尾=$($contents[-1])"
    }

    Invoke-Case 'pull' 'M10 after_id 游标：只返回比它大的' {
        $null = Clear-Pending -Session $script:sessionB -OtherUid $UidA
        Close-ChatSession $script:sessionB; $script:sessionB = $null
        Start-Sleep -Milliseconds 400
        $sent = @()
        for($i = 1; $i -le 3; $i++){
            $u = New-UniqueId; $sent += $u
            $null = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $u -Content ("{0}/M10_{1}" -f $tagPrefix, $i) -ChatTime '2026-01-01 00:00:30'
        }
        $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB
        $r1 = Pull-OfflineMsg -Session $script:sessionB -OtherUid $UidA -AfterId 0 -Limit 2
        $p1 = @(Get-MsgList $r1.Body)
        Add-Check 'M10.1' 'limit=2 只返回 2 条' ($p1.Count -eq 2) "count=$($p1.Count)"
        $cursor = [long]$r1.Body.max_id
        $r2 = Pull-OfflineMsg -Session $script:sessionB -OtherUid $UidA -AfterId $cursor -Limit 10
        $p2 = @(Get-MsgList $r2.Body)
        Add-Check 'M10.2' 'after_id 之后只剩 1 条' ($p2.Count -eq 1) "count=$($p2.Count)"
        Add-Check 'M10.3' '剩下那条是最新的' ($p2.Count -eq 1 -and $p2[0].unique_id -eq $sent[2]) "uid=$($p2[0].unique_id)"
        $null = Confirm-OfflineMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds (@($p1 + $p2 | ForEach-Object { $_.unique_id }))
    }

    # ============================================================ ack
    Invoke-Case 'ack' 'M13 重复 ACK 幂等' {
        $uid13 = New-UniqueId
        Close-ChatSession $script:sessionB; $script:sessionB = $null
        Start-Sleep -Milliseconds 400
        $null = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $uid13 -Content "${tagPrefix}/M13" -ChatTime '2026-01-01 00:00:40'
        $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB
        $null = Pull-OfflineMsg -Session $script:sessionB -OtherUid $UidA -AfterId 0 -Limit 10
        $a1 = Confirm-OfflineMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds @($uid13)
        $a2 = Confirm-OfflineMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds @($uid13)
        Add-Check 'M13.1' '首次 ACK error=0' (($null -ne $a1) -and ($a1.Body.error -eq 0)) "error=$($a1.Body.error)"
        Add-Check 'M13.2' '重复 ACK 仍 error=0' (($null -ne $a2) -and ($a2.Body.error -eq 0)) "error=$($a2.Body.error)"
        Add-Check 'M13.3' 'delivered 保持 1' ((Get-Delivered $uid13) -eq 1) "delivered=$(Get-Delivered $uid13)"
    }

    Invoke-Case 'ack' 'M15 非法参数：空数组 / 超过 128 条' {
        $uid15 = New-UniqueId
        Close-ChatSession $script:sessionB; $script:sessionB = $null
        Start-Sleep -Milliseconds 400
        $null = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $uid15 -Content "${tagPrefix}/M15" -ChatTime '2026-01-01 00:00:50'
        $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB
        $e1 = Confirm-OfflineMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds @()
        Add-Check 'M15.1' '空数组 → error=1001' (($null -ne $e1) -and ($e1.Body.error -eq 1001)) "error=$($e1.Body.error)"
        $many = @(); for($i = 0; $i -lt 40; $i++){ $many += [guid]::NewGuid().ToString() }
        # 注：单帧上限 MAX_LENGTH=2048，最多塞下约 50 个 uuid，
        #    因此 MarkHasDelivered 里「>128 条」那条守卫在协议层不可达（保留为防滥用）。
        #    这里改成验证「一堆不存在的 id 不会误标」。
        $e2 = Confirm-OfflineMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds $many
        Add-Check 'M15.2' '40 个不存在的 id → error=0 且不误标' (($null -ne $e2) -and ($e2.Body.error -eq 0)) "error=$($e2.Body.error)"
        Add-Check 'M15.3' 'delivered 未被误改（仍为 0）' ((Get-Delivered $uid15) -eq 0) "delivered=$(Get-Delivered $uid15)"
        $null = Confirm-OfflineMsg -Session $script:sessionB -OtherUid $UidA -UniqueIds @($uid15)
    }

    # ============================================================ authz
    Invoke-Case 'authz' 'M14 越权 ACK：A 标不掉自己发给 B 的消息' {
        Close-ChatSession $script:sessionB; $script:sessionB = $null
        Start-Sleep -Milliseconds 400
        $uid14 = New-UniqueId
        $rsp = Send-ChatMsg -Session $sessionA -ToUid $UidB -UniqueId $uid14 -Content "${tagPrefix}/M14" -ChatTime '2026-01-01 00:01:00'
        Add-Check 'M14.1' '前置：消息未投递（online=false）' ($null -ne $rsp -and $rsp.online -eq $false) "online=$($rsp.online)"
        $ack = Confirm-OfflineMsg -Session $sessionA -OtherUid $UidB -UniqueIds @($uid14)
        Add-Check 'M14.2' '越权 ACK 返回 error=0（不报文错误，只是 0 行命中）' (($null -ne $ack) -and ($ack.Body.error -eq 0)) "error=$($ack.Body.error)"
        Add-Check 'M14.3' 'delivered 仍为 0' ((Get-Delivered $uid14) -eq 0) "delivered=$(Get-Delivered $uid14)"
        # 收尾：让 B 正常收下
        $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB
        $null = Clear-Pending -Session $script:sessionB -OtherUid $UidA
    }

    Invoke-Case 'authz' 'M17 好友列表忽略报文里的 uid' {
        $sqlCount = [int]((Invoke-Sql ("select count(*) from {0}.friend where self_id={1}" -f $DbSchema, $UidA)) -split "`n")[0]
        $expect = [Math]::Min($sqlCount, 10)
        if($script:sessionB -eq $null){ $script:sessionB = Open-ChatSession -Uid $UidB -Token $tokenB }
        Send-Frame -Stream $sessionA.Stream -MsgId 1042 -Json '{"uid":999999,"begin":0,"limit":10}'
        $rsp = Read-Frame $sessionA.Stream
        $obj = if($null -ne $rsp){ $rsp.Body | ConvertFrom-Json } else { $null }
        $got = if($null -ne $obj){ @($obj.friend_list).Count } else { -1 }
        Add-Check 'M17.1' '返回的是 A 自己的好友列表' ($got -eq $expect) "count=$got（期望 $expect，SQL 统计 $sqlCount）"
        Add-Check 'M17.2' '没有按报文 uid=999999 返回空列表' ($got -ne 0 -or $expect -eq 0) "count=$got"
    }
}
finally{
    Close-ChatSession $script:sessionA
    Close-ChatSession $script:sessionB

    if(-not $KeepData){
        $null = Invoke-Sql ("delete from {0}.message where content like '{1}%'" -f $DbSchema, $ContentPrefix)
    }
    $afterMsgs = [int]((Invoke-Sql ("select count(*) from {0}.message" -f $DbSchema)) -split "`n")[0]
    if($KeepData){
        Write-Host ("数据       : 已保留（前缀 {0}），message 行数 {1}" -f $ContentPrefix, $afterMsgs) -ForegroundColor Yellow
    }else{
        Write-Host ("数据       : 已清理，message 行数={0}（基线 {1}）" -f $afterMsgs, $baselineMsgs)
    }
    Add-Check 'D1' '测试数据已清理（message 行数回到基线）' ($afterMsgs -eq $baselineMsgs) "now=$afterMsgs baseline=$baselineMsgs"

    Remove-Item Env:MYSQL_PWD -ErrorAction SilentlyContinue
    Remove-Item Env:REDISCLI_AUTH -ErrorAction SilentlyContinue
}

# ---------------------------------------------------------------- 汇总
$fail  = @($script:Checks | Where-Object { -not $_.Ok -and -not $_.Known })
$known = @($script:Checks | Where-Object { -not $_.Ok -and $_.Known })
Write-Host ""
Write-Host "== 断言汇总 =="
Write-Host ("总计: {0} 条断言，失败 {1} 条，已知缺陷 {2} 条" -f $script:Checks.Count, $fail.Count, $known.Count)
if($known.Count -gt 0){
    foreach($k in $known){ Write-Host ("  KNOWN {0} {1}  {2}" -f $k.Id, $k.Name, $k.Detail) -ForegroundColor Yellow }
}
if($fail.Count -gt 0){
    foreach($f in $fail){ Write-Host ("  FAIL {0} {1}  {2}" -f $f.Id, $f.Name, $f.Detail) -ForegroundColor Red }
    exit 1
}
if($known.Count -gt 0){ Write-Host "通过（含已知缺陷，见上）" -ForegroundColor Yellow; exit 0 }
Write-Host "全部通过" -ForegroundColor Green
exit 0