#include "network/server.h"
#include "core/debug.h"

#include <iostream>
#include <chrono>
#include <algorithm>

Server::Server() = default;

Server::~Server() {
    stop();
}

// === 生命周期 ===

bool Server::start(const std::string& worldPath, int64_t seed, uint16_t port) {
    if (running_.load()) {
        std::cerr << "[Server] Already running" << std::endl;
        return false;
    }

    worldPath_ = worldPath;
    worldSeed_ = seed;
    port_ = port;

    // 初始化网络
    if (!network_.startServer(port)) {
        return false;
    }

    // 设置网络回调
    network_.setOnClientConnect([this](uint32_t id) {
        onPlayerConnect(id);
    });
    network_.setOnClientDisconnect([this](uint32_t id) {
        onPlayerDisconnect(id);
    });

    // 初始化地形生成器
    terrainGen_ = std::make_unique<OverworldGenerator>(static_cast<int>(seed));

    // 初始化存档管理器
    // worldPath 已经是完整的世界目录（如 saves/MyWorld）
    // 直接使用 worldPath 作为 worldDir_，不再拼接子目录
    {
        // 提取世界名（路径的最后一个组件）
        std::string wName = worldPath;
        auto slash = wName.rfind('/');
        if (slash != std::string::npos) wName = wName.substr(slash + 1);
        auto backslash = wName.rfind('\\');
        if (backslash != std::string::npos) wName = wName.substr(backslash + 1);

        // basePath 是 worldPath 的父目录
        std::string basePath = worldPath;
        auto sep = basePath.rfind('/');
        if (sep != std::string::npos) basePath = basePath.substr(0, sep);
        else basePath = ".";

        saveManager_.setWorld(wName, basePath);
    }

    // 加载世界数据（如果存在）
    // TODO: 从存档加载 totalTicks_、天气等

    running_.store(true);
    std::cout << "[Server] Started on port " << port
              << " (seed=" << seed << ", world=" << worldPath << ")" << std::endl;
    return true;
}

void Server::stop() {
    if (!running_.load()) return;

    running_.store(false);

    // 等待 tick 线程结束
    if (tickThread_.joinable()) {
        tickThread_.join();
    }

    // 保存世界
    std::cout << "[Server] Saving world..." << std::endl;

    // 保存所有在线玩家数据
    {
        std::lock_guard<std::mutex> lock(playersMutex_);
        for (auto& [id, sp] : players_) {
            Player tmpPlayer;
            tmpPlayer.position = sp.position;
            tmpPlayer.yaw = sp.yaw;
            tmpPlayer.pitch = sp.pitch;
            tmpPlayer.hp = sp.health;
            tmpPlayer.hunger = static_cast<int>(sp.hunger);
            tmpPlayer.saturation = sp.saturation;
            tmpPlayer.spawnPoint = sp.position;
            saveManager_.savePlayerByName(sp.name, tmpPlayer, sp.inventory);
        }
    }

    // 保存所有修改过的区块
    int savedChunks = saveManager_.saveAllDirtyChunks(world_);
    if (savedChunks > 0) {
        std::cout << "[Server] Saved " << savedChunks << " modified chunks" << std::endl;
    }
    saveManager_.closeAllRegions();

    // 断开所有客户端
    network_.stopServer();

    players_.clear();
    std::cout << "[Server] Stopped" << std::endl;
}

// === Integrated Server 线程模式 ===

void Server::startThread() {
    if (!running_.load()) {
        std::cerr << "[Server] Cannot start thread: server not running" << std::endl;
        return;
    }
    tickThread_ = std::thread(&Server::tickLoop, this);
}

void Server::stopThread() {
    running_.store(false);
    if (tickThread_.joinable()) {
        tickThread_.join();
    }
}

// === Tick Loop ===

