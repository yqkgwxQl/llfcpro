# 项目规则（AGENTS.md）

> 给 AI 助手看的项目说明与工作规则，人也可以读。
> 最后核对：2026-11-07

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
| `server/ChatServer1/`、`server/ChatServer2/` | 聊天服务，两个节点（CMake，C++） |
| `server/VarifyServer/` | 验证码 / 邮件服务（Node.js） |
| `server/sql/` | 数据库结构脚本（仅结构，无数据） |
| `<模块>/build/` | CMake 构建产物，已 gitignore |
| `<模块>/config.ini` | 真实配置（含密码），已 gitignore，只提交 `.example` |

## 三、真实构建方式（**和 README 不一样**）

> ⚠️ README 的「快速开始」还停留在 **VS2019 + Qt 5.15 + MSVC**，那是旧方案。
> 当前实际用的是 **MinGW + Qt 6.11**，证据在 `<模块>/build/CMakeCache.txt`。

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
cmake --build server\ChatServer1\build --parallel
```

万一需要重新配置时：

```powershell
cmake -S <模块> -B <模块>\build -G "MinGW Makefiles" `
      -DCMAKE_PREFIX_PATH="F:/YingYongRuanJian3/Qt/qita/6.11.0/mingw_64"
```

> `VarifyServer` 不走 CMake，是 Node.js：`npm install` → `npm run serve`。

## 四、怎么跑起来

**基础设施**（先起）：MySQL 8 + Redis 6。连接参数见各模块 `config.ini`。

**启动顺序**（不能颠倒）：

1. `server/VarifyServer` — `npm install` → `npm run serve`
2. `server/StatusServer` — 编译产物 exe
3. `server/GateServer` — 编译产物 exe
4. `server/ChatServer1`（可再起 `ChatServer2` 模拟多节点）
5. `client/llfcchat` — 客户端

**配置文件**：每个模块把 `config.ini.example` 复制成 `config.ini`，再填真实地址与密码。

## 五、工作规则

### 可以直接做

- 读任何文件、搜索、分析
- 在仓库内新建 / 修改源码与文档
- 跑编译、跑测试、跑格式检查
- 把临时文件放到 `<模块>/build/` 或系统临时目录

### 必须先问

- `git commit` / `git push`
- 删除或移动文件
- 修改真实配置：`config.ini`、`config.json`
- 操作仓库外的任何文件

### 红线

- **绝不**把密码、Token、SMTP 授权码写进源码、文档或提交
- **不动** `.gitignore` 里排除的含口令目录：`server/GateServer/{cc_mysql_test, mysql_2, redis_test}/`
- `config.ini` / `config.json` 永远不入库，仓库里只留 `.example` 模板

## 六、代码约定

- C++ 沿用现有命名：服务端 `.cc/.h`，客户端 `.cpp/.h`
- 提交信息用中文，写清「改了什么 + 为什么」
- 新增依赖前先确认能否复用现有 boost / gRPC / Qt，不要引入第二套

## 七、已知问题

| 问题 | 说明 |
|---|---|
| README 构建章节过时 | 还写 VS2019 + Qt 5.15，实际是 MinGW + Qt 6.11。以本文第三节为准 |
| `CMakeLists.txt` 含硬编码绝对路径 | gRPC / protobuf 路径写死为 `E:/zhongzhuan2/grpc/...`，换机器需要改 |
| 各模块 `config.ini` 不在仓库 | 换机器要按 `.example` 重建 |
| 数据库初始化 | 只提交 `server/sql/schema.sql`（结构），**绝不提交任何数据**；已有库需手动把 `user_id.id` 设为当前最大 uid |
