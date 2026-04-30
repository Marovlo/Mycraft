#include "network/network_manager.h"
#include "core/debug.h"

#include <iostream>
#include <algorithm>

bool NetworkManager::enetInitialized_ = false;

NetworkManager::NetworkManager() = default;

NetworkManager::~NetworkManager() {
    if (isServer_) {
        stopServer();
    } else if (isConnected_) {
        disconnect();
    }
}

// === 全局 ENet 初始化 ===

bool NetworkManager::initENet() {
    if (enetInitialized_) return true;
    if (enet_initialize() != 0) {
        std::cerr << "[Network] Failed to initialize ENet" << std::endl;
        return false;
    }
    enetInitialized_ = true;
    return true;
}

void NetworkManager::deinitENet() {
    if (enetInitialized_) {
        enet_deinitialize();
        enetInitialized_ = false;
    }
}

// === 服务端模式 ===

bool NetworkManager::startServer(uint16_t port, int maxClients) {
    if (!enetInitialized_) {
        if (!initENet()) return false;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    host_ = enet_host_create(&address,
                             static_cast<size_t>(maxClients),
                             static_cast<size_t>(NetChannel::COUNT),
                             0, 0);  // 不限制带宽
    if (!host_) {
        std::cerr << "[Network] Failed to create server on port " << port << std::endl;
        return false;
    }

    isServer_ = true;
    std::cout << "[Network] Server started on port " << port
              << " (max " << maxClients << " clients)" << std::endl;
    return true;
}

void NetworkManager::stopServer() {
    if (!isServer_ || !host_) return;

    // 断开所有客户端
    for (auto& [id, info] : clients_) {
        if (info.peer) {
            enet_peer_disconnect_now(info.peer, 0);
        }
    }
    clients_.clear();
    peerToId_.clear();

    enet_host_destroy(host_);
    host_ = nullptr;
    isServer_ = false;
    std::cout << "[Network] Server stopped" << std::endl;
}

// === 客户端模式 ===

bool NetworkManager::connectToServer(const std::string& host, uint16_t port) {
    if (!enetInitialized_) {
        if (!initENet()) return false;
    }

    // 创建客户端 host（0 = 不监听端口）
    host_ = enet_host_create(nullptr,
                             1,  // 只连接一个服务器
                             static_cast<size_t>(NetChannel::COUNT),
                             0, 0);
    if (!host_) {
        std::cerr << "[Network] Failed to create client host" << std::endl;
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, host.c_str());
    address.port = port;

    serverPeer_ = enet_host_connect(host_, &address,
                                    static_cast<size_t>(NetChannel::COUNT), 0);
    if (!serverPeer_) {
        std::cerr << "[Network] Failed to initiate connection to "
                  << host << ":" << port << std::endl;
        enet_host_destroy(host_);
        host_ = nullptr;
        return false;
    }

    // 等待连接完成（最多 5 秒）
    ENetEvent event;
    if (enet_host_service(host_, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT) {
        isConnected_ = true;
        std::cout << "[Network] Connected to " << host << ":" << port << std::endl;
        if (onConnected_) onConnected_();
        return true;
    }

    std::cerr << "[Network] Connection to " << host << ":" << port
              << " timed out" << std::endl;
    enet_peer_reset(serverPeer_);
    serverPeer_ = nullptr;
    enet_host_destroy(host_);
    host_ = nullptr;
    return false;
}

void NetworkManager::disconnect() {
    if (!isConnected_ || !serverPeer_) return;

    enet_peer_disconnect(serverPeer_, 0);

    // 等待断开确认（最多 3 秒）
    ENetEvent event;
    bool disconnected = false;
    while (enet_host_service(host_, &event, 3000) > 0) {
        if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            disconnected = true;
            break;
        }
        if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            enet_packet_destroy(event.packet);
        }
    }

    if (!disconnected) {
        enet_peer_reset(serverPeer_);
    }

    serverPeer_ = nullptr;
    isConnected_ = false;

    if (host_) {
        enet_host_destroy(host_);
        host_ = nullptr;
    }

    if (onDisconnected_) onDisconnected_();
    std::cout << "[Network] Disconnected from server" << std::endl;
}

// === 收发包 ===

