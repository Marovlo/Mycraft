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
#include <cstdio>
#include <vector>
#include <string>
#include <GLFW/glfw3.h>

void Game::handleDebugKeys() {
    // F2: Screenshot
    if (input_.isKeyPressed(GLFW_KEY_F2)) {
        std::string dir = std::string(ASSET_DIR) + "/../debug_output";
        std::string path = dir + "/screenshot_" + std::to_string(tickClock_.getTotalTicks()) + ".png";
        engine_.requestScreenshot(path);
    }

    // F3: 切换调试屏幕
    if (input_.isKeyPressed(GLFW_KEY_F3)) {
        showDebug_ = !showDebug_;
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

// ========== F3 调试屏幕 ==========
void Game::drawDebugScreen(float screenW, float screenH) {
    int scale = std::clamp(static_cast<int>(screenH / 240.0f), 2, 4);
    float glyphH = 6.0f * scale;
    float lineH = glyphH + 2.0f * scale;  // 行高（含间距）
    float pad = 4.0f * scale;
    float advance = glyphH * 0.6f + glyphH * 0.1f;  // 单字符宽度

    // 收集调试信息
    int px = static_cast<int>(std::floor(player_.position.x));
    int py = static_cast<int>(std::floor(player_.position.y));
    int pz = static_cast<int>(std::floor(player_.position.z));
    int cx = blockToChunk(px);
    int cz = blockToChunk(pz);

    // 群系
    const char* biomeName = "Unknown";
    auto* gen = dynamic_cast<OverworldGenerator*>(terrainGen_.get());
    if (gen) {
        auto biome = gen->getBiome(px, pz);
        switch (biome) {
            case OverworldGenerator::Biome::Plains: biomeName = "Plains"; break;
            case OverworldGenerator::Biome::Forest: biomeName = "Forest"; break;
            case OverworldGenerator::Biome::Desert: biomeName = "Desert"; break;
            case OverworldGenerator::Biome::Snowy:  biomeName = "Snowy";  break;
        }
    }

    // 光照等级
    int footY = static_cast<int>(std::floor(player_.position.y - 0.01f));
    uint8_t skyL = 15, blkL = 0;
    const Chunk* chunk = world_.getChunk(cx, cz);
    if (chunk && footY >= 0 && footY < CHUNK_HEIGHT) {
        int lx = blockToLocal(px);
        int lz = blockToLocal(pz);
        skyL = chunk->getSkyLight(lx, footY, lz);
        blkL = chunk->getBlockLight(lx, footY, lz);
    }

    // 实体统计
    size_t totalEntities = entityManager_.count();
    int mobCount = 0;
    int itemCount = 0;
    for (const auto& e : entityManager_.entities()) {
        if (!e || !e->alive) continue;
        if (e->kind() == EntityKind::Mob) ++mobCount;
        else if (e->kind() == EntityKind::Item) ++itemCount;
    }

    // 昼夜时间
    uint32_t dayTime = dayNightCycle_.getTime();
    uint32_t day = dayNightCycle_.getDay();

    // 朝向
    const char* facing = "North";
    float yaw = std::fmod(player_.yaw, 360.0f);
    if (yaw < 0) yaw += 360.0f;
    if (yaw >= 315.0f || yaw < 45.0f)   facing = "South (+Z)";
    else if (yaw >= 45.0f && yaw < 135.0f)  facing = "West (-X)";
    else if (yaw >= 135.0f && yaw < 225.0f) facing = "North (-Z)";
    else                                      facing = "East (+X)";

    // 构建左侧信息行
    std::vector<std::string> leftLines;
    leftLines.push_back(std::to_string(fps_) + " FPS");
    leftLines.push_back("");
    leftLines.push_back("XYZ: " + std::to_string(px) + " / " + std::to_string(py) + " / " + std::to_string(pz));
    // 精确坐标（保留2位小数）
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Exact: %.2f / %.2f / %.2f",
                      player_.position.x, player_.position.y, player_.position.z);
        leftLines.push_back(std::string(buf));
    }
    leftLines.push_back("Chunk: " + std::to_string(cx) + " / " + std::to_string(cz));
    leftLines.push_back("Facing: " + std::string(facing));
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Yaw: %.1f Pitch: %.1f", player_.yaw, player_.pitch);
        leftLines.push_back(std::string(buf));
    }
    leftLines.push_back("");
    leftLines.push_back("Biome: " + std::string(biomeName));
    leftLines.push_back("Light: sky=" + std::to_string(skyL) + " block=" + std::to_string(blkL));
    leftLines.push_back("Day " + std::to_string(day) + " Time: " + std::to_string(dayTime) +
                         (dayNightCycle_.isDay() ? " (Day)" : " (Night)"));

    // 构建右侧信息行
    std::vector<std::string> rightLines;
    rightLines.push_back("Chunks: " + std::to_string(world_.chunks().size()));
    rightLines.push_back("Entities: " + std::to_string(totalEntities));
    rightLines.push_back("  Mobs: " + std::to_string(mobCount));
    rightLines.push_back("  Items: " + std::to_string(itemCount));
    rightLines.push_back("");
    rightLines.push_back("Render dist: " + std::to_string(RENDER_DISTANCE));
    rightLines.push_back("Seed: " + std::to_string(worldSeed_));

    // 绘制左侧（带半透明背景）
    for (size_t i = 0; i < leftLines.size(); ++i) {
        const auto& line = leftLines[i];
        if (line.empty()) continue;
        float y = pad + i * lineH;
        float bgW = line.size() * advance + pad;
        uiRenderer_.drawRect(0, y, bgW, lineH, glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
        uiRenderer_.drawTextLeft(line, pad * 0.5f, y + 1.0f * scale, glyphH,
                                 glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    }

    // 绘制右侧（右对齐，带半透明背景）
    for (size_t i = 0; i < rightLines.size(); ++i) {
        const auto& line = rightLines[i];
        if (line.empty()) continue;
        float y = pad + i * lineH;
        float textW = line.size() * advance;
        float bgW = textW + pad;
        float bgX = screenW - bgW;
        uiRenderer_.drawRect(bgX, y, bgW, lineH, glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
        uiRenderer_.drawTextLeft(line, bgX + pad * 0.5f, y + 1.0f * scale, glyphH,
                                 glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    }
}
