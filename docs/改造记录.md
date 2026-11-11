# ChatServer 合并与健壮性改造总结

> 日期：2026-11-08
> 范围：`server/ChatServer`（不含 `GateServer` / `StatusServer` / 客户端）
> 用途：记录「这次改了什么、为什么改、还剩什么」，避免下次接手时只能翻聊天记录。

---

## 1. 这次到底在解决什么

### 起因

`server/ChatServer1` 与 `server/ChatServer2` 是两份几乎相同的副本：

| 对比项 | 结果 |
|---|---|
| 源文件 | 40 个，其中 36 个字节级相同 |
| 真正差异 | `CMakeLists.txt` 的 target 名、`message.proto` 的一段注释、`MysqlPool.cc` 的 4 行调试注释、`config.ini` |
| 节点身份来源 | 全部来自配置（`[SelfServer] Name/Port/RPCPort`），代码里没有硬编码 |

结论：**删除一份副本、改成「一个 exe + 每个节点一份配置」几乎零信息损失。**

### 目标演进（有意为之，但要写下来）

| 阶段 | 目标 | 是否本次范围 |
|---|---|---|
| A | 消除节点副本：单 exe + `nodes/*/config.ini` | ✅ 是 |
| B | 启动可靠性：配置/端口校验、快速失败、明确退出码 | ✅ 是 |
| C | 数据层正确性：连接池超时、坏连接处理、事务状态恢复 | 🟨 附带（由 B 引出） |
| D | 横向复制到其它服务、抽公共库 | ⬜ 下一步 |

> 教训：A 做完后目标实际上从「结构」切到了「可靠性」，但当时没有明确记录，
> 导致后面越改越像「不知道自己在修什么」。

---

## 2. 改动清单

### 2.1 目录与配置

| 项 | 内容 |
|---|---|
| 目录 | `ChatServer1/`、`ChatServer2/` → `ChatServer/` |
| 节点配置 | `ChatServer/nodes/chatserver1/config.ini`、`nodes/chatserver2/config.ini`（已在 `.gitignore` 的 `config.ini` 规则覆盖范围内） |
| 产物 | 单个 `CHATSERVER.exe` |

### 2.2 命令行与配置加载

| 项 | 文件 |
|---|---|
| `--config/-c`、`--logdir`、`--help/-h`、`--config=path`、位置参数 | `CliOptions.h/.cc` |
| `ExeDir()`（取 exe 所在目录，与 CWD 无关） | `CliOptions.cc` |
| 默认配置查找顺序：`--config` → `<exe目录>/config.ini` → `<CWD>/config.ini` | `CliOptions.cc` |
| 路径绝对化 + `lexically_normal()` | `CliOptions.cc` |
| 必填项校验（`[SelfServer] Name/Port/RPCPort`） | `ChatServer.cc` |
| 端口数字/范围校验（1..65535） | `ChatServer.cc` |
| 退出码：2 用法错误 / 3 配置错误 / 4 启动失败 | `CliOptions.h` |
| 日志目录跟随配置文件：`<config 目录>/logs` | `ChatServer.cc` |
| `ConfigMgr::SetConfigPath(const std::string&)` + `_override_path` | `ConfigMgr.h/.cc` |

### 2.3 Redis

| 项 | 文件 |
|---|---|
| `GetRedisConnection(timeout=3s)`：`wait` → `wait_for`，超时返回 `nullptr` + `LOGE` | `RedisConPool.h/.cc` |
| 超时路径先解锁再打日志 | `RedisConPool.cc` |
| `RedisMgr::Ping(timeout)`（只读探针，失败不归还连接） | `RedisMgr.h/.cc` |
| 启动时检查 `HSet(LOGIN_COUNT, ...)` 返回值，失败退出 | `ChatServer.cc` |

### 2.4 MySQL

| 项 | 文件 |
|---|---|
| `GetSqlConnection(timeout=3s)`：`wait_for` + 超时日志 | `MysqlPool.h/.cc` |
| 检查线程：`join`（不 detach）、1 秒粒度轮询、异常兜底 | `MysqlPool.cc` |
| `checkConnection()`：锁内取/放、锁外 `SELECT 1`、归还后 `notify` | `MysqlPool.cc` |
| `EnsureCapacity()`：把池子补到 `poolSize_`，push 前复查容量 | `MysqlPool.cc` |
| `DiscardConnection()`：销毁坏连接 + 尝试补位；入口处统一维护 `borrowed_` | `MysqlPool.cc` |
| 连接计数 `borrowed_`（借出 + 空闲 = 总数，防止缩水/超发） | `MysqlPool.h/.cc` |
| `MysqlMgr::Ping(timeout)`、`MysqlDao::Ping(timeout)` | `MysqlMgr.*`、`MysqlDao.*` |
| 启动探测：`Ping` 失败 → 退出码 4 | `ChatServer.cc` |

