// ============================================================
// Mycraft 专用服务器（Headless Dedicated Server）
// 无渲染、无窗口，纯命令行运行
// 用法: MycraftServer [--port PORT] [--world PATH] [--seed SEED]
// ============================================================

// stb 实现（headless server 也需要，因为共享代码中有图片加载）
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "network/server.h"
#include "network/network_manager.h"
#include "core/block.h"
#include "core/item.h"
#include "crafting/recipe.h"
#include "crafting/smelting_recipe.h"
#include "entity/mob_entity.h"

#include <iostream>
#include <string>
#include <csignal>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>

static std::atomic<bool> g_running{true};

static void signalHandler(int sig) {
    (void)sig;
    std::cout << "\n[Server] Received shutdown signal, stopping...\n";
    g_running.store(false);
}

static void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options]\n"
              << "Options:\n"
              << "  --port PORT    Server port (default: 25565)\n"
              << "  --world PATH   World save directory (default: server_world)\n"
              << "  --seed SEED    World seed (default: 42)\n"
              << "  --max-players N  Maximum players (default: 64)\n"
              << "  --help         Show this help\n";
}

int main(int argc, char** argv) {
    // 解析命令行参数
    uint16_t port = DEFAULT_PORT;
    std::string worldPath = "server_world";
    int64_t seed = 42;
    int maxPlayers = 64;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--world") == 0 && i + 1 < argc) {
            worldPath = argv[++i];
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = std::stoll(argv[++i]);
        } else if (std::strcmp(argv[i], "--max-players") == 0 && i + 1 < argc) {
            maxPlayers = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 初始化注册表
    BlockRegistry::instance().registerDefaults();
    ItemRegistry::instance().registerDefaults();
    RecipeRegistry::instance().registerDefaults();
    SmeltingRegistry::instance().registerDefaults();
    MobRegistry::instance().registerDefaults();

    // 初始化 ENet
    if (!NetworkManager::initENet()) {
        std::cerr << "[Server] Failed to initialize ENet\n";
        return 1;
    }

    std::cout << "============================================\n";
    std::cout << "  Mycraft Dedicated Server\n";
    std::cout << "============================================\n";
    std::cout << "  Port:        " << port << "\n";
    std::cout << "  World:       " << worldPath << "\n";
    std::cout << "  Seed:        " << seed << "\n";
    std::cout << "  Max Players: " << maxPlayers << "\n";
    std::cout << "============================================\n";

    // 创建并启动服务器
    Server server;
    if (!server.start(worldPath, seed, port)) {
        std::cerr << "[Server] Failed to start server\n";
        NetworkManager::deinitENet();
        return 1;
    }

    std::cout << "[Server] Server is running. Press Ctrl+C to stop.\n";

    // 主循环：20 TPS tick loop
    using Clock = std::chrono::steady_clock;
    using Duration = std::chrono::duration<double>;
    constexpr double TICK_DURATION = 1.0 / 20.0;

    auto nextTick = Clock::now();
    uint64_t tickCount = 0;

    while (g_running.load() && server.isRunning()) {
        auto now = Clock::now();

        if (now >= nextTick) {
            server.tick();
            tickCount++;
            nextTick += std::chrono::duration_cast<Clock::duration>(
                Duration(TICK_DURATION));

            // 防止螺旋式死亡
            if (Clock::now() - nextTick > std::chrono::milliseconds(500)) {
                nextTick = Clock::now();
            }

            // 每 1200 tick（1 分钟）输出状态
            if (tickCount % 1200 == 0) {
                std::cout << "[Server] Tick " << tickCount << " - running\n";
            }
        } else {
            // 休眠到下一个 tick
            auto sleepMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                nextTick - now).count();
            if (sleepMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    std::min<long long>(sleepMs, 10)));
            }
        }
    }

    // 停止服务器
    std::cout << "[Server] Stopping server...\n";
    server.stop();
    NetworkManager::deinitENet();

    std::cout << "[Server] Server stopped after " << tickCount << " ticks.\n";
    return 0;
}