void Server::tickLoop() {
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<double>;

    constexpr double TICK_DURATION = 1.0 / 20.0;  // 50ms per tick

    auto nextTick = Clock::now();
    auto startTime = Clock::now();
    int tickCount = 0;

    VLOG(DebugCat::Server, "Tick thread started");

    while (running_.load()) {
        auto now = Clock::now();

        if (now >= nextTick) {
            auto tickStart = Clock::now();
            try {
                processTick();
            } catch (const std::exception& e) {
                std::fprintf(stderr, "[SRV-TICK] EXCEPTION in processTick: %s\n", e.what());
                break;
            } catch (...) {
                std::fprintf(stderr, "[SRV-TICK] UNKNOWN EXCEPTION in processTick!\n");
                break;
            }
            auto tickEnd = Clock::now();
            double tickMs = std::chrono::duration<double, std::milli>(tickEnd - tickStart).count();
            tickCount++;
            
            if (tickCount <= 10 || tickMs > 20.0 || tickCount % 100 == 0) {
                double elapsed = std::chrono::duration<double>(now - startTime).count();
                VLOG(DebugCat::Server, "tick %d: %.1fms (t=%.1fs, clients=%u)",
                     tickCount, tickMs, elapsed, network_.getClientCount());
            }
            
            nextTick += std::chrono::duration_cast<Clock::duration>(
                Duration(TICK_DURATION));

            // 防止螺旋式死亡：如果落后太多，跳过
            if (Clock::now() - nextTick > std::chrono::milliseconds(500)) {
                nextTick = Clock::now();
            }
        } else {
            // 在等待下一个 tick 期间处理网络事件
            auto sleepMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                nextTick - now).count();
            if (sleepMs > 0) {
                int waitMs = static_cast<int>(std::min<long long>(sleepMs, 10));
                network_.pollEvents(waitMs);
            }
        }
    }
    VLOG(DebugCat::Server, "Tick loop exited after %d ticks (running=%d)", tickCount, running_.load());
}

void Server::tick() {
    processTick();
}

void Server::processTick() {
    // 1. 处理网络包
    network_.pollEvents(0);
    processPackets();

    // 2. 世界逻辑
    tickBlockUpdates();
    tickEntities();
    tickDayNight();
    tickMobSpawning();
    tickFurnaces();

    // 3. 同步实体位置给客户端
    broadcastEntityPositions();

    // 4. 发送区块给需要的玩家
    {
        std::lock_guard<std::mutex> lock(playersMutex_);
        for (auto& [id, player] : players_) {
            sendChunksToPlayer(id);
        }
    }

    // 5. 自动保存
    tickAutoSave();

    // 6. 心跳
    if (totalTicks_ - lastKeepAliveTick_ >= KEEPALIVE_INTERVAL) {
        lastKeepAliveTick_ = totalTicks_;
        PacketBuffer buf;
        buf.writeU64(totalTicks_);
        network_.broadcastToAll(PacketType::S2C_KeepAlive, buf, NetChannel::Reliable);
    }

    totalTicks_++;
}

// === 网络包处理 ===

void Server::processPackets() {
    auto packets = network_.drainPackets();

    for (auto& pkt : packets) {
        PacketBuffer buf(std::move(pkt.data));

        switch (pkt.type) {
            case PacketType::C2S_Login:
                handleLogin(pkt.senderId, buf);
                break;
            case PacketType::C2S_PlayerPosition:
                handlePlayerPosition(pkt.senderId, buf);
                break;
            case PacketType::C2S_PlayerAction:
                handlePlayerAction(pkt.senderId, buf);
                break;
            case PacketType::C2S_BlockDig:
                handleBlockDig(pkt.senderId, buf);
                break;
            case PacketType::C2S_BlockPlace:
                handleBlockPlace(pkt.senderId, buf);
                break;
            case PacketType::C2S_BlockUse:
                handleBlockUse(pkt.senderId, buf);
                break;
            case PacketType::C2S_ChatMessage:
                handleChatMessage(pkt.senderId, buf);
                break;
            case PacketType::C2S_KeepAlive:
                handleKeepAlive(pkt.senderId, buf);
                break;
            case PacketType::C2S_HeldItemChange:
                handleHeldItemChange(pkt.senderId, buf);
                break;
            case PacketType::C2S_InventoryUpdate:
                handleInventoryUpdate(pkt.senderId, buf);
                break;
            case PacketType::C2S_Disconnect:
                handleDisconnect(pkt.senderId);
                break;
            default:
                break;
        }
    }
}

