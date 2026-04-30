#pragma once

#include <cstdint>

// ========== 音效材质类型（MC 原版方块音效分组） ==========
// 每种材质对应一组 dig/step/place 音效
// 独立头文件，避免 block.h ↔ sound_engine.h 循环依赖
enum class SoundMaterial : uint8_t {
    Stone,      // 石头、矿石、砖块等
    Wood,       // 木头、木板
    Gravel,     // 沙砾
    Grass,      // 草方块、泥土
    Sand,       // 沙子
    Snow,       // 雪
    Cloth,      // 羊毛
    Glass,      // 玻璃（只有破坏音效）
    Coral,      // 珊瑚
    WetGrass,   // 湿草
    Metal,      // 金属方块
    Count
};