void NetworkManager::pollEvents(int timeoutMs) {
    if (!host_) return;

    ENetEvent event;
    while (enet_host_service(host_, &event, timeoutMs) > 0) {
        timeoutMs = 0;  // 后续事件不等待

        if (isServer_) {
            handleServerEvent(event);
        } else {
            handleClientEvent(event);
        }
    }
}

std::vector<ReceivedPacket> NetworkManager::drainPackets() {
    std::lock_guard<std::mutex> lock(packetMutex_);
    std::vector<ReceivedPacket> result;
    result.swap(incomingPackets_);
    return result;
}

void NetworkManager::handleServerEvent(const ENetEvent& event) {
    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            uint32_t id = nextPlayerId();
            ClientInfo info;
            info.peer = event.peer;
            info.playerId = id;
            info.authenticated = false;
            clients_[id] = info;
            peerToId_[event.peer] = id;
            event.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(id));

            std::cout << "[Network] Client connected, assigned id=" << id << std::endl;
            if (onConnect_) onConnect_(id);
            break;
        }

        case ENET_EVENT_TYPE_DISCONNECT: {
            auto it = peerToId_.find(event.peer);
            if (it != peerToId_.end()) {
                uint32_t id = it->second;
                std::cout << "[Network] Client " << id << " disconnected" << std::endl;
                if (onDisconnect_) onDisconnect_(id);
                clients_.erase(id);
                peerToId_.erase(it);
            }
            break;
        }

        case ENET_EVENT_TYPE_RECEIVE: {
            if (event.packet->dataLength < PACKET_HEADER_SIZE) {
                enet_packet_destroy(event.packet);
                break;
            }

            ReceivedPacket pkt;
            pkt.type = static_cast<PacketType>(event.packet->data[0]);

            // 查找发送者 ID
            auto it = peerToId_.find(event.peer);
            pkt.senderId = (it != peerToId_.end()) ? it->second : 0;

            // 复制 payload（跳过 PacketType 头）
            size_t payloadSize = event.packet->dataLength - PACKET_HEADER_SIZE;
            if (payloadSize > 0) {
                pkt.data.assign(event.packet->data + PACKET_HEADER_SIZE,
                               event.packet->data + event.packet->dataLength);
            }

            {
                std::lock_guard<std::mutex> lock(packetMutex_);
                incomingPackets_.push_back(std::move(pkt));
            }

            stats_.bytesReceived += event.packet->dataLength;
            stats_.packetsReceived++;

            enet_packet_destroy(event.packet);
            break;
        }

        default:
            break;
    }
}

void NetworkManager::handleClientEvent(const ENetEvent& event) {
    switch (event.type) {
        case ENET_EVENT_TYPE_DISCONNECT: {
            isConnected_ = false;
            serverPeer_ = nullptr;
            std::cout << "[Network] Lost connection to server" << std::endl;
            if (onDisconnected_) onDisconnected_();
            break;
        }

        case ENET_EVENT_TYPE_RECEIVE: {
            if (event.packet->dataLength < PACKET_HEADER_SIZE) {
                enet_packet_destroy(event.packet);
                break;
            }

            ReceivedPacket pkt;
            pkt.type = static_cast<PacketType>(event.packet->data[0]);
            pkt.senderId = 0;  // 来自服务器

            size_t payloadSize = event.packet->dataLength - PACKET_HEADER_SIZE;
            if (payloadSize > 0) {
                pkt.data.assign(event.packet->data + PACKET_HEADER_SIZE,
                               event.packet->data + event.packet->dataLength);
            }

            {
                std::lock_guard<std::mutex> lock(packetMutex_);
                incomingPackets_.push_back(std::move(pkt));
            }

            stats_.bytesReceived += event.packet->dataLength;
            stats_.packetsReceived++;

            enet_packet_destroy(event.packet);
            break;
        }

        default:
            break;
    }
}

// === 发送 ===