void Server::handleLogin(uint32_t senderId, PacketBuffer& buf) {
    std::string playerName = buf.readString();

    std::cout << "[Server] Player '" << playerName << "' logging in (id=" << senderId << ")" << std::endl;

    // 认证玩家
    network_.setClientAuthenticated(senderId, playerName);

    // 创建服务端玩家状态，尝试从存档加载
    // 注意：在锁外完成 IO（loadPlayerByName），只在写入 players_ 时加锁
    ServerPlayer newPlayer;
    newPlayer.playerId = senderId;
    newPlayer.name = playerName;
    newPlayer.position = glm::vec3(0.0f, 80.0f, 0.0f);  // 默认出生点

    {
        Player tmpPlayer;
        Inventory tmpInventory;
        if (saveManager_.loadPlayerByName(playerName, tmpPlayer, tmpInventory)) {
            newPlayer.position = tmpPlayer.position;
            newPlayer.yaw = tmpPlayer.yaw;
            newPlayer.pitch = tmpPlayer.pitch;
            newPlayer.health = tmpPlayer.hp;
            newPlayer.hunger = static_cast<float>(tmpPlayer.hunger);
            newPlayer.saturation = tmpPlayer.saturation;
            newPlayer.inventory = tmpInventory;
            std::cout << "[Server] Loaded player '" << playerName << "' at ("
                      << newPlayer.position.x << ", " << newPlayer.position.y << ", "
                      << newPlayer.position.z << ")\n";
        } else {
            // 新玩家：计算地形高度作为出生点
            if (terrainGen_) {
                auto* gen = dynamic_cast<OverworldGenerator*>(terrainGen_.get());
                int surfaceY = gen ? gen->getTerrainHeight(0, 0) : 64;
                int spawnY = std::max(surfaceY, 63) + 2;  // +2 确保在地面上方
                newPlayer.position = glm::vec3(0.5f, static_cast<float>(spawnY), 0.5f);
            }
            // 给新玩家初始物品
            newPlayer.inventory.getSlot(0) = {Item::IronPickaxe,  1, 0};
            newPlayer.inventory.getSlot(1) = {Item::WoodenAxe,     1, 0};
            newPlayer.inventory.getSlot(2) = {Item::WoodenShovel,  1, 0};
            newPlayer.inventory.getSlot(3) = {Item::WoodenPickaxe, 1, 0};
            newPlayer.inventory.getSlot(4) = {Item::Torch,         64, 0};
            std::cout << "[Server] New player '" << playerName << "', starting at ("
                      << newPlayer.position.x << ", " << newPlayer.position.y << ", "
                      << newPlayer.position.z << ")\n";
        }
    }

    // 写入 players_ map（加锁）
    {
        std::lock_guard<std::mutex> lock(playersMutex_);
        players_[senderId] = std::move(newPlayer);
    }

    // 发送登录成功（不持锁）
    {
        PacketBuffer resp;
        resp.writeU32(senderId);
        network_.sendToClient(senderId, PacketType::S2C_LoginSuccess, resp, NetChannel::Reliable);
    }

    // 发送世界信息（含玩家位置作为出生点）
    sendWorldInfo(senderId);

    // 发送背包同步（真实数据）
    sendInventorySync(senderId);

    // 通知其他玩家
    broadcastPlayerJoin(senderId);
}