### 2.5 归还语义与事务

| 项 | 文件 |
|---|---|
| `SqlConnGuard`：析构时 `markBroken ? Discard : Return` | `MysqlDao.h` |
| `IsConnectionLost()`：2006 / 2013 / 2055 | `MysqlDao.cc` |
| 15 个 DAO 函数统一改为 `SqlConnGuard` + `markBroken` | `MysqlDao.cc` |
| `restoreAutoCommit` Defer（声明在 guard 之后，先执行） | `MysqlDao.cc`（`RegUserTransaction` / `AddFriend`） |
| 三路 catch 统一 `rollback`，防止 `setAutoCommit(true)` 隐式提交半截事务 | `MysqlDao.cc` |

### 2.6 构建

| 项 | 文件 |
|---|---|
| `-Werror=return-type`（防止「非 void 函数缺少 return」这类 UB 再次漏过） | `CMakeLists.txt` |

---

## 3. 修复路线（一步引出下一步）

| 阶段 | 起点问题 | 动作 | 引出 |
|---|---|---|---|
| P0 | 文档与现状不一致 | 同步 `AGENTS.md` / `README.md` | — |
| P1 | 两份节点副本 | 合并为一个目录 + `nodes/*/config.ini` | 一个 exe 必须知道「我是谁」 |
| P2 | `main` 写死 `cwd/config.ini` | 新增 `CliOptions`、`--config/--logdir`、校验、退出码 | 启动必须快速失败 |
| P3 | Redis 不可用时永久阻塞 | `wait_for` 超时、检查返回值 | 同样的病 MySQL 也有 |
| P4 | MySQL 同样会假死 | 超时 + `Ping` + 检查线程 | 检查线程暴露一串并发/记账问题 |
| P5 | 坏连接该丢弃还是归还 | `SqlConnGuard`、`markBroken`、`DiscardConnection` | 事务状态需要一并恢复 |
| P6 | 带 `--config` 启动崩溃 | `gdb` + 干净构建定位 → `ParseArgs` 缺 `return ParseStatus::Ok` | 回归，非新问题 |
| P7 | exe 路径找不到 | 明确 exe 在 `build/`、配置在 `nodes/` | — |

> 分水岭是 **P4 之后的 `DiscardConnection`（可选增强）**：
> 它本身不是 Bug 修复，却长出了 `borrowed_` 记账、`EnsureCapacity`、容量超发、
> 计数下溢、锁边界、`SqlConnGuard` 等一串新问题——这就是「越修越多」的起点。

---

## 4. 遗留与风险（已知未做）

| 项 | 说明 | 影响 |
|---|---|---|
| `GateServer` / `StatusServer` 未移植 | 它们的 `MysqlPool` / `RedisConPool` 仍是旧逻辑（无限 `wait`、无探测） | 这两个服务仍可能「假启动 / 卡死」 |
| 未抽 `server/common` | 同一份修复要在 3 个服务里重复 | 维护成本 |
| `message.proto` 仍有 4 份 | 改协议要改四处 | 易漏、wire 不兼容风险 |
| 运行期恢复延迟 | 探活只在「空闲 ≥600 秒」时执行 | MySQL 重启后最长 10 分钟才恢复 |
| `DiscardConnection` 在业务线程同步 `connect` | MySQL 挂了时该线程会等连接超时 | 建议给连接串加 `connectTimeout` |
| `CMakeLists.txt` 硬编码绝对路径 | 换机器要改 | 换机成本 |
| README 缺「源码差异」章节 | 作者已标注待补 | 文档完整性 |

---

## 5. Backlog（骨架）

状态：⬜ 待办 / 🟨 进行中 / ✅ 完成 / 💤 暂缓

