# Phase 06 - 联机系统

## 概述

实现 Minecraft 原版风格的联机系统，采用**专用服务器（Dedicated Server）+ 客户端**架构。
单人模式通过内嵌本地服务器（Integrated Server）实现，保证单人/多人代码路径完全统一。

## 设计目标

1. **架构统一**：单人模式 = 本地 Integrated Server + 本地 Client（MC 原版做法）
2. **可扩展**：架构设计支持从 2 人局域网扩展到大规模在线服务器
3. **低延迟**：使用 UDP（ENet 库），支持可靠/不可靠双通道
4. **弱服务器友好**：区块生成可分摊给客户端（种子同步），服务端只做权威状态管理
5. **用户体验**：单人模式点击路径与 MC 原版一致（主菜单 → 单人游戏 → 选择世界 → 进入），无需手动启动服务器

## 技术选型

### 网络库：ENet

- **协议**：MIT License（无版权问题）
- **特性**：基于 UDP 的可靠传输库，提供连接管理、多通道、分包、排序
- **代码量**：约 3000 行纯 C 代码，轻量易集成
- **跨平台**：支持 Windows/macOS/Linux
- **集成方式**：通过 CMake FetchContent 自动下载

### 通道设计

| 通道 | 类型 | 用途 |
|------|------|------|
| Channel 0 | 可靠有序 | 登录/登出、方块变更、背包操作、聊天、世界元数据 |
| Channel 1 | 不可靠 | 实体位置更新、玩家移动、粒子效果触发 |
| Channel 2 | 可靠无序 | 区块数据传输（大块数据，不需要严格顺序） |

## 架构设计

```
┌─────────────────────────────────────────────────────────┐
│                    单人模式                               │
│  ┌──────────────┐    ┌───────────────────────────────┐  │
│  │   Client     │◄──►│  Integrated Server (同进程)    │  │
│  │  (渲染/输入)  │    │  本地 loopback，零网络延迟     │  │
│  └──────────────┘    └───────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                    多人模式                               │
│  ┌──────────────┐         ┌──────────────────────┐      │
│  │  Client A    │◄──UDP──►│                      │      │
│  └──────────────┘         │   Dedicated Server   │      │
│  ┌──────────────┐         │   (独立进程/机器)     │      │
│  │  Client B    │◄──UDP──►│                      │      │
│  └──────────────┘         │                      │      │
│  ┌──────────────┐         │                      │      │
│  │  Client C    │◄──UDP──►│                      │      │
│  └──────────────┘         └──────────────────────┘      │
└─────────────────────────────────────────────────────────┘
```

### 职责分离

| 模块 | 服务端 | 客户端 |
|------|--------|--------|
| 世界生成 | 发送种子 + 结构位置 | 本地生成区块（减少带宽） |
| 玩家移动 | 验证（可选，防作弊） | 客户端权威 + 本地预测 |
| 方块操作 | 权威确认 | 客户端预测 + 服务端纠正 |
| 实体/AI | 权威运行 | 接收位置 + 插值显示 |
| 战斗判定 | 权威 | 发送攻击请求 |
| 物品/背包 | 权威 | 显示 + 发送操作请求 |
| 渲染/音效 | 无 | 纯客户端 |
| 物理 | 验证 | 本地运行 |

## 协议设计

### Packet 类型

```cpp
enum class PacketType : uint8_t {
    // === 连接管理 ===
    C2S_Login           = 0x01,  // 客户端请求登录
    S2C_LoginSuccess    = 0x02,  // 服务端确认登录
    S2C_Disconnect      = 0x03,  // 服务端断开连接
    C2S_Disconnect      = 0x04,  // 客户端主动断开

    // === 世界数据 ===
    S2C_WorldInfo       = 0x10,  // 世界种子、时间、天气等
    S2C_ChunkData       = 0x11,  // 区块方块数据
    S2C_BlockChange     = 0x12,  // 单个方块变更
    S2C_MultiBlockChange= 0x13,  // 批量方块变更

    // === 玩家 ===
    C2S_PlayerPosition  = 0x20,  // 玩家位置/朝向
    S2C_PlayerPosition  = 0x21,  // 其他玩家位置
    C2S_PlayerAction    = 0x22,  // 玩家动作（挖掘、放置、攻击等）
    S2C_PlayerAction    = 0x23,  // 广播其他玩家动作
    S2C_PlayerJoin      = 0x24,  // 新玩家加入
    S2C_PlayerLeave     = 0x25,  // 玩家离开

    // === 实体 ===
    S2C_SpawnEntity     = 0x30,  // 生成实体
    S2C_EntityPosition  = 0x31,  // 实体位置更新
    S2C_EntityRemove    = 0x32,  // 移除实体
    S2C_EntityAnimation = 0x33,  // 实体动画

    // === 方块交互 ===
    C2S_BlockDig        = 0x40,  // 开始/停止挖掘
    C2S_BlockPlace      = 0x41,  // 放置方块
    C2S_BlockUse        = 0x42,  // 使用方块（打开箱子等）

    // === 背包 ===
    S2C_InventorySync   = 0x50,  // 同步整个背包
    S2C_SlotChange      = 0x51,  // 单个槽位变更
    C2S_SlotClick       = 0x52,  // 客户端点击槽位
    C2S_HeldItemChange  = 0x53,  // 切换手持物品

    // === 聊天 ===
    C2S_ChatMessage     = 0x60,  // 发送聊天消息
    S2C_ChatMessage     = 0x61,  // 广播聊天消息

    // === 心跳 ===
    C2S_KeepAlive       = 0xFE,  // 客户端心跳
    S2C_KeepAlive       = 0xFF,  // 服务端心跳
};
```

