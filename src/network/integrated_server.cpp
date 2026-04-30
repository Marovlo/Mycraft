#include "network/integrated_server.h"

#include <iostream>
#include <thread>
#include <chrono>

IntegratedServer::IntegratedServer() = default;

IntegratedServer::~IntegratedServer() {
    stop();
}

bool IntegratedServer::startAndConnect(const std::string& worldPath, int64_t seed,
                                        const std::string& playerName) {
    // 1. 启动本地服务器
    if (!server_.start(worldPath, seed, LOCAL_PORT)) {
        std::cerr << "[IntegratedServer] Failed to start local server" << std::endl;
        return false;
    }

    // 2. 启动服务器 tick 线程
    server_.startThread();

    // 3. 等待服务器就绪（短暂延迟确保端口绑定完成）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 4. 客户端连接到本地服务器
    if (!connection_.connect("127.0.0.1", LOCAL_PORT, playerName)) {
        std::cerr << "[IntegratedServer] Failed to connect to local server" << std::endl;
        server_.stop();
        return false;
    }

    std::cout << "[IntegratedServer] Started and connected (world=" << worldPath
              << ", seed=" << seed << ")" << std::endl;
    return true;
}

void IntegratedServer::stop() {
    // 1. 断开客户端连接
    if (connection_.isConnected()) {
        connection_.disconnect();
    }

    // 2. 停止服务器（会保存世界）
    if (server_.isRunning()) {
        server_.stop();
    }

    std::cout << "[IntegratedServer] Stopped" << std::endl;
}

bool IntegratedServer::isRunning() const {
    return server_.isRunning();
}