| ID | 事项 | 优先级 | 状态 | 验收方式 |
|---|---|---|---|---|
| BL-01 | 双节点联调：两个账号分别登到 8090/8091，互发消息 | P0 | 🟨 | 客户端能收到跨节点消息 |
| BL-02 | 停 MySQL → 观察探活失败 → 重启 → 1 分钟内恢复 | P0 | ⬜ | 日志出现「重连成功」，查询恢复 |
| BL-03 | Ctrl+C 1 秒内干净退出，无 terminate / 崩溃 | P0 | ⬜ | 连续 5 次退出均正常 |
| BL-04 | Redis 端口改错 → 3 秒内超时退出（退出码 4） | P1 | ⬜ | 日志含「Redis 连接池等待超时」 |
| BL-05 | 把超时 + 探测 + 检查线程移植到 `GateServer` | P1 | ⬜ | 该服务同样能快速失败 |
| BL-06 | 同上移植到 `StatusServer` | P1 | ⬜ | 同上 |
| BL-07 | 抽 `server/common`（Logger/ConfigMgr/Redis/MySQL/RPC 客户端） | P1 | ⬜ | 三个服务共用一份源码 |
| BL-08 | `message.proto` 统一为单一来源 + 一处生成 | P2 | ⬜ | 删掉其余 3 份拷贝 |
| BL-09 | CMake 绝对路径改为变量 / `CMakePresets.json` | P2 | ⬜ | 换机器只改一处 |
| BL-10 | 运行期探活阈值 600s → 60s（或改用 `isValid()`） | P2 | ⬜ | 停库后 1 分钟内恢复 |
| BL-11 | README 补「源码差异」章节 | P3 | ⬜ | 文档评审通过 |
| BL-12 | `GateServer` / `StatusServer` 也加 `-Werror=return-type` | P3 | ⬜ | 构建通过 |

---

## 6. 验收清单（每批改动后都要跑）

### 6.1 构建

```powershell
cmake --build server\ChatServer\build --parallel
# 期望：0 error；-Werror=return-type 下也能通过
```

### 6.2 冒烟四连

```powershell
cd server\ChatServer\build

# 1) 正常启动（需 MySQL + Redis 已就绪）
.\CHATSERVER.exe --config ..\nodes\chatserver1\config.ini --logdir ..\nodes\chatserver1\logs
# 期望：5 条 "mysql connection init success" + 启动横幅（node/config/tcp/grpc/peer/logs）

# 2) 坏配置：应快速报错退出，不崩溃
.\CHATSERVER.exe --config ..\nodes\nope.ini
# 期望："error: 配置文件不存在" + 退出码 3

# 3) 停掉 MySQL 后再启动
# 期望：3 秒内 "Mysql 连接池等待超时" + "MySQL 不可达" + 退出码 4

# 4) 运行中 Ctrl+C
# 期望：1 秒内退出，无 terminate / 0xC0000374
```

### 6.3 自愈（BL-02）

```powershell
# 1) 正常启动节点
# 2) 停 MySQL 服务，用客户端登录（触发查询失败）
# 3) 观察日志："Error keeping connection alive" / "探活失败"
# 4) 重启 MySQL，等待 ≤1 分钟
# 5) 再登录：应恢复正常，日志含 "mysql again connection success"
```

### 6.4 日志位置

| 启动方式 | 日志目录 |
|---|---|
| `--config nodes/chatserver1/config.ini` | `nodes/chatserver1/logs/` |
| `--config nodes/chatserver2/config.ini` | `nodes/chatserver2/logs/` |
| 无 `--config` | `<当前目录>/logs/` |
| 显式 `--logdir D:\x` | `D:\x\` |

文件名：`<SelfServer.Name>_<YYYYMMDD>.log`，超过 64MB 轮转为 `_1`、`_2`。

---

## 7. 过程教训（流程）

1. **目标切换要写下来**：从「去重复」切到「可靠性」时，应在文档里换一行标题。
2. **可选优化必须标记并延后**：`DiscardConnection` 是增强而非修复，却引出了十个新问题。
3. **评审要有边界**：每轮只处理「崩溃 / 数据错误 / 阻塞主目标」三类，其余记 backlog。
4. **缺陷分三类对待**：新引入的回归（必须立刻修）、历史债务（排期）、设计改进（评估）。
5. **完成定义**：每个修复都配一条可执行的验收命令，没有验收就不算完成。
6. **让编译器兜底**：`-Werror=return-type` 把「靠人看」变成「编译不过」。
7. **记录优先**：本次 40+ 轮里没有一份过程记录，这是最大的流程问题——本文件就是补课。

---

## 8. 下一步

1. 完成 6.2 四连 + BL-02 自愈验证（BL-01 ~ BL-03）。
2. 按 BL-05 / BL-06 逐个移植到 `GateServer`、`StatusServer`，每移植一个立刻冒烟。
3. 移植稳定后再做 BL-07（抽 `server/common`）——那时三份代码已经收敛，抽库是水到渠成。
