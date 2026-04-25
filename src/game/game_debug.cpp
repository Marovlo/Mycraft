// game_debug.cpp — 调试快捷键处理
// 从 game.cpp 的 handleGameplayInput() 中提取。
// F3: FPS 显示切换
// F4: 打印调试状态
// F5: 传送到地面
// F6: 定位群系
// F7: 传送到出生点
// F8: 显示世界存档信息

#include "game.h"
#include <iostream>
#include <cmath>
#include <GLFW/glfw3.h>

void Game::handleDebugKeys() {
    // F2: Screenshot
    if (input_.isKeyPressed(GLFW_KEY_F2)) {
        std::string dir = std::string(ASSET_DIR) + "/../debug_output";
        std::string path = dir + "/screenshot_" + std::to_string(tickClock_.getTotalTicks()) + ".png";
        engine_.requestScreenshot(path);
    }

    // F3: 切换 FPS 显示
    if (input_.isKeyPressed(GLFW_KEY_F3)) {
        showFps_ = !showFps_;
    }

    // F4: Debug state dump
    if (input_.isKeyPressed(GLFW_KEY_F4)) {
        std::cout << "\n=== DEBUG STATE (tick " << tickClock_.getTotalTicks() << ") ===\n";
        std::cout << "Player pos: " << player_.position.x << ", " << player_.position.y << ", " << player_.position.z << "\n";
        std::cout << "Player yaw/pitch: " << player_.yaw << " / " << player_.pitch << "\n";
        std::cout << "Chunk pos: " << playerChunkX_ << ", " << playerChunkZ_ << "\n";
        std::cout << "Loaded chunks: " << world_.chunks().size() << "\n";
        std::cout << "Selected slot: " << inventory_.getSelectedSlot();
        const auto& held = inventory_.getHeldItem();
        if (!held.isEmpty()) {
            std::cout << " -> ItemId " << held.id << " x" << held.count
                      << " (" << ItemRegistry::instance().get(held.id).displayName << ")";
        }
        std::cout << "\nWindow: " << engine_.getWindowWidth() << "x" << engine_.getWindowHeight() << "\n";
        std::cout << "Worker threads: " << chunkTaskMgr_.threadCount() << "\n";
        std::cout << "=== END ===\n\n";
    }

    // F5: Teleport to surface
    if (input_.isKeyPressed(GLFW_KEY_F5)) {
        auto* gen = dynamic_cast<OverworldGenerator*>(terrainGen_.get());
        int px = static_cast<int>(std::floor(player_.position.x));
        int pz = static_cast<int>(std::floor(player_.position.z));
        int surfY = gen ? gen->getTerrainHeight(px, pz) : 80;
        surfY = std::max(surfY, SEA_LEVEL) + 1;
        player_.position.y = static_cast<float>(surfY);
        player_.velocity = glm::vec3(0);
        player_.fallStartY = player_.position.y;
        player_.wasFalling = false;
        std::cout << "[TP] Teleported to surface Y=" << surfY << "\n";
    }

    // F6: Locate biomes
    if (input_.isKeyPressed(GLFW_KEY_F6)) {
        auto* gen = dynamic_cast<OverworldGenerator*>(terrainGen_.get());
        if (gen) {
            int cx = static_cast<int>(std::floor(player_.position.x));
            int cz = static_cast<int>(std::floor(player_.position.z));
            const char* biomeNames[] = {"Plains", "Forest", "Desert", "Snowy"};
            std::cout << "\n=== LOCATE BIOMES (from " << cx << "," << cz << ") ===\n";
            for (int biome = 0; biome < 4; ++biome) {
                bool found = false;
                for (int r = 0; r < 500 && !found; r += 16) {
                    for (int dx = -r; dx <= r && !found; dx += 16) {
                        for (int dz = -r; dz <= r && !found; dz += 16) {
                            if (std::abs(dx) != r && std::abs(dz) != r) continue;
                            auto b = gen->getBiome(cx+dx, cz+dz);
                            if (static_cast<int>(b) == biome) {
                                std::cout << "  " << biomeNames[biome] << ": /tp "
                                          << (cx+dx) << " 100 " << (cz+dz) << "\n";
                                found = true;
                            }
                        }
                    }
                }
                if (!found) std::cout << "  " << biomeNames[biome] << ": not found within 500 blocks\n";
            }
            std::cout << "=== END ===\n\n";
        }
    }

    // F7: Teleport to spawn
    if (input_.isKeyPressed(GLFW_KEY_F7)) {
        player_.position = glm::vec3(0.5f, 100.0f, 0.5f);
        player_.velocity = glm::vec3(0);
        player_.fallStartY = player_.position.y;
        player_.wasFalling = false;
        std::cout << "[TP] Teleported to spawn (0, 100, 0)\n";
    }

    // F8: World save info
    if (input_.isKeyPressed(GLFW_KEY_F8)) {
        std::cout << "\n=== WORLD INFO ===\n";
        std::cout << "World: " << saveManager_.getWorldName() << "\n";
        std::cout << "Seed: " << worldSeed_ << "\n";
        std::cout << "Ticks: " << tickClock_.getTotalTicks() << "\n";
        std::cout << "Save dir: " << saveManager_.getWorldDir() << "\n";
        int modifiedChunks = 0;
        for (const auto& [key, chunk] : world_.chunks()) {
            if (chunk.isModified()) ++modifiedChunks;
        }
        std::cout << "Loaded chunks: " << world_.chunks().size()
                  << " (modified: " << modifiedChunks << ")\n";
        std::cout << "==================\n";
    }
}
