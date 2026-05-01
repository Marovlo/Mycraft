#include "network/client_connection.h"
#include "core/debug.h"

#include <iostream>

ClientConnection::ClientConnection() = default;

ClientConnection::~ClientConnection() {
    if (isConnected()) {
        disconnect();
    }
}

// === 连接管理 ===

bool ClientConnection::connect(const std::string& host, uint16_t port,
                                const std::string& playerName) {
    playerName_ = playerName;

    if (!network_.connectToServer(host, port)) {
        return false;
    }

    // 发送登录包
    PacketBuffer loginBuf;
    loginBuf.writeString(playerName);
    network_.sendToServer(PacketType::C2S_Login, loginBuf, NetChannel::Reliable);

    return true;
}

void ClientConnection::disconnect() {
    if (!isConnected()) return;

    PacketBuffer buf;
    network_.sendToServer(PacketType::C2S_Disconnect, buf, NetChannel::Reliable);
    network_.disconnect();

    loggedIn_ = false;
    localPlayerId_ = 0;
    hasWorldInfo_ = false;
    remotePlayers_.clear();
}

// === 每帧更新 ===

void ClientConnection::update() {
    if (!isConnected()) return;

    network_.pollEvents(0);
    processPackets();

    // 更新远程玩家动画 tick
    for (auto& [id, rp] : remotePlayers_) {
        // 挥臂动画递增
        if (rp.isSwingArm) {
            rp.swingTicks++;
            if (rp.swingTicks >= RemotePlayer::SWING_DURATION) {
                rp.isSwingArm = false;
                rp.swingTicks = 0;
            }
        }
        // 拉弓蓄力递增
        if (rp.isChargingBow) {
            rp.bowChargeTicks++;
        }
        // 吃东西递增
        if (rp.isEating) {
            rp.eatingTicks++;
            if (rp.eatingTicks >= 32) {
                rp.isEating = false;
                rp.eatingTicks = 0;
            }
        }
    }
}

// === 发送操作 ===

void ClientConnection::sendPosition(const glm::vec3& pos, float yaw, float pitch, bool onGround) {
    if (!loggedIn_) return;

    PacketBuffer buf;
    buf.writeVec3(pos);
    buf.writeFloat(yaw);
    buf.writeFloat(pitch);
    buf.writeBool(onGround);
    network_.sendToServer(PacketType::C2S_PlayerPosition, buf, NetChannel::Unreliable);
}

void ClientConnection::sendBlockDig(PlayerActionType action, int x, int y, int z) {
    if (!loggedIn_) return;

    PacketBuffer buf;
    buf.writeU8(static_cast<uint8_t>(action));
    buf.writeI32(x);
    buf.writeI32(y);
    buf.writeI32(z);
    network_.sendToServer(PacketType::C2S_BlockDig, buf, NetChannel::Reliable);
}

void ClientConnection::sendBlockPlace(int x, int y, int z, uint8_t blockId) {
    if (!loggedIn_) return;

    PacketBuffer buf;
    buf.writeI32(x);
    buf.writeI32(y);
    buf.writeI32(z);
    buf.writeU8(blockId);
    network_.sendToServer(PacketType::C2S_BlockPlace, buf, NetChannel::Reliable);
}

void ClientConnection::sendBlockUse(int x, int y, int z) {
    if (!loggedIn_) return;

    PacketBuffer buf;
    buf.writeI32(x);
    buf.writeI32(y);
    buf.writeI32(z);
    network_.sendToServer(PacketType::C2S_BlockUse, buf, NetChannel::Reliable);
}

void ClientConnection::sendPlayerAction(PlayerActionType action) {
    if (!loggedIn_) return;

    PacketBuffer buf;
    buf.writeU8(static_cast<uint8_t>(action));
    network_.sendToServer(PacketType::C2S_PlayerAction, buf, NetChannel::Reliable);
}

void ClientConnection::sendChatMessage(const std::string& message) {
    if (!loggedIn_) return;

    PacketBuffer buf;
    buf.writeString(message);
    network_.sendToServer(PacketType::C2S_ChatMessage, buf, NetChannel::Reliable);
}

void ClientConnection::sendHeldItemChange(uint8_t slot) {
    if (!loggedIn_) return;

    PacketBuffer buf;
    buf.writeU8(slot);
    network_.sendToServer(PacketType::C2S_HeldItemChange, buf, NetChannel::Reliable);
}

// === 获取数据 ===

std::vector<ReceivedChunkData> ClientConnection::drainChunkData() {
    std::lock_guard<std::mutex> lock(chunkMutex_);
    std::vector<ReceivedChunkData> result;
    result.swap(receivedChunks_);
    return result;
}

