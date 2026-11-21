# 项目规则（AGENTS.md）

> 给 AI 助手看的项目说明与工作规则，人也可以读。
> 最后核对：2026-11-17

## 一、这个项目是什么

C++ 全栈分布式即时通讯学习项目：Qt 客户端 + 5 个后端服务（4 个 C++、1 个 Node.js）。

- 业务、架构、功能清单 → [README.md](README.md)
- 原项目的实现教程（day01–day42）在参考仓库：<https://github.com/secondtonone1/llfcchat>

## 二、目录地图

| 路径 | 是什么 |
|---|---|
| `client/llfcchat/` | Qt6 客户端（CMake） |
| `server/GateServer/` | HTTP 网关（CMake，C++） |
| `server/StatusServer/` | 状态服务（CMake，C++） |
| `server/ChatServer/` | 聊天服务：**单 exe**，多节点由 `nodes/<节点>/config.ini` 区分 |
| `server/VarifyServer/` | 验证码 / 邮件服务（Node.js） |
| `server/sql/` | 数据库结构脚本（仅结构，无数据） |
| `server/proto/` | **协议唯一真源**：`message.proto` + `generated/`（protoc 3.13.0 生成，入库）；三个 C++ 服务编译同一份，VarifyServer 运行时加载 |
| `server/common/` | **三个后端服务共用的基础库**（Logger / ConfigMgr / AsioIOServicePool / Redis 组 / MySQL 组 / data.h / const.h），以 STATIC 库 `llfc_common` 供各服务链接 |
| `docs/测试指南.md` | 自动化检查 / 场景测试 / 手工清单（离线/在线、基础设施启停） |
| `docs/改造记录.md` | ChatServer 合并与健壮性改造的记录（改了什么 / 为什么 / 剩余 backlog） |
| `tools/test/` | 测试脚本：`health-check.ps1`（只读检查）、`infra-ctl.ps1`（启停 MySQL/Redis）、`test-run.ps1`（场景运行）、`service-run.ps1`（起服务+客户端）、`stack-ctl.ps1`（后台栈启停）、`api-test.ps1`（写库测试，带备份/清理）、`doc-sync.ps1`（文档同步检查）、`msg-proto.ps1`（消息链路协议测试：发送/拉取/ACK/分页/幂等/越权，断言以 `message` 表为准） |
| `<模块>/build/` | CMake 构建产物，已 gitignore |
| `<模块>/config.ini` | 真实配置（含密码），已 gitignore，只提交 `.example` |

## 三、真实构建方式（细节版，README 第 6 节只给最短路径）

> 当前实际用的是 **MinGW + Qt 6.11**（证据在 `<模块>/build/CMakeCache.txt`），
> 与 README 第 6 节一致；本节补充编译器 / Qt / gRPC 的具体路径。

| 组件 | 实际路径 |
|---|---|
| 编译器 | `E:/YingYongRuanjian/mingw64/bin/g++.exe`（生成器 `MinGW Makefiles`） |
| Qt | `F:/YingYongRuanJian3/Qt/qita/6.11.0/mingw_64` |
| gRPC / protobuf | `E:/zhongzhuan2/grpc/build_mingw`（静态库 `.a`） |

`build/` 目录已经配置好，日常直接增量编译：

```powershell
cmake --build client\llfcchat\build --parallel
cmake --build server\GateServer\build --parallel
cmake --build server\StatusServer\build --parallel
cmake --build server\ChatServer\build --parallel
```

万一需要重新配置时：

```powershell
cmake -S <模块> -B <模块>\build -G "MinGW Makefiles" `
      -DCMAKE_PREFIX_PATH="F:/YingYongRuanJian3/Qt/qita/6.11.0/mingw_64"
