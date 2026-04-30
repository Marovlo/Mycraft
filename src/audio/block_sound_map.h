#pragma once

#include "sound_engine.h"
#include "core/block.h"

// ========== 方块 → 音效材质映射 ==========
// MC 原版中每种方块都有对应的音效材质，决定了行走、破坏、放置时的音效
// 这个映射在 BlockRegistry 初始化后调用一次即可

class BlockSoundMap {
public:
    static BlockSoundMap& instance();

    // 初始化映射表（在 BlockRegistry::registerDefaults() 之后调用）
    void init();

    // 获取方块对应的音效材质
    SoundMaterial getMaterial(BlockId id) const;

private:
    BlockSoundMap() = default;

    // BlockId → SoundMaterial 映射表
    // 使用固定大小数组，O(1) 查找
    static constexpr int MAX_BLOCKS = 256;
    SoundMaterial materials_[MAX_BLOCKS] = {};
    bool initialized_ = false;
};