void Server::handlePlayerPosition(uint32_t senderId, PacketBuffer& buf) {
    glm::vec3 pos = buf.readVec3();
    float yaw = buf.readFloat();
    float pitch = buf.readFloat();
    bool onGround = buf.readBool();

    {
        std::lock_guard<std::mutex> lock(playersMutex_);
        auto it = players_.find(senderId);
        if (it == players_.end()) return;

        it->second.position = pos;
        it->second.yaw = yaw;
        it->second.pitch = pitch;
        it->second.onGround = onGround;
    }

    // 广播给其他玩家（在锁外调用，避免死锁）
    broadcastPlayerPosition(senderId);
}

void Server::handlePlayerAction(uint32_t senderId, PacketBuffer& buf) {
    auto actionType = static_cast<PlayerActionType>(buf.readU8());

    // 广播动作给其他玩家
    PacketBuffer broadcast;
    broadcast.writeU32(senderId);
    broadcast.writeU8(static_cast<uint8_t>(actionType));
    network_.broadcastExcept(senderId, PacketType::S2C_PlayerAction, broadcast, NetChannel::Reliable);
}

// 获取或加载区块：先查内存，再查存档，最后用地形生成器生成
// 生成的纯地形区块不标记 modified（不需要保存），只有玩家修改后才标记
Chunk& Server::getOrLoadChunk(int cx, int cz) {
    Chunk* existing = world_.getChunk(cx, cz);
    if (existing) return *existing;

    Chunk& chunk = world_.getOrCreateChunk(cx, cz);

    // 先尝试从存档加载
    if (saveManager_.loadChunk(cx, cz, chunk)) {
        chunk.clearModified();  // 从磁盘加载的，不需要重新保存
        return chunk;
    }

    // 存档中没有，用地形生成器生成
    if (terrainGen_) {
        terrainGen_->generate(chunk);
    }
    chunk.clearModified();  // 纯地形，不需要保存
    return chunk;
}

void Server::handleBlockDig(uint32_t senderId, PacketBuffer& buf) {
    auto action = static_cast<PlayerActionType>(buf.readU8());
    int32_t x = buf.readI32();
    int32_t y = buf.readI32();
    int32_t z = buf.readI32();

    if (action == PlayerActionType::FinishDigging) {
        // 确保区块在内存中（从存档加载或生成）
        getOrLoadChunk(x >> 4, z >> 4);

        // 更新服务器世界状态并广播
        world_.setBlock(x, y, z, 0);  // setBlock 内部会 markModified
        broadcastBlockChange(x, y, z, 0);

        // 验证区块已被标记为 modified
        Chunk* chunk = world_.getChunk(x >> 4, z >> 4);
        std::cout << "[Server] BlockDig at (" << x << "," << y << "," << z
                  << ") chunk=(" << (x>>4) << "," << (z>>4) << ") modified="
                  << (chunk ? chunk->isModified() : -1) << "\n";
    }
}

void Server::handleBlockPlace(uint32_t senderId, PacketBuffer& buf) {
    int32_t x = buf.readI32();
    int32_t y = buf.readI32();
    int32_t z = buf.readI32();
    uint8_t blockId = buf.readU8();

    // 确保区块在内存中（从存档加载或生成）
    getOrLoadChunk(x >> 4, z >> 4);

    // 更新服务器世界状态并广播
    world_.setBlock(x, y, z, blockId);  // setBlock 内部会 markModified
    broadcastBlockChange(x, y, z, blockId);
}

void Server::handleBlockUse(uint32_t senderId, PacketBuffer& buf) {
    int32_t x = buf.readI32();
    int32_t y = buf.readI32();
    int32_t z = buf.readI32();

    // TODO: 处理方块交互（打开箱子、工作台等）
}

