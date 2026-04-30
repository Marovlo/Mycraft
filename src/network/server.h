#pragma once

#include "network/network_manager.h"
#include "network/packet_types.h"
#include "network/packet_buffer.h"
#include "core/tick_clock.h"
#include "world/world.h"
#include "world/terrain_generator.h"
#include "world/block_update_system.h"
#include "world/save_manager.h"
#include "world/chest_manager.h"
#include "world/furnace_manager.h"
#include "world/day_night_cycle.h"
#include "entity/entity_manager.h"
#include "entity/mob_spawner.h"
#include "player/player.h"
#include "player/inventory.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>

// ============================================================
// Server - 专用服务器 / Integrated Server 核心
// 运行独立的 Tick Loop，管理世界状态
// ============================================================

// 服务端玩家状态
struct ServerPlayer {
    uint32_t playerId = 0;
    std::string name;
    glm::vec3 position{0.0f, 80.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool onGround = true;
    Inventory inventory;
    int selectedSlot = 0;
    float health = 20.0f;
    float hunger = 20.0f;
    float saturation = 5.0f;
    bool isDead = false;

    // 区块加载追踪：已发送给该玩家的区块
    std::unordered_set<uint64_t> loadedChunks;

    // 上次发送位置的区块坐标（用于判断是否需要发送新区块）
    int lastChunkX = 0;
    int lastChunkZ = 0;

    // 视距（以区块为单位）
    int viewDistance = 8;
};

class Server {
public:
    Server();
    ~Server();

    // === 生命周期 ===
    // 启动服务器（指定世界路径和端口）
    bool start(const std::string& worldPath, int64_t seed, uint16_t port = DEFAULT_PORT);

    // 停止服务器（保存世界并关闭）
    void stop();

    // 是否正在运行
    bool isRunning() const { return running_.load(); }

    // === Integrated Server 模式 ===
    // 在独立线程中运行服务器 tick loop
    void startThread();
    void stopThread();

    // === 外部 Tick（用于测试或同步调用） ===
    void tick();

    // 获取服务器端口
    uint16_t getPort() const { return port_; }

    // 获取世界种子
    int64_t getSeed() const { return worldSeed_; }

private:
    // === Tick Loop ===
    void tickLoop();  // 线程主函数

    // 每 tick 执行的逻辑
    void processTick();

    // === 网络包处理 ===
    void processPackets();
    void handleLogin(uint32_t senderId, PacketBuffer& buf);
    void handlePlayerPosition(uint32_t senderId, PacketBuffer& buf);
    void handlePlayerAction(uint32_t senderId, PacketBuffer& buf);
    void handleBlockDig(uint32_t senderId, PacketBuffer& buf);
    void handleBlockPlace(uint32_t senderId, PacketBuffer& buf);
    void handleBlockUse(uint32_t senderId, PacketBuffer& buf);
    void handleChatMessage(uint32_t senderId, PacketBuffer& buf);
    void handleKeepAlive(uint32_t senderId, PacketBuffer& buf);
    void handleHeldItemChange(uint32_t senderId, PacketBuffer& buf);
    void handleDisconnect(uint32_t senderId);

    // === 世界同步 ===
    void sendWorldInfo(uint32_t playerId);
    void sendChunksToPlayer(uint32_t playerId);
    void broadcastBlockChange(int x, int y, int z, uint8_t blockId);
    void broadcastEntityPositions();
    void broadcastPlayerPosition(uint32_t playerId);
    void sendInventorySync(uint32_t playerId);

    // === 玩家管理 ===
    void onPlayerConnect(uint32_t playerId);
    void onPlayerDisconnect(uint32_t playerId);
    void broadcastPlayerJoin(uint32_t playerId);
    void broadcastPlayerLeave(uint32_t playerId);

    // === 世界逻辑 ===
    void tickBlockUpdates();
    void tickEntities();
    void tickDayNight();
    void tickMobSpawning();
    void tickFurnaces();
    void tickAutoSave();

    // === 数据 ===
    NetworkManager network_;
    World world_;
    std::unique_ptr<TerrainGenerator> terrainGen_;
    BlockUpdateSystem blockUpdateSystem_;
    EntityManager entityManager_;
    MobSpawner mobSpawner_;
    DayNightCycle dayNightCycle_;
    SaveManager saveManager_;
    ChestManager chestManager_;
    FurnaceManager furnaceManager_;

    // 玩家状态
    std::unordered_map<uint32_t, ServerPlayer> players_;
    mutable std::mutex playersMutex_;

    // 服务器配置
    uint16_t port_ = DEFAULT_PORT;
    int64_t worldSeed_ = 42;
    std::string worldPath_;
    int maxViewDistance_ = 10;

    // Tick 控制
    std::atomic<bool> running_{false};
    std::thread tickThread_;
    uint64_t totalTicks_ = 0;

    // 自动保存
    static constexpr uint64_t AUTOSAVE_INTERVAL = 6000;  // 5 分钟
    uint64_t lastAutoSaveTick_ = 0;

    // 心跳
    static constexpr uint64_t KEEPALIVE_INTERVAL = 200;  // 10 秒
    uint64_t lastKeepAliveTick_ = 0;
};
