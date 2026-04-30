#include "game.h"
#include "world/light_engine.h"
#include "audio/sound_engine.h"
#include <cmath>
#include <random>

// ============================================================
// 洞穴环境音效 — MC 原版逻辑
// ============================================================
// MC 原版：每 tick 有 1/6000 的概率尝试触发洞穴音效。
// 触发时，在玩家周围 16 格范围内随机采样方块，
// 如果找到光照等级 ≤ 3 的空气方块，就从该方向播放洞穴音效。
// 因为洞穴内光照低，所以在洞穴中触发概率自然更高。

void Game::tickCaveAmbient() {
    if (player_.dead) return;

    // 使用 tick 数作为随机种子的一部分
    static std::mt19937 rng(42);

    // MC 原版：每 tick 1/6000 概率
    std::uniform_int_distribution<int> triggerDist(0, 5999);
    if (triggerDist(rng) != 0) return;

    // 在玩家周围 16 格范围内随机采样
    glm::vec3 playerPos = player_.position;
    int px = static_cast<int>(std::floor(playerPos.x));
    int py = static_cast<int>(std::floor(playerPos.y));
    int pz = static_cast<int>(std::floor(playerPos.z));

    std::uniform_int_distribution<int> offsetDist(-16, 16);

    // 尝试多次采样（MC 原版尝试 10 次）
    for (int attempt = 0; attempt < 10; ++attempt) {
        int sx = px + offsetDist(rng);
        int sy = py + offsetDist(rng);
        int sz = pz + offsetDist(rng);

        // 边界检查
        if (sy < 0 || sy >= CHUNK_HEIGHT) continue;

        // 检查是否为空气方块
        BlockId block = world_.getBlock(sx, sy, sz);
        if (block != Block::Air) continue;

        // 检查光照等级 ≤ 3
        uint8_t light = LightEngine::getLight(world_, sx, sy, sz);
        if (light > 3) continue;

        // 找到了暗处空气方块 — 播放洞穴音效
        glm::vec3 soundPos(sx + 0.5f, sy + 0.5f, sz + 0.5f);
        getSoundEngine().play(SoundEventId::AmbientCave, soundPos, 0.7f, 1.0f);
        return;  // 每次触发只播放一次
    }
}

// ============================================================
// 天气环境音效 — MC 原版逻辑
// ============================================================
// MC 原版：下雨时播放雨声循环，雷暴时随机播放雷声。
// 当前天气系统未实现，保留接口，不触发。

void Game::tickWeatherAmbient() {
    // 天气系统预留 — 当 isRaining_/isThundering_ 被天气系统设置后自动生效
    if (!isRaining_) return;

    // 雨声：每 100 tick 播放一次（约 5 秒间隔）
    static int rainSoundTimer = 0;
    rainSoundTimer++;
    if (rainSoundTimer >= 100) {
        rainSoundTimer = 0;
        getSoundEngine().play2D(SoundEventId::AmbientRain, 0.3f, 1.0f);
    }

    // 雷声：雷暴时每 tick 1/1000 概率
    if (isThundering_) {
        static std::mt19937 rng(123);
        std::uniform_int_distribution<int> dist(0, 999);
        if (dist(rng) == 0) {
            // 在玩家附近随机位置播放雷声
            std::uniform_real_distribution<float> posDist(-32.0f, 32.0f);
            glm::vec3 thunderPos = player_.position + glm::vec3(posDist(rng), 30.0f, posDist(rng));
            getSoundEngine().play(SoundEventId::AmbientThunder, thunderPos, 1.0f, 1.0f);
        }
    }
}