```

> `VarifyServer` 不走 CMake，是 Node.js：`npm install` → `npm run serve`。

> `ChatServer` 的编译选项含 `-Werror=return-type`：非 void 函数漏写 `return` 属于 UB，
> 在 `-O2` 下表现为「随机崩溃」，因此升级为编译错误（曾经由此定位到 `CliOptions.cc` 的回归）。

## 四、怎么跑起来

**唯一的硬约束**：MySQL 8 + Redis 6 必须先就绪——各服务一进 `main` 就连它们
（`MysqlMgr::GetInstance()`、`RedisMgr::GetInstance()->HSet(...)`），连不上直接启动失败。
连接参数见各模块 `config.ini`。

除此之外没有严格顺序（与 README 第 6.4 节一致）：

| 顺序 | 服务 | 说明 |
|---|---|---|
| 1 | MySQL 8 / Redis 6 | 必须先起，启动方式不限（服务 / Docker / 压缩包） |
| 2 | `server/StatusServer`、`server/GateServer` | **二者顺序任意** |
| 3 | `server/ChatServer`（一个 exe，可起 1~N 个节点） | 见下方命令 |
| 4 | `client/llfcchat` — 客户端 | |
| 可选 | `server/VarifyServer` — `npm install` → `npm run serve` | **只在注册 / 重置密码时需要**，不是必起的第一步 |

聊天服务不再是两个 exe，而是**同一个 exe + 每节点一份配置**：

```powershell
cd server\ChatServer\build
.\CHATSERVER.exe --config ..\nodes\chatserver1\config.ini --logdir ..\nodes\chatserver1\logs
.\CHATSERVER.exe --config ..\nodes\chatserver2\config.ini --logdir ..\nodes\chatserver2\logs
```

- `--config` 缺省时依次找 `<exe目录>/config.ini`、`<当前目录>/config.ini`
- `--logdir` 缺省为 `<配置文件所在目录>/logs`
- 两节点必须：`[SelfServer] Name` 不同、TCP 端口不同（8090/8091）、gRPC 端口不同（50055/50056）
- 退出码：`2` 用法错误、`3` 配置错误、`4` 启动失败

**配置文件**：`GateServer` / `StatusServer` 仍是「`config.ini.example` → `config.ini` + CMake 复制到 `build/`」，
改完配置要重新 configure 一次；`ChatServer` 用 `nodes/<节点>/config.ini`，由 `--config` 直接指定，不依赖 `build/` 副本。

## 五、工作规则

### 可以直接做

- 读任何文件、搜索、分析
- 在仓库内新建 / 修改源码与文档
- 跑编译、跑测试、跑格式检查
- 把临时文件放到 `<模块>/build/` 或系统临时目录

### 必须先问

- 删除或移动文件
- 修改真实配置：`config.ini`、`config.json`
- 操作仓库外的任何文件

### 绝不执行（由用户自己完成）

- **`git commit` / `git push`**：AI 只负责在提交前**总结本次工作内容**（改了什么 / 为什么 / 怎么验证 / 还剩什么），
  暂存、填写 commit message、commit、push 全部由用户在 VSCode 里自己操作。
  详见 `docs/文档同步清单.md` 第 7 节「提交（commit / push）约定」。

### 红线

- **绝不**把密码、Token、SMTP 授权码写进源码、文档或提交
- **不动** `.gitignore` 里排除的含口令目录：`server/GateServer/{cc_mysql_test, mysql_2, redis_test}/`
- `config.ini` / `config.json` 永远不入库，仓库里只留 `.example` 模板

## 六、代码约定

- C++ 沿用现有命名：服务端 `.cc/.h`，客户端 `.cpp/.h`
- 提交信息用中文，写清「改了什么 + 为什么」
- 新增依赖前先确认能否复用现有 boost / gRPC / Qt，不要引入第二套
- 数据层与公共常量（`RedisConPool` / `RedisMgr` / `MysqlPool` / `MysqlDao` / `MysqlMgr` / `data.h` / `const.h`）**只在 `server/common/` 存在一份**（2026-11-22 抽出，STATIC 库 `llfc_common`）：改这里即可，**不要**在三个服务目录里再放副本；跑 `doc-sync.ps1` 的 D7（common 齐全 / 服务目录无副本 / 三服务都链接 `llfc_common`）

## 七、已知问题

| 问题 | 说明 |
|---|---|
| ~~数据层有 3 份拷贝~~ **已解决** | 2026-11-22 抽出 `server/common/`（STATIC 库 `llfc_common`）：`Logger` / `ConfigMgr` / `AsioIOServicePool` / `RedisConPool` / `RedisMgr` / `MysqlPool` / `MysqlDao` / `MysqlMgr` / `data.h` / `const.h` 各只剩一份；`doc-sync.ps1` 的 D7 改为「common 齐全 / 服务目录禁止副本 / 三服务都链接 `llfc_common`」。详见 `docs/改造记录.md` §12 |
| ~~`message.proto` 4 份拷贝~~ **已解决** | 2026-11-22 收敛为 `server/proto/message.proto` + `server/proto/generated/`（唯一生成物，入库）；四个服务目录不再有副本，`doc-sync.ps1` 的 D8 会检查。改完协议执行 `cmake --build server\ChatServer\build --target regen_proto` |
| `ChatServer` 运行期探活有延迟 | 连接空闲 ≥600 秒才检查，MySQL 重启后最长约 10 分钟才恢复（BL-10 计划调到 60 秒） |
| README 缺「源码差异」章节 | README 第 2 节目前只列工程层面差异（构建 / 工具链 / 日志 / 配置 / 目录），源码层面的 diff 仍未写，README 里自己标注了「以后会更新」 |
| `CMakeLists.txt` 含硬编码绝对路径 | gRPC / protobuf 路径写死为 `E:/zhongzhuan2/grpc/...`，换机器需要改 |
| 各模块 `config.ini` 不在仓库 | 换机器要按 `.example` 重建；`ChatServer` 还需按 `nodes/<节点>/` 目录重建各节点配置 |
| 数据库初始化 | 只提交 `server/sql/schema.sql`（结构），**绝不提交任何数据**；已有库需手动把 `user_id.id` 设为当前最大 uid |
| 客户端消息状态只做到「服务端已收」 | `SENDING / DELIVERED / FAILED` 已可用；服务端消息已落库、离线消息可拉取（BL-19 ✅ 2026-11-20），投递状态由**接收方 ACK** 确认（BL-22 ✅ 2026-11-21）。仍缺：**发送超时兜底、已读回执、发件箱同步（双向历史）**（BL-17 / BL-20 / BL-25）。设计与现状见 `docs/改造记录.md` 第 10、11 节 |