// === 包处理 ===

void ClientConnection::processPackets() {
    auto packets = network_.drainPackets();

    for (auto& pkt : packets) {
        PacketBuffer buf(std::move(pkt.data));

        switch (pkt.type) {
            case PacketType::S2C_LoginSuccess:
                handleLoginSuccess(buf);
                break;
            case PacketType::S2C_WorldInfo:
                handleWorldInfo(buf);
                break;
            case PacketType::S2C_ChunkData:
                handleChunkData(buf);
                break;
            case PacketType::S2C_BlockChange:
                handleBlockChange(buf);
                break;
            case PacketType::S2C_PlayerPosition:
                handlePlayerPosition(buf);
                break;
            case PacketType::S2C_PlayerJoin:
                handlePlayerJoin(buf);
                break;
            case PacketType::S2C_PlayerLeave:
                handlePlayerLeave(buf);
                break;
            case PacketType::S2C_PlayerAction:
                handlePlayerAction(buf);
                break;
            case PacketType::S2C_SpawnEntity:
                handleEntitySpawn(buf);
                break;
            case PacketType::S2C_EntityPosition:
                handleEntityPosition(buf);
                break;
            case PacketType::S2C_EntityRemove:
                handleEntityRemove(buf);
                break;
            case PacketType::S2C_InventorySync:
                handleInventorySync(buf);
                break;
            case PacketType::S2C_HealthUpdate:
                handleHealthUpdate(buf);
                break;
            case PacketType::S2C_ChatMessage:
                handleChatMessage(buf);
                break;
            case PacketType::S2C_KeepAlive:
                handleKeepAlive(buf);
                break;
            case PacketType::S2C_Disconnect:
                handleDisconnect(buf);
                break;
            case PacketType::S2C_TimeUpdate:
                handleTimeUpdate(buf);
                break;
            default:
                break;
        }
    }
}

void ClientConnection::handleLoginSuccess(PacketBuffer& buf) {
    localPlayerId_ = buf.readU32();
    loggedIn_ = true;
    std::cout << "[Client] Login successful, playerId=" << localPlayerId_ << std::endl;
}

void ClientConnection::handleWorldInfo(PacketBuffer& buf) {
    worldSeed_ = buf.readI64();
    serverTick_ = buf.readU64();
    uint8_t gameMode = buf.readU8();
    spawnPos_ = buf.readVec3();
    hasWorldInfo_ = true;

    std::cout << "[Client] World info: seed=" << worldSeed_
              << " tick=" << serverTick_
              << " mode=" << (int)gameMode
              << " spawn=(" << spawnPos_.x << "," << spawnPos_.y << "," << spawnPos_.z << ")"
              << std::endl;
    (void)gameMode;
}

void ClientConnection::handleChunkData(PacketBuffer& buf) {
    ReceivedChunkData chunk;
    chunk.cx = buf.readI32();
    chunk.cz = buf.readI32();

    uint32_t blockCount = buf.readU32();
    chunk.blocks.resize(blockCount);
    buf.readBytes(chunk.blocks.data(), blockCount);

    std::lock_guard<std::mutex> lock(chunkMutex_);
    receivedChunks_.push_back(std::move(chunk));
}

void ClientConnection::handleBlockChange(PacketBuffer& buf) {
    int x = buf.readI32();
    int y = buf.readI32();
    int z = buf.readI32();
    uint8_t blockId = buf.readU8();

    if (onBlockChange_) {
        onBlockChange_(x, y, z, blockId);
    }
}

void ClientConnection::handlePlayerPosition(PacketBuffer& buf) {
    uint32_t playerId = buf.readU32();
    glm::vec3 pos = buf.readVec3();
    float yaw = buf.readFloat();
    float pitch = buf.readFloat();
    bool onGround = buf.readBool();

    auto& player = remotePlayers_[playerId];
    player.playerId = playerId;
    player.prevPosition = player.position;
    player.prevYaw = player.yaw;
    player.prevPitch = player.pitch;
    player.position = pos;
    player.yaw = yaw;
    player.pitch = pitch;
    player.onGround = onGround;
}

void ClientConnection::handlePlayerJoin(PacketBuffer& buf) {
    uint32_t playerId = buf.readU32();
    std::string name = buf.readString();
    glm::vec3 pos = buf.readVec3();

    RemotePlayer player;
    player.playerId = playerId;
    player.name = name;
    player.position = pos;
    player.prevPosition = pos;
    remotePlayers_[playerId] = player;

    std::cout << "[Client] Player '" << name << "' joined (id=" << playerId << ")" << std::endl;
}

