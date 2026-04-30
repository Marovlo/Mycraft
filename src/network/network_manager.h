#pragma once

#include "network/packet_types.h"
#include "network/packet_buffer.h"

#include <enet/enet.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

// ============================================================
// NetworkManager - ENet 网络层封装
// 提供服务端和客户端两种模式
// ============================================================

// 连接的客户端信息（服务端视角）
struct ClientInfo {
    ENetPeer* peer = nullptr;
    uint32_t playerId = 0;
    std::string playerName;
    bool authenticated = false;
};

// 收到的网络包
struct ReceivedPacket {
    PacketType type;
    uint32_t senderId;          // 发送者 playerId（服务端填充）
    std::vector<uint8_t> data;  // 不含 PacketType 头的 payload
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    // === 初始化/销毁 ===
    // 全局 ENet 初始化（程序启动时调用一次）
    static bool initENet();
    static void deinitENet();

    // === 服务端模式 ===
    bool startServer(uint16_t port, int maxClients = MAX_PLAYERS);
    void stopServer();
    bool isServer() const { return isServer_; }

    // === 客户端模式 ===
    bool connectToServer(const std::string& host, uint16_t port);
    void disconnect();
    bool isConnected() const { return isConnected_; }

    // === 收发包 ===
    // 轮询网络事件，将收到的包放入队列（每帧/每tick调用）
    void pollEvents(int timeoutMs = 0);

    // 获取收到的包队列（调用后清空）
    std::vector<ReceivedPacket> drainPackets();

    // 发送包到指定客户端（服务端用）
    void sendToClient(uint32_t playerId, PacketType type,
                      const PacketBuffer& payload, NetChannel channel);

    // 广播包给所有已认证客户端（服务端用）
    void broadcastToAll(PacketType type, const PacketBuffer& payload,
                        NetChannel channel);

    // 广播给除指定玩家外的所有客户端
    void broadcastExcept(uint32_t excludeId, PacketType type,
                         const PacketBuffer& payload, NetChannel channel);

    // 发送包到服务器（客户端用）
    void sendToServer(PacketType type, const PacketBuffer& payload,
                      NetChannel channel);

    // === 客户端管理（服务端用） ===
    uint32_t getClientCount() const;
    const ClientInfo* getClientInfo(uint32_t playerId) const;
    void setClientAuthenticated(uint32_t playerId, const std::string& name);
    void kickClient(uint32_t playerId, const std::string& reason);

    // === 回调 ===
    using ConnectCallback = std::function<void(uint32_t playerId)>;
    using DisconnectCallback = std::function<void(uint32_t playerId)>;

    void setOnClientConnect(ConnectCallback cb) { onConnect_ = std::move(cb); }
    void setOnClientDisconnect(DisconnectCallback cb) { onDisconnect_ = std::move(cb); }

    // 客户端：连接/断开回调
    void setOnConnected(std::function<void()> cb) { onConnected_ = std::move(cb); }
    void setOnDisconnected(std::function<void()> cb) { onDisconnected_ = std::move(cb); }

    // === 统计 ===
    struct NetStats {
        uint64_t bytesSent = 0;
        uint64_t bytesReceived = 0;
        uint64_t packetsSent = 0;
        uint64_t packetsReceived = 0;
        uint32_t rttMs = 0;  // 客户端到服务器的 RTT
    };
    NetStats getStats() const { return stats_; }

private:
    void handleServerEvent(const ENetEvent& event);
    void handleClientEvent(const ENetEvent& event);

    ENetPacket* createENetPacket(PacketType type, const PacketBuffer& payload,
                                 NetChannel channel);

    uint32_t nextPlayerId();

    // ENet 主机
    ENetHost* host_ = nullptr;

    // 客户端模式：到服务器的连接
    ENetPeer* serverPeer_ = nullptr;
    bool isConnected_ = false;

    // 服务端模式
    bool isServer_ = false;
    std::unordered_map<uint32_t, ClientInfo> clients_;  // playerId → ClientInfo
    std::unordered_map<ENetPeer*, uint32_t> peerToId_;  // peer → playerId
    uint32_t nextId_ = 1;

    // 收到的包队列
    std::vector<ReceivedPacket> incomingPackets_;
    mutable std::mutex packetMutex_;

    // 回调
    ConnectCallback onConnect_;
    DisconnectCallback onDisconnect_;
    std::function<void()> onConnected_;
    std::function<void()> onDisconnected_;

    // 统计
    NetStats stats_;

    static bool enetInitialized_;
};