void Server::handleChatMessage(uint32_t senderId, PacketBuffer& buf) {
    std::string message = buf.readString();

    std::lock_guard<std::mutex> lock(playersMutex_);
    auto it = players_.find(senderId);
    std::string senderName = (it != players_.end()) ? it->second.name : "Unknown";

    // 广播聊天消息
    PacketBuffer broadcast;
    broadcast.writeString(senderName);
    broadcast.writeString(message);
    network_.broadcastToAll(PacketType::S2C_ChatMessage, broadcast, NetChannel::Reliable);

    std::cout << "[Chat] <" << senderName << "> " << message << std::endl;
}

void Server::handleKeepAlive(uint32_t senderId, PacketBuffer& buf) {
    // 客户端响应了心跳，更新 RTT 等
    (void)senderId;
    (void)buf;
}

void Server::handleHeldItemChange(uint32_t senderId, PacketBuffer& buf) {
    uint8_t slot = buf.readU8();

    std::lock_guard<std::mutex> lock(playersMutex_);
    auto it = players_.find(senderId);
    if (it != players_.end()) {
        it->second.selectedSlot = slot;
    }
}

void Server::handleInventoryUpdate(uint32_t senderId, PacketBuffer& buf) {
    // 客户端上报完整背包状态（拾取物品/挖方块/合成后）
    // 格式：[slotCount: u8][itemId: u16][count: u16][durability: u16] × slotCount
    uint8_t slotCount = buf.readU8();
    if (slotCount != 36) return;

    std::lock_guard<std::mutex> lock(playersMutex_);
    auto it = players_.find(senderId);
    if (it == players_.end()) return;

    Inventory& inv = it->second.inventory;
    for (int i = 0; i < 36; i++) {
        uint16_t itemId     = buf.readU16();
        uint16_t count      = buf.readU16();
        uint16_t durability = buf.readU16();
        ItemStack& slot = inv.getSlot(i);
        slot.id         = static_cast<ItemId>(itemId);
        slot.count      = count;
        slot.durability = durability;
    }
}

void Server::handleDisconnect(uint32_t senderId) {
    onPlayerDisconnect(senderId);
}

// === 世界同步 ===

void Server::sendWorldInfo(uint32_t playerId) {
    PacketBuffer buf;
    buf.writeI64(worldSeed_);
    buf.writeU64(totalTicks_);
    buf.writeU8(0);  // 游戏模式：0=生存

    // 发送出生点 + 朝向
    std::lock_guard<std::mutex> lock(playersMutex_);
    auto it = players_.find(playerId);
    if (it != players_.end()) {
        buf.writeVec3(it->second.position);
        buf.writeFloat(it->second.yaw);
        buf.writeFloat(it->second.pitch);
    } else {
        buf.writeVec3(glm::vec3(0, 80, 0));
        buf.writeFloat(0.0f);
        buf.writeFloat(0.0f);
    }

    network_.sendToClient(playerId, PacketType::S2C_WorldInfo, buf, NetChannel::Reliable);
}

