# LLFCChatPro - C++ 全栈分布式即时通讯系统（学习项目）

> ⚡ 一个基于 C++ 的全栈即时通讯项目，支持 PC 端 QT 界面、分布式服务架构、gRPC 服务间通信、ASIO 异步 TCP 长连接。
> 
> 📖 **本项目是跟着 B 站 UP主 [恋恋风辰](https://space.bilibili.com/405025224) 的教程系列（[视频合集](https://www.bilibili.com/video/BV13Z421W7WA/)）一步步实践完成的学习项目。**

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Qt-5.15-green.svg)](https://www.qt.io/)
[![Boost](https://img.shields.io/badge/Boost-1.81-orange.svg)](https://www.boost.org/)
[![gRPC](https://img.shields.io/badge/gRPC-1.34-red.svg)](https://grpc.io/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## 📖 目录

- [项目概述](#项目概述)
- [项目架构](#项目架构)
- [技术栈](#技术栈)
- [项目结构](#项目结构)
- [功能介绍](#功能介绍)
- [快速开始](#快速开始)
- [开发环境配置](#开发环境配置)
- [服务部署说明](#服务部署说明)
- [性能测试](#性能测试)
- [面试考察点](#面试考察点)
- [第二季规划](#第二季规划)
- [致谢](#致谢)

---

## 项目概述

**LLFCChatPro** 是一个跟着 B 站教程从零到一实践的全栈即时通讯系统，涵盖了客户端界面开发、分布式服务器设计、网络通信、数据持久化等完整的技术链路。项目展示了 C++ 在实际工业级项目中的综合应用，适合作为项目实践案例写入简历。

| 维度 | 说明 |
|------|------|
| **客户端** | PC 端基于 Qt 5.15 开发，仿微信布局，支持登录/注册/重置密码/好友管理/实时聊天 |
| **网关服务** | C++ Boost.Beast 实现 HTTP 网关，负责登录、注册、验证码请求分发 |
| **验证服务** | Node.js + gRPC，用于邮箱验证码生成与邮件发送 |
| **状态服务** | 服务注册与发现 + 负载均衡，根据连接数动态分配 ChatServer |
| **聊天服务** | C++ Boost.Asio 实现高并发 TCP 长连接，支持分布式多节点部署 |
| **服务通信** | 服务间通过 gRPC (Protocol Buffers) 通信 |
| **数据层** | MySQL 存储用户数据 + Redis 处理缓存与验证码过期 |

经测试，单服务节点可支持 **8000+** 稳定 TCP 连接，多节点分布式部署可支撑 **1W~2W** 活跃用户。

---

## 项目架构

### 系统架构图

```
┌──────────────────────────────────────────────────────────────┐
│                        QT 客户端 (PC)                         │
│            (登录 / 注册 / 聊天 / 好友 / 联系人)               │
└──────┬──────────────┬──────────────┬─────────────────────────┘
       │  HTTP/JSON   │   TCP/TLV    │
       ▼              ▼              ▼
┌──────────┐   ┌──────────┐   ┌──────────────────┐
│GateServer│   │ Status   │   │   ChatServer 1   │
│ 网关服务  │◄──│ 状态服务  │◄──│  (ASIO TCP)      │
│(Beast     │   │(gRPC)    │   │   ChatServer 2   │
│ HTTP)    │   │ 负载均衡  │   │  (ASIO TCP)      │
└────┬─────┘   └──────────┘   │   ChatServer 3   │
     │                        │  (ASIO TCP)      │
     │ gRPC                   └────────┬─────────┘
     ▼                                │ gRPC
┌──────────┐                          │
│ Varify   │◄─────────────────────────┘
│ Server   │     服务间通信
│(Node.js  │
│ 验证码)  │
└──────────┘

┌──────────────────────────────────────────────┐
│             Redis (缓存/会话)                  │
│              MySQL (持久化)                    │
└──────────────────────────────────────────────┘
```

### 服务通信流程

1. **注册流程**：Client → GateServer (HTTP) → VarifyServer (gRPC，发送验证码) → Redis（缓存验证码）→ 返回结果
2. **登录流程**：Client → GateServer (HTTP) → StatusServer (gRPC，获取最优 ChatServer) → 返回 ChatServer 地址 + Token → Client 连接 ChatServer (TCP) → ChatServer 验证 Token (gRPC → StatusServer)
3. **聊天流程**：Client A → ChatServer1 (TCP) → 判断目标用户所在服务器 → 同服直接转发 / 跨服 gRPC → 目标用户
4. **好友申请**：Client A → ChatServer1 → Redis 查询目标服务器 → 同服 Session 转发 / 跨服 gRPC 通知

---

## 技术栈

### 前端

| 技术 | 用途 |
|------|------|
| **Qt 5.15** (Widgets) | 桌面 GUI 框架 |
| **QListWidget** | 联系人列表、聊天列表 |
| **QGridLayout / QPainter** | 气泡聊天框组件 |
| **QTcpSocket** | TCP 长连接管理 |
| **QNetworkAccessManager** | HTTP 请求封装 |
| **QSS** | 界面样式美化 |
| **Qt Signal-Slot** | 异步消息解耦 |

### 后端

| 服务 | 技术 | 说明 |
|------|------|------|
| **GateServer** | Boost.Beast + Asio + jsoncpp | HTTP 网关，请求路由 |
| **ChatServer** | Boost.Asio | TCP 长连接，多线程 IOContext 池 |
| **StatusServer** | gRPC C++ | 服务注册、负载均衡、Token 验证 |
| **VarifyServer** | Node.js + gRPC-js | 验证码邮件发送 |

### 服务间通信

| 技术 | 用途 |
|------|------|
| **gRPC** (Protocol Buffers) | 所有服务间远程调用 |
| **grpc-cpp** | C++ 服务端/客户端 |
| **grpc-js / @grpc/proto-loader** | Node.js 服务端 |
| **连接池** | 每个 gRPC 客户端均封装连接池 |

### 数据层

| 技术 | 用途 |
|------|------|
| **MySQL 8.0** | 用户数据、好友关系持久化 |
| **MySQL Connector C++** | C++ MySQL 访问库 |
| **Redis 6.x** | 验证码缓存、Token 存储、会话管理 |
| **hiredis** | C++ Redis 访问库 |
| **ioredis** | Node.js Redis 库 |

### 协议

| 协议 | 位置 | 格式 |
|------|------|------|
| HTTP/1.1 | Client ↔ GateServer | JSON |
| TCP (自定义 TLV) | Client ↔ ChatServer | 消息头(2字节ID + 2字节长度) + 消息体(JSON) |
| gRPC | 服务间内部 | Protocol Buffers |

---

## 项目结构

```
llfcchatpro/
├── client/
│   └── llfcchat/                    # Qt 客户端工程
│       ├── main.cpp                 # 入口，QSS 加载
│       ├── mainwindow.h/cpp         # 主窗口（界面切换调度）
│       ├── logindialog.h/cpp/ui     # 登录界面
│       ├── registerdialog.h/cpp/ui  # 注册界面
│       ├── resetdialog.h/cpp/ui     # 重置密码界面
│       ├── chatdialog.h/cpp/ui      # 聊天主界面
│       ├── chatpage.h/cpp/ui        # 聊天页面
│       ├── chatview.h/cpp           # 自定义滚动聊天布局
│       ├── chatitembase.h/cpp       # 聊天项基类
│       ├── bubbleframe.h/cpp        # 气泡框基类（带三角形）
│       ├── textbubble.h/cpp         # 文本气泡
│       ├── picturebubble.h/cpp      # 图片气泡
│       ├── chatuserwid.h/cpp/ui     # 聊天用户列表项
│       ├── chatuserlist.h/cpp       # 聊天用户列表（滚动加载）
│       ├── contactuserlist.h/cpp    # 联系人列表
│       ├── applyfriendpage.h/cpp/ui # 好友申请页面
│       ├── applyfrienditem.h/cpp/ui # 申请条目
│       ├── applyfriend.h/cpp/ui     # 申请好友对话框
│       ├── authenfriend.h/cpp/ui    # 认证好友对话框
│       ├── findsuccessdlg.h/cpp/ui  # 查找成功对话框
│       ├── findfaildlg.h/cpp/ui     # 查找失败对话框
│       ├── searchlist.h/cpp         # 搜索列表
│       ├── httpmgr.h/cpp            # HTTP 管理器（单例）
│       ├── tcpmgr.h/cpp             # TCP 管理器（单例，粘包处理）
│       ├── usermgr.h/cpp            # 用户信息管理器
│       ├── userdata.h/cpp           # 用户数据结构
│       ├── statewidget.h/cpp        # 侧边栏状态控件
│       ├── clickedlabel.h/cpp       # 可点击 Label（六态样式）
│       ├── clickedbtn.h/cpp         # 可点击 Button（三态样式）
│       ├── customizeedit.h/cpp      # 自定义输入框
│       ├── timerbtn.h/cpp           # 定时器按钮（倒计时）
│       ├── loadingdlg.h/cpp/ui      # 加载对话框
│       ├── singleton.h              # 单例基类模板
│       ├── global.h/cpp             # 全局定义 + 配置
│       ├── config.ini               # 客户端配置文件
│       ├── style/stylesheet.qss     # 全局 QSS 样式
│       ├── rc.qrc                   # Qt 资源文件
│       └── static/                  # 静态资源（图片等）
│
├── server/
│   ├── GateServer/                  # 网关服务 (VS 工程)
│   │   ├── CServer.h/cpp            # HTTP 服务监听
│   │   ├── HttpConnection.h/cpp     # HTTP 连接处理
│   │   ├── LogicSystem.h/cpp        # 逻辑路由注册
│   │   ├── VerifyGrpcClient.h/cpp   # gRPC 验证客户端
│   │   ├── StatusGrpcClient.h/cpp   # gRPC 状态客户端
│   │   ├── RedisMgr.h/cpp           # Redis 管理器 + 连接池
│   │   ├── MysqlMgr.h/cpp           # MySQL 管理器 + 连接池
│   │   ├── ConfigMgr.h/cpp          # 配置管理
│   │   ├── Singleton.h              # 单例基类
│   │   ├── AsioIOServicePool.h/cpp  # ASIO 多线程 IO 池
│   │   ├── message.proto            # Protobuf 定义
│   │   └── config.ini               # 服务配置
│   │
│   ├── StatusServer/                # 状态服务 (VS 工程)
│   │   ├── StatusServiceImpl.h/cpp  # gRPC 服务实现
│   │   ├── RedisMgr.h/cpp           # Redis 管理器
│   │   ├── MysqlMgr.h/cpp           # MySQL 管理器
│   │   └── config.ini               # 服务配置
│   │
│   ├── ChatServer/                  # 聊天服务 1 (VS 工程)
│   │   ├── ChatServer2/             # 聊天服务 2 (复制)
│   │   ├── ChatServer3/             # 聊天服务 3 (复制)
│   │   ├── CServer.h/cpp            # TCP 服务监听
│   │   ├── CSession.h/cpp           # 会话管理
│   │   ├── LogicSystem.h/cpp        # 业务逻辑路由
│   │   ├── UserMgr.h/cpp            # 用户 Session 绑定
│   │   ├── ChatGrpcClient.h/cpp     # gRPC 跨服通信客户端
│   │   ├── ChatServiceImpl.h/cpp    # gRPC 跨服通信服务端
│   │   ├── StatusGrpcClient.h/cpp   # 状态服务客户端
│   │   ├── RedisMgr.h/cpp           # Redis 管理器
│   │   ├── MysqlMgr.h/cpp           # MySQL 管理器
│   │   ├── message.proto            # Protobuf 定义
│   │   └── config.ini               # 服务配置
│   │
│   ├── VarifyServer/                # 验证服务 (Node.js)
│   │   ├── server.js                # gRPC 服务入口
│   │   ├── proto.js                 # Protocol Buffers 加载
│   │   ├── email.js                 # 邮件发送封装
│   │   ├── redis.js                 # Redis 缓存操作
│   │   ├── config.js                # 配置读取
│   │   ├── const.js                 # 常量定义
│   │   ├── config.json              # 配置文件
│   │   ├── message.proto            # Protobuf 定义
│   │   └── package.json             # Node.js 依赖
│   │
│   └── chatsever_buchong/           # 补充代码
│
├── 开发文档/                         # 30 天完整开发教程
│   └── day01-day30.md               # 每日开发笔记
│
└── README.md                        # 本文件
```

---

## 功能介绍

### ✅ 用户模块

| 功能 | 描述 |
|------|------|
| **邮箱注册** | 邮箱正则验证 → 发送验证码邮件 → 验证码校验 → 写入 MySQL |
| **密码登录** | 用户名+密码验证 → 从 StatusServer 获取 ChatServer 地址 → 建立长连接 |
| **重置密码** | 邮箱验证码验证 → 更新 MySQL 密码 |
| **密码显示/隐藏** | 点击切换明文/密文，带悬浮和按下态图标 |
| **验证码倒计时** | 获取验证码后按钮 60s 倒计时，结束后恢复可点击 |
| **配置化管理** | 客户端和服务端均支持 config.ini / config.json 配置 |

### 💬 聊天模块

| 功能 | 描述 |
|------|------|
| **气泡聊天框** | 自定义网格布局 + QPainter 绘制带三角的气泡（自己/他人样式） |
| **文本消息** | 自动计算文字宽度，自适应气泡大小 |
| **图片消息** | 支持缩略图显示，保持宽高比 |
| **发送按钮** | 支持文本/图片切换发送 |
| **聊天列表** | 自定义 ChatUserWid 显示头像、昵称、最近消息 |
| **动态加载** | 滚动到底部自动加载更多聊天项 |
| **滚动布局** | 自定义 ChatView 实现自动滚动到底部 |

### 👥 好友模块

| 功能 | 描述 |
|------|------|
| **搜索用户** | 支持 UID/用户名搜索，实时加载动画 |
| **发送申请** | 添加好友时附带标签信息 |
| **标签管理** | 可点选/搜索标签，支持多行展示与折叠 |
| **好友认证** | 同意/拒绝好友申请，支持备注信息 |
| **申请列表** | 展示待处理/已处理的好友申请 |
| **联系人列表** | 按首字母分组展示好友，带红点提示 |
| **好友上下线** | 跨服务器好友状态同步 |

### 🔄 分布式特性

| 特性 | 描述 |
|------|------|
| **负载均衡** | StatusServer 根据 ChatServer 实时连接数分配最优节点 |
| **跨服通信** | ChatServer 之间通过 gRPC 双向通信（连接池） |
| **连接数管理** | Redis 实时记录每个 ChatServer 的连接数 |
| **用户路由** | Redis 存储用户 → 服务器映射，支持跨服消息转发 |
| **Token 验证** | 用户登录生成 Token 存储在 StatusServer 和 Redis 中 |
| **Session 绑定** | 每个用户的 TCP 连接绑定 UID，服务器异常可清理 |

### 📊 性能优化

| 优化项 | 描述 |
|--------|------|
| **ASIO 多线程 IO 池** | 根据 CPU 核数创建多个 IOContext 线程 |
| **gRPC 连接池** | 每个 gRPC 目标维护 5 个长连接，避免频繁建连 |
| **MySQL 连接池** | 生产者-消费者模型管理数据库连接 |
| **Redis 连接池** | 同理 Redis 连接池，支持密码认证 |
| **心跳保活** | 数据库/Redis 连接定期保活 |
| **智能指针管理** | Session、连接等全部使用 shared_ptr 管理生命周期 |

---

## 快速开始

### 1. 环境要求

| 依赖 | 版本 | 备注 |
|------|------|------|
| **Visual Studio** | 2019+ | 用于编译 C++ 服务端 |
| **Qt** | 5.15+ | 客户端开发（建议使用 Qt 5.15.2 MSVC 2019 64bit） |
| **Boost** | 1.81+ | C++ 网络库 |
| **gRPC** | 1.34+ | 服务间通信 |
| **CMake** | 3.20+ | gRPC 编译 |
| **Node.js** | 16+ | VarifyServer |
| **MySQL** | 8.0+ | 数据持久化 |
| **Redis** | 6.x+ | 缓存/会话 |
| **NASM** | - | 编译 gRPC 需要 |
| **Go** | - | 编译 gRPC 可选 |

### 2. 编译 gRPC（Windows）

```bash
# 克隆 gRPC（使用国内镜像）
git clone -b v1.34.0 https://gitee.com/mirrors/grpc-framework.git grpc
cd grpc

# 修改 .gitmodules 中的仓库地址为 gitee 镜像（详见文档）
git submodule update --init

# 使用 CMake 生成 VS 工程
# 打开 CMake GUI，配置源码目录和构建目录
# 选择 VS2019 Win64，点击 Generate 生成项目
# 打开生成的项目，选择 Release x64，编译 ALL_BUILD
```

### 3. 编译 Boost

```bash
# 下载 boost_1_81_0
# 执行 bootstrap.bat 生成 b2.exe
.\b2.exe install --toolset=msvc-14.2 --build-type=complete ^
  --prefix="D:\cppsoft\boost_1_81_0" link=static runtime-link=shared ^
  threading=multi debug release
```

### 4. 编译 jsoncpp

```bash
# 下载 jsoncpp 源码，用 VS 打开 makefiles/msvc2010/jsoncpp.sln
# 升级到 VS2019 工具集
# 编译 Debug 和 Release 版本的 lib_json
# 设置运行库为 MDd (Debug) / MD (Release)
```

### 5. 启动基础设施

```bash
# 启动 Redis
redis-server.exe .\redis.windows.conf --port <端口> --requirepass <密码>

# 启动 MySQL
net start mysql
# 执行数据库初始化脚本创建库、表和存储过程
```

### 6. 启动服务端

```bash
# 1. 启动 VarifyServer（验证码服务）
cd server/VarifyServer
npm install
npm run serve

# 2. 启动 StatusServer（状态服务）
# 在 VS 中打开 StatusServer 工程，编译运行

# 3. 启动 GateServer（网关服务）
# 在 VS 中打开 GateServer 工程，编译运行

# 4. 启动 ChatServer（聊天服务）
# 在 VS 中打开 ChatServer 工程，编译运行
# 可同时启动 ChatServer2、ChatServer3 模拟多节点
```

### 7. 启动客户端

在 Qt Creator 中打开 `client/llfcchat/llfcchat.pro`，编译运行即可。

---

## 开发环境配置

### VS 属性管理器配置

为所有 C++ 服务项目创建统一的属性表（PropertySheet.props），避免重复配置：

| 配置项 | 路径示例 |
|--------|----------|
| **包含目录** | `D:\cppsoft\boost_1_81_0`、`D:\cppsoft\grpc\include`、`D:\cppsoft\grpc\third_party\protobuf\src` 等 |
| **库目录** | `D:\cppsoft\boost_1_81_0\stage\lib`、`D:\cppsoft\grpc\visualpro\Debug` 等 |
| **附加依赖项** | `libprotobufd.lib`、`grpc.lib`、`grpc++.lib`、`gpr.lib`、各 abseil 库、`crypt32.lib` 等 |

### 生成 PB 文件

```bash
# 在 proto 文件目录执行 start.bat 或手动执行：
protoc.exe -I="." --grpc_out="." --plugin=protoc-gen-grpc="grpc_cpp_plugin.exe" message.proto
protoc.exe --cpp_out="." message.proto
```

### 数据库建表（参考教程）

```sql
-- 创建数据库
CREATE DATABASE llfc CHARACTER SET utf8mb4;

-- 用户表
CREATE TABLE `user` (
  `uid` int NOT NULL,
  `name` varchar(255) NOT NULL,
  `email` varchar(255) NOT NULL,
  `pwd` varchar(255) NOT NULL,
  `nick` varchar(255) DEFAULT NULL,
  `desc` varchar(255) DEFAULT NULL,
  `icon` varchar(255) DEFAULT NULL,
  `sex` int DEFAULT NULL,
  PRIMARY KEY (`uid`),
  UNIQUE KEY `name` (`name`),
  UNIQUE KEY `email` (`email`)
) ENGINE=InnoDB;

-- 用户 ID 自增表
CREATE TABLE `user_id` (
  `id` int NOT NULL
) ENGINE=InnoDB;
INSERT INTO `user_id` VALUES (10000);

-- 好友申请表
CREATE TABLE `friend_apply` (
  `id` int NOT NULL AUTO_INCREMENT,
  `from_uid` int NOT NULL,
  `to_uid` int NOT NULL,
  `status` tinyint DEFAULT '0',
  PRIMARY KEY (`id`),
  UNIQUE KEY `unique_apply` (`from_uid`, `to_uid`)
) ENGINE=InnoDB;

-- 好友关系表
CREATE TABLE `friend` (
  `id` int NOT NULL AUTO_INCREMENT,
  `self_id` int NOT NULL,
  `friend_id` int NOT NULL,
  `back` varchar(255) DEFAULT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- 注册存储过程（来自教程）
DELIMITER //
CREATE PROCEDURE `reg_user`(
    IN `new_name` VARCHAR(255), 
    IN `new_email` VARCHAR(255), 
    IN `new_pwd` VARCHAR(255), 
    OUT `result` INT)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION BEGIN ROLLBACK; SET result = -1; END;
    START TRANSACTION;
    IF EXISTS (SELECT 1 FROM `user` WHERE `name` = new_name) THEN
        SET result = 0; COMMIT;
    ELSEIF EXISTS (SELECT 1 FROM `user` WHERE `email` = new_email) THEN
        SET result = 0; COMMIT;
    ELSE
        UPDATE `user_id` SET `id` = `id` + 1;
        SELECT `id` INTO @new_id FROM `user_id`;
        INSERT INTO `user` (`uid`, `name`, `email`, `pwd`) VALUES (@new_id, new_name, new_email, new_pwd);
        SET result = @new_id; COMMIT;
    END IF;
END //
DELIMITER ;
```

---

## 服务部署说明

### 配置文件

每个服务都有独立的 `config.ini` 或 `config.json`，配置格式如下（请替换为自己的实际参数）：

```ini
# GateServer/config.ini（格式参考）
[GateServer]
Port = <端口>
[VarifyServer]
Host = <地址>
Port = <端口>
[StatusServer]
Host = <地址>
Port = <端口>
[Mysql]
Host = <地址>
Port = <端口>
User = <用户名>
Passwd = <密码>
Schema = <数据库名>
[Redis]
Host = <地址>
Port = <端口>
Passwd = <密码>
```

```json
// VarifyServer/config.json（格式参考）
{
    "email": {
      "user": "<你的邮箱地址>",
      "pass": "<SMTP授权码>"
    },
    "redis": {
      "host": "<地址>",
      "port": <端口>,
      "passwd": "<密码>"
    }
}
```

### 分布式部署

| 服务 | 默认端口 | 说明 |
|------|---------|------|
| GateServer | 8080 | HTTP 网关 |
| VarifyServer | 50051 | gRPC 验证码 |
| StatusServer | 50052 | gRPC 状态管理 |
| ChatServer1 | 8090 (TCP) / 50055 (gRPC) | 聊天服务 1 |
| ChatServer2 | 8091 (TCP) / 50056 (gRPC) | 聊天服务 2 |
| Redis | 6380 | 缓存 |
| MySQL | 3308 | 数据库 |

### StatusServer 负载均衡策略

```cpp
ChatServer getChatServer() {
    // 遍历所有 ChatServer
    // 从 Redis 读取每个 ChatServer 的实时连接数 (HGet LOGIN_COUNT)
    // 选择连接数最少的 ChatServer 返回
    // 同时生成唯一 Token 并存储到 Redis 用于后续验证
}
```

---

## 性能测试（教程作者实测数据）

| 测试项 | 结果 |
|--------|------|
| **单节点 TCP 连接上限** | 8000 ~ 20000（取决于机器性能） |
| **10000 连接稳定性** | 未出现丢包和断线，延迟稳定在 10ms 以内 |
| **10ms 延迟下连接数** | 8000+ |
| **多节点分布式** | 可支撑 1W~2W 活跃用户 |

测试方法：客户端多线程定时连接，采用 Ping-Pong 协议验证收发效率。

---

## 面试考察点

> 以下内容整理自学长（恋恋风辰）的 [day30-项目难点和面试技巧.md](./开发文档/day30-项目难点和面试技巧.md)，是教程作者总结的常见面试问题与回答思路。

### 1. 如何用 Asio 实现 TCP 服务？
- 多线程模式：根据 CPU 核数封装 IOContext 连接池
- 异步 async_read / async_write 回调驱动
- Proactor 网络模式
- Session 类管理连接，智能指针保证生命周期
- TLV 消息格式防粘包（2字节 ID + 2字节长度 + JSON 消息体）
- 心跳机制检测连接可用性

### 2. 如何保证服务高可用？
- 分布式架构：拆分 GateServer / ChatServer / StatusServer
- 故障转移：ChatServer 异常后自动分配其他节点
- 数据备份：MySQL 定期备份
- 连接池保活：Redis / MySQL 连接池定期心跳

### 3. 为何封装连接池？
- 多线程共用同一连接不安全
- 连接数随线程数增长不可控
- 生产者-消费者模型管理连接
- RAII 思想自动回收（Defer 模式）

### 4. 用到哪些设计模式？
| 模式 | 应用 |
|------|------|
| **单例模式** | 网络管理器、数据库管理器 |
| **工厂模式** | LogicSystem 注册回调 |
| **代理模式** | MysqlMgr / RedisMgr 封装底层细节 |
| **MVC** | 客户端三层架构（Model-View-Controller） |
| **生产者-消费者** | 线程池、连接池 |
| **RAII** | Defer 回收连接、智能指针管理 Session |
| **观察者** | Qt 信号槽机制 |

### 5. 为什么要设计心跳？
- 检测僵尸连接（客户端崩溃、拔网线等场景）
- 保持连接活性（Redis / MySQL 连接长时间未访问会断开）

---

## 第二季规划

> 以下是教程作者计划在第二季开发的功能，目前尚未实现：

- [x] 资源服务器及断点续传
- [x] 客户端与聊天服务的心跳机制
- [x] 断线重连与踢人操作
- [x] 用户离线数据管理
- [x] 用户信息编辑、头像上传
- [x] 文件/图片/语音消息传输
- [x] 音视频实时通信（WebRTC）

---

## 致谢

- **教程作者**：[恋恋风辰](https://space.bilibili.com/405025224) — 感谢大佬的优质教程
- **视频教程**：[Bilibili 合集](https://www.bilibili.com/video/BV13Z421W7WA/)
- **原作源码**：[Gitee: secondtonone1/llfcchat](https://gitee.com/secondtonone1/llfcchat)
- **作者博客**：[llfc.club](https://llfc.club/)
- **开发文档**：[./开发文档/](./开发文档/) — 跟着教程整理的 30 天开发笔记

本 README 中的项目架构、设计思路、代码示例均来源于恋恋风辰的教程系列，在此致以诚挚的感谢！

---

## License

[MIT](LICENSE)

<p align="center">
  <b>LLFCChatPro</b> — 跟着恋恋风辰学习的 C++ 全栈即时通讯实践
</p>