void ClientConnection::handlePlayerLeave(PacketBuffer& buf) {
    uint32_t playerId = buf.readU32();
    auto it = remotePlayers_.find(playerId);
    if (it != remotePlayers_.end()) {
        std::cout << "[Client] Player '" << it->second.name << "' left" << std::endl;
        remotePlayers_.erase(it);
    }
}

void ClientConnection::handlePlayerAction(PacketBuffer& buf) {
    uint32_t playerId = buf.readU32();
    auto actionType = static_cast<PlayerActionType>(buf.readU8());

    auto it = remotePlayers_.find(playerId);
    if (it == remotePlayers_.end()) return;

    RemotePlayer& rp = it->second;

    switch (actionType) {
        case PlayerActionType::SwingArm:
            rp.isSwingArm = true;
            rp.swingTicks = 0;
            break;
        case PlayerActionType::BowDraw:
            rp.isChargingBow = true;
            rp.bowChargeTicks = 0;
            break;
        case PlayerActionType::BowRelease:
            rp.isChargingBow = false;
            rp.bowChargeTicks = 0;
            break;
        case PlayerActionType::EatStart:
            rp.isEating = true;
            rp.eatingTicks = 0;
            break;
        case PlayerActionType::EatFinish:
            rp.isEating = false;
            rp.eatingTicks = 0;
            break;
        case PlayerActionType::StartSneaking:
            rp.isSneaking = true;
            break;
        case PlayerActionType::StopSneaking:
            rp.isSneaking = false;
            break;
        case PlayerActionType::StartSprinting:
            rp.isSprinting = true;
            break;
        case PlayerActionType::StopSprinting:
            rp.isSprinting = false;
            break;
        default:
            break;
    }
}

void ClientConnection::handleEntitySpawn(PacketBuffer& buf) {
    // TODO: 生成远程实体
    (void)buf;
}

void ClientConnection::handleEntityPosition(PacketBuffer& buf) {
    // TODO: 更新远程实体位置
    (void)buf;
}

void ClientConnection::handleEntityRemove(PacketBuffer& buf) {
    // TODO: 移除远程实体
    (void)buf;
}

void ClientConnection::handleInventorySync(PacketBuffer& buf) {
    uint8_t slotCount = buf.readU8();
    if (slotCount != 36) {
        return;
    }

    std::array<std::tuple<uint16_t,uint16_t,uint16_t>, 36> slots;
    for (int i = 0; i < 36; i++) {
        uint16_t itemId     = buf.readU16();
        uint16_t count      = buf.readU16();
        uint16_t durability = buf.readU16();
        slots[i] = {itemId, count, durability};
    }

    // 缓存数据（供 drainInventorySync 使用）
    {
        std::lock_guard<std::mutex> lock(inventoryMutex_);
        pendingInventorySlots_ = slots;
        hasPendingInventorySync_ = true;
    }

    // 如果已注册回调，立即触发
    if (onInventorySync_) {
        onInventorySync_(slots);
    }
}

bool ClientConnection::drainInventorySync(
    std::array<std::tuple<uint16_t,uint16_t,uint16_t>, 36>& slots) {
    std::lock_guard<std::mutex> lock(inventoryMutex_);
    if (!hasPendingInventorySync_) return false;
    slots = pendingInventorySlots_;
    hasPendingInventorySync_ = false;
    return true;
}

void ClientConnection::handleHealthUpdate(PacketBuffer& buf) {
    // TODO: 更新本地玩家生命值/饥饿值
    (void)buf;
}

void ClientConnection::handleChatMessage(PacketBuffer& buf) {
    std::string sender = buf.readString();
    std::string message = buf.readString();

    if (onChat_) {
        onChat_(sender, message);
    }
}

void ClientConnection::handleKeepAlive(PacketBuffer& buf) {
    // 回复心跳
    uint64_t tick = buf.readU64();
    serverTick_ = tick;

    PacketBuffer resp;
    resp.writeU64(tick);
    network_.sendToServer(PacketType::C2S_KeepAlive, resp, NetChannel::Reliable);
}

void ClientConnection::handleDisconnect(PacketBuffer& buf) {
    std::string reason = buf.readString();
    std::cout << "[Client] Disconnected by server: " << reason << std::endl;

    loggedIn_ = false;
    if (onDisconnect_) {
        onDisconnect_(reason);
    }
}

void ClientConnection::handleTimeUpdate(PacketBuffer& buf) {
    serverTick_ = buf.readU64();
}