void Server::sendChunksToPlayer(uint32_t playerId) {
    auto it = players_.find(playerId);
    if (it == players_.end()) return;

    ServerPlayer& player = it->second;
    int pcx = static_cast<int>(std::floor(player.position.x)) >> 4;
    int pcz = static_cast<int>(std::floor(player.position.z)) >> 4;

    // 清除超出视距的旧区块记录
    std::vector<uint64_t> toRemove;
    for (uint64_t key : player.loadedChunks) {
        int cx = static_cast<int>(static_cast<uint32_t>(key >> 32));
        int cz = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFF));
        int dx = cx - pcx;
        int dz = cz - pcz;
        if (dx * dx + dz * dz > (player.viewDistance + 1) * (player.viewDistance + 1)) {
            toRemove.push_back(key);
        }
    }
    for (uint64_t key : toRemove) {
        player.loadedChunks.erase(key);
    }

    // 每 tick 最多发送 2 个 modified 区块，避免带宽峰值
    int sentThisTick = 0;
    constexpr int MAX_CHUNKS_PER_TICK = 2;

    for (int dx = -player.viewDistance; dx <= player.viewDistance && sentThisTick < MAX_CHUNKS_PER_TICK; dx++) {
        for (int dz = -player.viewDistance; dz <= player.viewDistance && sentThisTick < MAX_CHUNKS_PER_TICK; dz++) {
            int cx = pcx + dx;
            int cz = pcz + dz;
            uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
                           static_cast<uint64_t>(static_cast<uint32_t>(cz));

            // 检查服务器内存中是否有该区块且已被修改
            Chunk* chunk = world_.getChunk(cx, cz);
            if (!chunk || !chunk->isModified()) {
                // 未修改的区块客户端自己用种子生成，只需记录 AOI
                player.loadedChunks.insert(key);
                continue;
            }

            // 该区块已被修改，需要发送给客户端
            // 用 loadedChunks 记录「已发送 modified 版本」的区块
            // 每次 modified 区块进入视距都发送（重连后 loadedChunks 是空的）
            if (player.loadedChunks.count(key)) continue;  // 本次连接已发过

            // 发送原始方块数据（客户端期望 blockCount 字节的原始数据）
            size_t blockBytes = static_cast<size_t>(Chunk::blockCount()) * sizeof(BlockId);
            PacketBuffer pkt;
            pkt.writeI32(cx);
            pkt.writeI32(cz);
            pkt.writeU32(static_cast<uint32_t>(blockBytes));
            pkt.writeBytes(reinterpret_cast<const uint8_t*>(chunk->blocksData()), blockBytes);
            network_.sendToClient(playerId, PacketType::S2C_ChunkData, pkt, NetChannel::Reliable);

            player.loadedChunks.insert(key);
            sentThisTick++;
        }
    }
}

void Server::broadcastBlockChange(int x, int y, int z, uint8_t blockId) {
    PacketBuffer buf;
    buf.writeI32(x);
    buf.writeI32(y);
    buf.writeI32(z);
    buf.writeU8(blockId);
    network_.broadcastToAll(PacketType::S2C_BlockChange, buf, NetChannel::Reliable);
}

void Server::broadcastEntityPositions() {
    // TODO: 遍历所有实体，发送位置更新给视距内的玩家
    // 使用 AOI（Area of Interest）过滤
}

void Server::broadcastPlayerPosition(uint32_t playerId) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    auto it = players_.find(playerId);
    if (it == players_.end()) return;

    PacketBuffer buf;
    buf.writeU32(playerId);
    buf.writeVec3(it->second.position);
    buf.writeFloat(it->second.yaw);
    buf.writeFloat(it->second.pitch);
    buf.writeBool(it->second.onGround);

    network_.broadcastExcept(playerId, PacketType::S2C_PlayerPosition, buf, NetChannel::Unreliable);
}

void Server::sendInventorySync(uint32_t playerId) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    auto it = players_.find(playerId);
    if (it == players_.end()) return;

    const Inventory& inv = it->second.inventory;
    PacketBuffer buf;
    buf.writeU8(36);  // 36 个槽位
    for (int i = 0; i < 36; i++) {
        const ItemStack& slot = inv.getSlot(i);
        buf.writeU16(static_cast<uint16_t>(slot.id));
        buf.writeU16(static_cast<uint16_t>(slot.count));
        buf.writeU16(static_cast<uint16_t>(slot.durability));
    }
    network_.sendToClient(playerId, PacketType::S2C_InventorySync, buf, NetChannel::Reliable);
}

// === 玩家管理 ===

void Server::onPlayerConnect(uint32_t playerId) {
    std::cout << "[Server] Player " << playerId << " connected" << std::endl;
}

