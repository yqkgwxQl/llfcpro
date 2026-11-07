# LLFCChatPro —— C++ 全栈分布式即时通讯（学习项目）

> 本项目是对开源项目 **[secondtonone1/llfcchat](https://github.com/secondtonone1/llfcchat)** 的**仿写与改造**，
> 配合 B 站UP主的视频教程学习：<https://space.bilibili.com/271469206>

**声明**

- 系统架构、业务逻辑、通信协议、数据库设计均来自原作者 **secondtonone1** 的 [llfcchat](https://github.com/secondtonone1/llfcchat)。
- 本仓库重写的是：构建系统（CMake + MinGW）、工具链（Qt6）、日志模块、配置模板化、目录结构。
- 本仓库**不包含**原项目的开发文档与 README，那些属于原作者，请到原仓库阅读。
- 本项目仅用于学习，不可用于商业用途。

---

## 目录

- [1. 项目是什么](#1-项目是什么)
- [2. 与参考项目的差异](#2-与参考项目的差异)
- [3. 技术栈](#3-技术栈)
- [4. 系统架构](#4-系统架构)
- [5. 目录结构](#5-目录结构)
- [6. 快速开始](#6-快速开始)
- [7. 配置说明](#7-配置说明)
- [8. 学习资料](#8-学习资料)

---

## 1. 项目是什么

一个可运行的分布式即时通讯系统，包含 **Qt 桌面客户端** 和 **5 个后端服务**，覆盖：

- 邮箱验证码 **注册 / 重置密码**
- 登录鉴权与 **Token 会话**
- 好友搜索、申请、认证
- **分布式聊天**：多 ChatServer 节点跨服消息转发
- 文件 / 图片传输与断点续传
- 心跳保活、多节点踢人、分布式锁

**技术覆盖面**：现代 C++（C++17）、Boost.Asio/Beast 网络编程、gRPC 服务间通信、MySQL/Redis 数据层、Qt 桌面开发、Node.js 辅助服务。

---

## 2. 与参考项目的差异

这是本仓库区别于 [llfcchat](https://github.com/secondtonone1/llfcchat) 的地方(**代码里其实有很大差异,这里暂时不进行添加,以后会更新README.md,详细说明源码之间的差异**)：

| 方面 | 参考项目 | 本项目 |
|---|---|---|
| **构建系统** | Visual Studio 解决方案（`.sln` / `.vcxproj`）+ 客户端 `qmake`（`.pro`） | **CMake**（每个模块独立 `CMakeLists.txt`） |
| **编译器** | MSVC | **MinGW-w64 g++**（`-Wall -Wextra -pthread`） |
| **C++ 标准** | C++11 | **C++17** |
| **客户端 Qt** | Qt 5（`.pro` 里兼容 5/6 的写法） | **Qt 6.11**（`find_package(Qt6 6.5)`） |
| **日志** | 无统一日志模块 | 新增 **`Logger.cc/h`**（异步落盘、分级别、支持 `Logger.md` 说明） |
| **配置管理** | 直接放真实 `config.ini` | 仓库只放 **`config.ini.example`**，CMake `configure_file` 自动复制到构建目录 |
| **目录命名** | `server/ChatServer` | `server/ChatServer1`（与 `ChatServer2` 成对，贴合多节点语义） |
| **忽略规则** | 较宽松 | `.gitignore` 收紧：构建产物、IDE 配置、真实配置、含口令的练习目录全部排除 |
| **文档** | 42 篇开发文档（day01–day42） | 不重复收录，见 [学习资料](#8-学习资料) |

---

## 3. 技术栈

| 层 | 组件 | 版本 / 说明 |
|---|---|---|
| 语言 | C++ | C++17 |
| 编译器 | MinGW-w64 g++ | 64 位 |
| 构建 | CMake | ≥ 3.15（客户端要求 ≥ 3.19） |
| 客户端 | Qt | **6.11.0 mingw_64** |
| 网络 | Boost.Asio / Beast | Boost 1.91（HTTP + TCP） |
| 服务间通信 | gRPC + protobuf | **gRPC 1.34.0**，protobuf 随其内置 |
| JSON | jsoncpp | — |
| 数据库 | MySQL 8 + MySQL Connector/C++ | MySQL 8.x；Connector 需用 MinGW 编译版 |
| 缓存 | Redis 6 + hiredis | 验证码、Token、会话、连接数 |
| 辅助服务 | Node.js | VarifyServer（验证码 / 邮件） |

---

## 4. 系统架构

```
        ┌──────────────┐
        │  Qt 客户端    │
        └──────┬───────┘
      HTTP     │      TCP(长连接)
               ▼
     ┌──────────────────┐
     │   GateServer     │  HTTP 网关：注册/登录/重置
     └───┬────────┬─────┘
         │gRPC    │gRPC
         ▼        ▼
 ┌────────────┐ ┌──────────────┐
 │VarifyServer│ │ StatusServer │  验证码/邮件   状态与 Token
 └────────────┘ └───────┬──────┘
                        │gRPC
        ┌───────────────┴───────────────┐
        ▼                               ▼
 ┌──────────────┐               ┌──────────────┐
 │ ChatServer1  │◄──gRPC 转发──►│ ChatServer2  │
 └──────┬───────┘               └──────┬───────┘
        │                              │
        └───────────┬──────────────────┘
                    ▼
          ┌──────────────────┐
          │ MySQL  /  Redis  │
          └──────────────────┘
```

**要点**：客户端只连 GateServer（HTTP）和某个 ChatServer（TCP）；服务间用 gRPC；Redis 负责跨节点状态（用户在哪台 ChatServer、连接数、Token）。

---

## 5. 目录结构

```
llfcchatpro/
├─ client/llfcchat/          Qt6 客户端（CMake）
├─ server/
│  ├─ GateServer/            HTTP 网关
│  ├─ StatusServer/          状态服务（Token / 用户状态）
│  ├─ ChatServer1/           聊天服务节点 1
│  ├─ ChatServer2/           聊天服务节点 2
│  └─ VarifyServer/          验证码 / 邮件服务（Node.js）
├─ docs/mindmaps/            设计思维导图（Excalidraw）
├─ AGENTS.md                 开发约定与构建细节
├─ .gitignore
└─ README.md
```

每个 C++ 模块内部结构一致：

```
<模块>/
├─ CMakeLists.txt
├─ config.ini.example        配置模板（复制成 config.ini 使用）
├─ *.cc / *.h                源码
├─ message.proto             gRPC 协议定义
├─ message.pb.* / *.grpc.pb.*  protoc 生成
└─ build/                    CMake 构建产物（不入库）
```

> `docs/mindmaps/` 存放开发过程中画的设计图：GateServer、ChatServer、客户端三张。
> 它们是 **Excalidraw 格式**——GitHub 上只能看到源码（一段 JSON），看图需克隆后用 Obsidian + Excalidraw 插件打开。
>
> **状态说明**：三张图是在不同阶段画的，与当前代码可能存在出入（不影响整体理解），内容也仍在精简。后续会逐步校准。

---

## 6. 快速开始

> 完整的构建参数与依赖路径见 **[AGENTS.md](AGENTS.md)** 的构建章节；本节省略细节，只给最短路径。

### 6.1 环境准备

| 依赖 | 说明 |
|---|---|
| MinGW-w64 | 64 位 g++，需加入 PATH |
| CMake | ≥ 3.15 |
| Qt 6.5+ | 客户端，需 mingw_64 版本 |
| Boost 1.8x | Asio / Beast |
| gRPC + protobuf | **需用 MinGW 自行编译** |
| MySQL Connector/C++ | 需 MinGW 版本 |
| MySQL 8 / Redis 6 | 数据层 |
| Node.js 16+ | 仅 VarifyServer |

### 6.2 编译

每个 C++ 模块独立构建：

```powershell
cmake -S server\GateServer -B server\GateServer\build -G "MinGW Makefiles"
cmake --build server\GateServer\build --parallel
```

其余模块同理：`StatusServer`、`ChatServer1`、`ChatServer2`、`client\llfcchat`。

### 6.3 数据库与缓存

**结构脚本**：[`server/sql/schema.sql`](server/sql/schema.sql) —— **只有建表语句与存储过程，不含任何业务数据**。

```powershell
# 连接参数见各模块 config.ini 的 [Mysql] 段
mysql -h <host> -P <port> -u <user> -p < server\sql\schema.sql
```

**库结构**（4 表 + 1 存储过程）：

| 对象 | 作用 | 关键字段 |
|---|---|---|
| `user` | 用户主表 | `uid` 用户号、`name` 登录名、`email`、`pwd`、`nick`、`desc`、`sex`、`icon` |
| `user_id` | uid 发号器，单行计数 | `id` 当前最大 uid |
| `friend` | 好友关系，双向各存一行 | `(self_id, friend_id)` 唯一键防重复；`back` 备注 |
| `friend_apply` | 好友申请 | `from_uid`、`to_uid`、`status`（0 待处理 / 1 同意 / 2 拒绝） |
| `reg_user` | 注册存储过程 | 事务内校验重名 + 原子发号，返回新 uid / 0 重名 / -1 异常 |

> 库里不预置任何测试账号；`user_id` 的那一行是发号器起点，属于结构的一部分。
> 若在已有数据的库上初始化，请把 `user_id.id` 调整为当前最大的 `uid`。

然后启动 Redis 6。

### 6.4 启动顺序

**唯一的硬约束：MySQL 和 Redis 必须先就绪。**

各服务一进 `main` 就连它们（`MysqlMgr::GetInstance()`、`RedisMgr::GetInstance()->HSet(...)`），
连不上会直接启动失败。

```
① MySQL 8  ┐
② Redis 6  ┘   ← 必须先起来

③ StatusServer  ┐
                ├─ 二者顺序任意
④ GateServer    ┘
⑤ ChatServer1/2

⑥ VarifyServer  ← 只在「注册 / 重置密码」时需要
```

**前置条件**：MySQL 与 Redis 已启动，且地址、端口、密码与各模块 `config.ini` 的 `[Mysql]` / `[Redis]` 段一致。启动方式不限——Windows 服务、Docker、官方压缩包都可以，本项目不绑定具体安装方式。

用 Docker 起一套只是示例，换成 Windows 服务或官方压缩包同样可以：

```powershell
# 端口与密码换成你 config.ini 里的值
docker run -d --name llfc-mysql -p <MySQL端口>:3306 -e MYSQL_ROOT_PASSWORD=<密码> mysql:8
docker run -d --name llfc-redis -p <Redis端口>:6379 redis:6 --requirepass <密码>
```

基础设施就绪后，启动各服务：

```powershell
# 2个 C++ 服务———— 网关服务器和状态服务器。启动顺序任意
server\GateServer\build\GateServer.exe
server\StatusServer\build\StatusServer.exe

# 聊天服务器
server\ChatServer1\build\CHATSERVER1.exe
server\ChatServer2\build\CHATSERVER2.exe

# 需要注册新用户 / 重置密码时才启动
cd server\VarifyServer
npm install
npm run serve
```


### 6.5 客户端

```powershell
client\llfcchat\build\llfcchat.exe
```

---

## 7. 配置说明

每个模块的配置都是「模板 + 本地副本」：

```
config.ini.example   ← 入库，结构说明
config.ini           ← 本地生成，不入库（含密码）
```

首次运行前：

```powershell
copy server\GateServer\config.ini.example server\GateServer\config.ini
# 然后填入真实的 MySQL / Redis / 服务地址与端口到config.ini里
```

CMake 在 configure 阶段会把 `config.ini` 复制到 `build/`，**所以填完配置后要重新 configure 一次**。

> `VarifyServer` 用的是 `config.example.json` → `config.json`，同理。
> 这类本地配置文件已在 `.gitignore` 中排除，仓库里只保留 `.example` 模板。

---

## 8. 学习资料

| 资料 | 链接 |
|---|---|
| **原项目（架构与代码来源）** | <https://github.com/secondtonone1/llfcchat> |
| **B 站视频教程** | <https://www.bilibili.com/video/BV1k2421K7ZB/?spm_id_from=333.1387.homepage.video_card.click> |
| 原项目开发文档（day01–day42） | 见原仓库 `开发文档/` 目录 |

学习建议：先看视频理解设计意图，再对照原仓库代码，最后看本项目「改了什么」——重点体会从 VS/qmake 迁到 CMake/MinGW 时踩的坑。

---

## 9. 致谢与许可

- 架构与业务实现：**[secondtonone1](https://github.com/secondtonone1/llfcchat)**
- 学习引导：B 站 UP 主（<https://space.bilibili.com/271469206>）
- 本项目对上述内容的改造：构建系统、工具链、日志模块、配置与文档

原项目采用的开源许可见其仓库。本项目的改造部分同样仅用于学习交流。
