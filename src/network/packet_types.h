#pragma once

#include <cstdint>
#include <cstddef>

// ============================================================
// Mycraft 网络协议 - 包类型定义
// 遵循 MC 原版的 C2S (Client→Server) / S2C (Server→Client) 命名
// ============================================================

enum class PacketType : uint8_t {
    // === 连接管理 ===
    C2S_Login           = 0x01,  // 客户端请求登录（用户名）
    S2C_LoginSuccess    = 0x02,  // 服务端确认登录（分配 playerId）
    S2C_Disconnect      = 0x03,  // 服务端踢出/关闭
    C2S_Disconnect      = 0x04,  // 客户端主动断开

    // === 世界数据 ===
    S2C_WorldInfo       = 0x10,  // 世界种子、时间、游戏模式等
    S2C_ChunkData       = 0x11,  // 完整区块数据
    S2C_BlockChange     = 0x12,  // 单个方块变更
    S2C_MultiBlockChange= 0x13,  // 批量方块变更（同一区块内）
    C2S_RequestChunk    = 0x14,  // 客户端请求区块

    // === 玩家位置/动作 ===
    C2S_PlayerPosition  = 0x20,  // 玩家位置 + 朝向
    S2C_PlayerPosition  = 0x21,  // 广播其他玩家位置
    C2S_PlayerAction    = 0x22,  // 玩家动作（开始挖掘、停止挖掘、跳跃等）
    S2C_PlayerAction    = 0x23,  // 广播其他玩家动作
    S2C_PlayerJoin      = 0x24,  // 新玩家加入通知
    S2C_PlayerLeave     = 0x25,  // 玩家离开通知
    S2C_PlayerTeleport  = 0x26,  // 服务端强制传送（纠正位置）

    // === 实体 ===
    S2C_SpawnEntity     = 0x30,  // 生成实体
    S2C_EntityPosition  = 0x31,  // 实体位置更新（增量）
    S2C_EntityRemove    = 0x32,  // 移除实体
    S2C_EntityAnimation = 0x33,  // 实体动画触发
    S2C_EntityMetadata  = 0x34,  // 实体元数据（生命值等）

    // === 方块交互 ===
    C2S_BlockDig        = 0x40,  // 开始/取消/完成挖掘
    C2S_BlockPlace      = 0x41,  // 放置方块
    C2S_BlockUse        = 0x42,  // 使用方块（右键交互）

    // === 背包/物品 ===
    S2C_InventorySync   = 0x50,  // 同步整个背包
    S2C_SlotChange      = 0x51,  // 单个槽位变更
    C2S_SlotClick       = 0x52,  // 客户端点击槽位操作
    C2S_HeldItemChange  = 0x53,  // 切换手持物品栏位
    C2S_InventoryUpdate = 0x54,  // 客户端上报完整背包状态（拾取/挖方块/合成后）

    // === 生存状态 ===
    S2C_HealthUpdate    = 0x58,  // 生命值/饥饿值/经验同步
    S2C_Respawn         = 0x59,  // 重生

    // === 聊天/命令 ===
    C2S_ChatMessage     = 0x60,  // 发送聊天消息/命令
    S2C_ChatMessage     = 0x61,  // 广播聊天消息

    // === 音效/粒子 ===
    S2C_SoundEffect     = 0x70,  // 播放音效（位置 + 音效ID）
    S2C_ParticleEffect  = 0x71,  // 播放粒子效果

    // === 时间/天气 ===
    S2C_TimeUpdate      = 0x80,  // 世界时间更新
    S2C_WeatherChange   = 0x81,  // 天气变化

    // === 心跳 ===
    C2S_KeepAlive       = 0xFE,  // 客户端心跳响应
    S2C_KeepAlive       = 0xFF,  // 服务端心跳请求
};

// 玩家动作子类型（用于 C2S_PlayerAction / S2C_PlayerAction）
enum class PlayerActionType : uint8_t {
    StartDigging    = 0,
    CancelDigging   = 1,
    FinishDigging   = 2,
    SwingArm        = 3,
    StartSprinting  = 4,
    StopSprinting   = 5,
    StartSneaking   = 6,
    StopSneaking    = 7,
    Jump            = 8,
    EatStart        = 9,
    EatFinish       = 10,
    BowDraw         = 11,
    BowRelease      = 12,
};

// 网络通道定义
enum class NetChannel : uint8_t {
    Reliable    = 0,  // 可靠有序：登录、方块变更、背包、聊天
    Unreliable  = 1,  // 不可靠：位置更新、实体移动
    ChunkData   = 2,  // 可靠无序：区块数据传输
    COUNT       = 3
};

// 包头大小：[PacketType: 1B]
// ENet 自身处理分包和长度，我们只需要 PacketType 作为包头
static constexpr size_t PACKET_HEADER_SIZE = 1;

// 最大玩家数（可配置）
static constexpr int MAX_PLAYERS = 64;

// 默认服务器端口
static constexpr uint16_t DEFAULT_PORT = 25565;