void Server::onPlayerDisconnect(uint32_t playerId) {
    std::cout << "[Server] Player " << playerId << " disconnected" << std::endl;

    broadcastPlayerLeave(playerId);

    // 保存玩家数据
    {
        std::lock_guard<std::mutex> lock(playersMutex_);
        auto it = players_.find(playerId);
        if (it != players_.end()) {
            const ServerPlayer& sp = it->second;
            // 构造 Player 对象用于序列化
            Player tmpPlayer;
            tmpPlayer.position = sp.position;
            tmpPlayer.yaw = sp.yaw;
            tmpPlayer.pitch = sp.pitch;
            tmpPlayer.hp = sp.health;
            tmpPlayer.hunger = static_cast<int>(sp.hunger);
            tmpPlayer.saturation = sp.saturation;
            tmpPlayer.spawnPoint = sp.position;  // 保存当前位置为出生点

            if (saveManager_.savePlayerByName(sp.name, tmpPlayer, sp.inventory)) {
                std::cout << "[Server] Saved player '" << sp.name << "'\n";
            }
            players_.erase(it);
        }
    }

    // 玩家断开时立即保存所有 dirty 区块（不等自动保存间隔）
    // 统计 dirty 区块数量用于调试
    int dirtyCount = 0;
    for (auto& [key, chunk] : world_.chunks()) {
        if (chunk.isModified()) ++dirtyCount;
    }
    std::cout << "[Server] Disconnect: " << dirtyCount << " dirty chunks in memory\n";

    int savedChunks = saveManager_.saveAllDirtyChunks(world_);
    std::cout << "[Server] Saved " << savedChunks << " modified chunks on player disconnect\n";
}

void Server::broadcastPlayerJoin(uint32_t playerId) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    auto it = players_.find(playerId);
    if (it == players_.end()) return;

    PacketBuffer buf;
    buf.writeU32(playerId);
    buf.writeString(it->second.name);
    buf.writeVec3(it->second.position);

    network_.broadcastExcept(playerId, PacketType::S2C_PlayerJoin, buf, NetChannel::Reliable);
}

void Server::broadcastPlayerLeave(uint32_t playerId) {
    PacketBuffer buf;
    buf.writeU32(playerId);
    network_.broadcastToAll(PacketType::S2C_PlayerLeave, buf, NetChannel::Reliable);
}

// === 世界逻辑 ===

void Server::tickBlockUpdates() {
    // 服务端的方块更新系统需要简化调用（无本地玩家引用）
    // TODO: 服务端的 BlockUpdateSystem 需要重构为不依赖 Player 引用
    // blockUpdateSystem_.tick(world_, entityManager_, ???, totalTicks_);
}

void Server::tickEntities() {
    // TODO: 更新所有实体（AI、物理等）
}

void Server::tickDayNight() {
    dayNightCycle_.tick();
}

void Server::tickMobSpawning() {
    // TODO: 生物生成逻辑
}

void Server::tickFurnaces() {
    furnaceManager_.tick();
}

void Server::tickAutoSave() {
    if (totalTicks_ - lastAutoSaveTick_ >= AUTOSAVE_INTERVAL) {
        lastAutoSaveTick_ = totalTicks_;
        std::cout << "[Server] Auto-saving..." << std::endl;

        // 保存所有在线玩家数据
        {
            std::lock_guard<std::mutex> lock(playersMutex_);
            for (auto& [id, sp] : players_) {
                Player tmpPlayer;
                tmpPlayer.position = sp.position;
                tmpPlayer.yaw = sp.yaw;
                tmpPlayer.pitch = sp.pitch;
                tmpPlayer.hp = sp.health;
                tmpPlayer.hunger = static_cast<int>(sp.hunger);
                tmpPlayer.saturation = sp.saturation;
                tmpPlayer.spawnPoint = sp.position;
                saveManager_.savePlayerByName(sp.name, tmpPlayer, sp.inventory);
            }
        }

        // 保存所有修改过的区块
        int savedChunks = saveManager_.saveAllDirtyChunks(world_);
        if (savedChunks > 0) {
            std::cout << "[Server] Auto-saved " << savedChunks << " modified chunks" << std::endl;
        }
    }
}