ENetPacket* NetworkManager::createENetPacket(PacketType type,
                                              const PacketBuffer& payload,
                                              NetChannel channel) {
    size_t totalSize = PACKET_HEADER_SIZE + payload.size();

    uint32_t flags = 0;
    switch (channel) {
        case NetChannel::Reliable:
            flags = ENET_PACKET_FLAG_RELIABLE;
            break;
        case NetChannel::Unreliable:
            flags = ENET_PACKET_FLAG_UNSEQUENCED;
            break;
        case NetChannel::ChunkData:
            flags = ENET_PACKET_FLAG_RELIABLE;
            break;
        default:
            flags = ENET_PACKET_FLAG_RELIABLE;
            break;
    }

    ENetPacket* packet = enet_packet_create(nullptr, totalSize, flags);
    if (!packet) return nullptr;

    // 写入包头
    packet->data[0] = static_cast<uint8_t>(type);

    // 写入 payload
    if (payload.size() > 0) {
        std::memcpy(packet->data + PACKET_HEADER_SIZE, payload.data(), payload.size());
    }

    return packet;
}

void NetworkManager::sendToClient(uint32_t playerId, PacketType type,
                                   const PacketBuffer& payload, NetChannel channel) {
    if (!isServer_) return;

    auto it = clients_.find(playerId);
    if (it == clients_.end() || !it->second.peer) return;

    ENetPacket* packet = createENetPacket(type, payload, channel);
    if (!packet) return;

    enet_peer_send(it->second.peer, static_cast<uint8_t>(channel), packet);

    stats_.bytesSent += PACKET_HEADER_SIZE + payload.size();
    stats_.packetsSent++;
}

void NetworkManager::broadcastToAll(PacketType type, const PacketBuffer& payload,
                                     NetChannel channel) {
    if (!isServer_) return;

    for (auto& [id, info] : clients_) {
        if (info.authenticated && info.peer) {
            ENetPacket* packet = createENetPacket(type, payload, channel);
            if (packet) {
                enet_peer_send(info.peer, static_cast<uint8_t>(channel), packet);
                stats_.bytesSent += PACKET_HEADER_SIZE + payload.size();
                stats_.packetsSent++;
            }
        }
    }
}

void NetworkManager::broadcastExcept(uint32_t excludeId, PacketType type,
                                      const PacketBuffer& payload, NetChannel channel) {
    if (!isServer_) return;

    for (auto& [id, info] : clients_) {
        if (id != excludeId && info.authenticated && info.peer) {
            ENetPacket* packet = createENetPacket(type, payload, channel);
            if (packet) {
                enet_peer_send(info.peer, static_cast<uint8_t>(channel), packet);
                stats_.bytesSent += PACKET_HEADER_SIZE + payload.size();
                stats_.packetsSent++;
            }
        }
    }
}

void NetworkManager::sendToServer(PacketType type, const PacketBuffer& payload,
                                   NetChannel channel) {
    if (isServer_ || !isConnected_ || !serverPeer_) return;

    ENetPacket* packet = createENetPacket(type, payload, channel);
    if (!packet) return;

    enet_peer_send(serverPeer_, static_cast<uint8_t>(channel), packet);

    stats_.bytesSent += PACKET_HEADER_SIZE + payload.size();
    stats_.packetsSent++;
}

// === 客户端管理 ===

uint32_t NetworkManager::getClientCount() const {
    uint32_t count = 0;
    for (auto& [id, info] : clients_) {
        if (info.authenticated) count++;
    }
    return count;
}

const ClientInfo* NetworkManager::getClientInfo(uint32_t playerId) const {
    auto it = clients_.find(playerId);
    return (it != clients_.end()) ? &it->second : nullptr;
}

void NetworkManager::setClientAuthenticated(uint32_t playerId, const std::string& name) {
    auto it = clients_.find(playerId);
    if (it != clients_.end()) {
        it->second.authenticated = true;
        it->second.playerName = name;
    }
}

void NetworkManager::kickClient(uint32_t playerId, const std::string& reason) {
    auto it = clients_.find(playerId);
    if (it == clients_.end() || !it->second.peer) return;

    // 发送断开原因
    PacketBuffer buf;
    buf.writeString(reason);
    sendToClient(playerId, PacketType::S2C_Disconnect, buf, NetChannel::Reliable);

    // 强制断开
    enet_peer_disconnect_later(it->second.peer, 0);
}

uint32_t NetworkManager::nextPlayerId() {
    return nextId_++;
}
