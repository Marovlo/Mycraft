#pragma once

#include "network/network_manager.h"
#include "network/packet_types.h"
#include "network/packet_buffer.h"

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <glm/glm.hpp>

// ============================================================
// ClientConnection - 客户端网络连接层
// 负责与服务器通信、处理收到的包、提供游戏层接口
// ============================================================

// 其他玩家的状态（用于渲染）
struct RemotePlayer {
    uint32_t playerId = 0;
    std::string name;
    glm::vec3 position{0.0f};
    glm::vec3 prevPosition{0.0f};  // 用于插值
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool onGround = true;
    double lastUpdateTime = 0.0;

    // === 动作状态（用于第三人称动画渲染） ===
    bool isSwingArm = false;       // 挥臂动画（攻击/挖掘）
    int  swingTicks = 0;           // 挥臂动画进度
    static constexpr int SWING_DURATION = 6;  // 挥臂动画持续 tick 数

    bool isChargingBow = false;    // 正在拉弓
    int  bowChargeTicks = 0;       // 拉弓蓄力 tick 数

    bool isEating = false;         // 正在吃东西
    int  eatingTicks = 0;          // 吃东西进度

    bool isSneaking = false;       // 潜行
    bool isSprinting = false;      // 冲刺
};

// 区块数据（从服务器接收）
struct ReceivedChunkData {
    int cx, cz;
    std::vector<uint8_t> blocks;
};

class ClientConnection {
public:
    ClientConnection();
    ~ClientConnection();

    // === 连接管理 ===
    bool connect(const std::string& host, uint16_t port, const std::string& playerName);
    void disconnect();
    bool isConnected() const { return network_.isConnected(); }
    bool isLoggedIn() const { return loggedIn_; }

    // === 每帧调用 ===
    void update();  // 轮询网络事件并处理包

    // === 发送操作到服务器 ===
    void sendPosition(const glm::vec3& pos, float yaw, float pitch, bool onGround);
    void sendBlockDig(PlayerActionType action, int x, int y, int z);
    void sendBlockPlace(int x, int y, int z, uint8_t blockId);
    void sendBlockUse(int x, int y, int z);
    void sendPlayerAction(PlayerActionType action);
    void sendChatMessage(const std::string& message);
    void sendHeldItemChange(uint8_t slot);

    // === 获取服务器数据 ===
    uint32_t getLocalPlayerId() const { return localPlayerId_; }
    int64_t getWorldSeed() const { return worldSeed_; }
    uint64_t getServerTick() const { return serverTick_; }
    // 是否已经收到 WorldInfo 包（包含 seed / spawn 等关键信息）
    bool hasWorldInfo() const { return hasWorldInfo_; }
    const glm::vec3& getSpawnPosition() const { return spawnPos_; }

    // 获取并清空接收到的区块数据
    std::vector<ReceivedChunkData> drainChunkData();

    // 获取远程玩家列表
    const std::unordered_map<uint32_t, RemotePlayer>& getRemotePlayers() const {
        return remotePlayers_;
    }

    // === 回调 ===
    using BlockChangeCallback = std::function<void(int x, int y, int z, uint8_t blockId)>;
    using ChatCallback = std::function<void(const std::string& sender, const std::string& msg)>;
    using DisconnectCallback = std::function<void(const std::string& reason)>;

    void setOnBlockChange(BlockChangeCallback cb) { onBlockChange_ = std::move(cb); }
    void setOnChat(ChatCallback cb) { onChat_ = std::move(cb); }
    void setOnDisconnect(DisconnectCallback cb) { onDisconnect_ = std::move(cb); }

private:
    void processPackets();
    void handleLoginSuccess(PacketBuffer& buf);
    void handleWorldInfo(PacketBuffer& buf);
    void handleChunkData(PacketBuffer& buf);
    void handleBlockChange(PacketBuffer& buf);
    void handlePlayerPosition(PacketBuffer& buf);
    void handlePlayerJoin(PacketBuffer& buf);
    void handlePlayerLeave(PacketBuffer& buf);
    void handlePlayerAction(PacketBuffer& buf);
    void handleEntitySpawn(PacketBuffer& buf);
    void handleEntityPosition(PacketBuffer& buf);
    void handleEntityRemove(PacketBuffer& buf);
    void handleInventorySync(PacketBuffer& buf);
    void handleHealthUpdate(PacketBuffer& buf);
    void handleChatMessage(PacketBuffer& buf);
    void handleKeepAlive(PacketBuffer& buf);
    void handleDisconnect(PacketBuffer& buf);
    void handleTimeUpdate(PacketBuffer& buf);

    NetworkManager network_;
    std::string playerName_;

    // 登录状态
    bool loggedIn_ = false;
    uint32_t localPlayerId_ = 0;

    // 世界信息
    int64_t worldSeed_ = 0;
    uint64_t serverTick_ = 0;
    bool hasWorldInfo_ = false;
    glm::vec3 spawnPos_{0.0f, 80.0f, 0.0f};

    // 接收到的区块数据队列
    std::vector<ReceivedChunkData> receivedChunks_;
    std::mutex chunkMutex_;

    // 远程玩家
    std::unordered_map<uint32_t, RemotePlayer> remotePlayers_;

    // 回调
    BlockChangeCallback onBlockChange_;
    ChatCallback onChat_;
    DisconnectCallback onDisconnect_;
};