### 序列化格式

- 使用紧凑的二进制格式（非 JSON/Protobuf，减少开销）
- 每个包头：`[PacketType: 1B][PayloadLength: 2B][Payload: N bytes]`
- 坐标使用定点数压缩（位置精度 1/4096 格）
- 角度使用单字节表示（256 级精度，约 1.4° 分辨率）

## 带宽优化策略

1. **种子同步生成**：服务端只发种子，客户端本地生成地形
2. **Delta 压缩**：只发送变化的方块，不发整个区块
3. **兴趣管理（AOI）**：只同步玩家视距内的实体
4. **位置量化**：坐标定点数压缩，角度字节表示
5. **批量打包**：每 tick 合并多个小包为一个大包
6. **区块增量**：玩家已有的区块只发送 diff

## 实现步骤

### Step 1: 代码分层准备
- 创建 `src/network/` 目录
- 定义 Packet 枚举和基础序列化工具

### Step 2: ENet 集成
- CMake FetchContent 引入 ENet
- 封装 NetworkManager 类（连接/断开/收发包）

### Step 3: 协议实现
- PacketBuffer（读写工具类）
- 各 Packet 类型的序列化/反序列化

### Step 4: 服务端核心
- ServerGame 类：独立 Tick Loop
- 玩家连接管理
- 世界状态权威管理

### Step 5: 客户端网络层
- ClientNetworkManager：连接服务器、收发包
- 客户端预测（移动）
- 实体插值

### Step 6: Integrated Server
- 单人模式启动本地 Server 线程
- 通过 localhost 连接（或进程内直连优化）
- 用户体验：主菜单 → 单人游戏 → 选择世界 → 自动启动本地服务器 → 连接

### Step 7: 世界同步
- 区块请求/传输
- 方块变更广播
- 种子同步 + 客户端本地生成

### Step 8: 实体同步
- 实体生成/移除/位置更新
- 位置插值（平滑显示）
- 动画同步

### Step 9: 优化
- AOI（Area of Interest）兴趣管理
- Delta 压缩
- 带宽控制与流量整形

## 文件结构

```
src/network/
├── packet_types.h          // PacketType 枚举定义
├── packet_buffer.h         // 二进制读写工具
├── packet_buffer.cpp
├── network_manager.h       // ENet 封装（底层收发）
├── network_manager.cpp
├── server.h                // 服务端主类
├── server.cpp
├── client_connection.h     // 客户端网络连接
├── client_connection.cpp
├── integrated_server.h     // Integrated Server（单人模式）
├── integrated_server.cpp
├── protocol_handler.h      // 协议处理（分发包到具体处理函数）
└── protocol_handler.cpp
```

## 依赖

- **ENet**: MIT License, https://github.com/lsalzman/enet
  - 纯 C 库，约 3000 行代码
  - 跨平台：POSIX sockets (macOS/Linux) + Winsock (Windows)
  - 无外部依赖

## 注意事项

1. 单人模式的用户体验必须与 MC 原版一致：打开游戏 → 点击单人游戏 → 选择世界 → 进入游戏。不需要用户手动启动服务器。
2. Integrated Server 在同一进程中以独立线程运行，通过 localhost UDP 连接（保证代码路径统一）。
3. 初期实现聚焦核心路径（连接、区块同步、玩家移动同步），后续逐步完善战斗/背包等。
4. ENet 通过 CMake FetchContent 自动管理，不需要用户手动安装。
