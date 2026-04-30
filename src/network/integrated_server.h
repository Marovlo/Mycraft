#pragma once

#include "network/server.h"
#include "network/client_connection.h"

#include <string>
#include <memory>
#include <atomic>

// ============================================================
// IntegratedServer - 单人模式的本地服务器
// 在同一进程中启动 Server 线程，客户端通过 localhost 连接
// 用户体验：主菜单 → 单人游戏 → 选择世界 → 自动启动 → 连接
// ============================================================

class IntegratedServer {
public:
    IntegratedServer();
    ~IntegratedServer();

    // 启动本地服务器并连接
    // worldPath: 存档路径
    // seed: 世界种子
    // playerName: 本地玩家名称
    bool startAndConnect(const std::string& worldPath, int64_t seed,
                         const std::string& playerName = "Player");

    // 停止服务器（保存世界、断开连接、停止线程）
    void stop();

    // 是否正在运行
    bool isRunning() const;

    // 获取客户端连接（用于游戏层通信）
    ClientConnection& getConnection() { return connection_; }
    const ClientConnection& getConnection() const { return connection_; }

    // 获取服务器实例（用于调试/管理）
    Server& getServer() { return server_; }
    const Server& getServer() const { return server_; }

private:
    Server server_;
    ClientConnection connection_;

    // Integrated Server 使用固定的本地端口
    // 使用较高端口避免与其他服务冲突
    static constexpr uint16_t LOCAL_PORT = 25566;
};
