#include "block_sound_map.h"

BlockSoundMap& BlockSoundMap::instance() {
    static BlockSoundMap inst;
    return inst;
}

void BlockSoundMap::init() {
    if (initialized_) return;

    // 从 BlockRegistry 读取每个方块的 soundMaterial
    // 不再硬编码映射 — 新方块只需在 registerBlock() 时设置 soundMaterial 即可
    const auto& reg = BlockRegistry::instance();
    uint16_t count = reg.blockCount();
    for (uint16_t i = 0; i < count && i < MAX_BLOCKS; ++i) {
        materials_[i] = reg.get(i).soundMaterial;
    }

    initialized_ = true;
}

SoundMaterial BlockSoundMap::getMaterial(BlockId id) const {
    if (id >= MAX_BLOCKS) return SoundMaterial::Stone;
    return materials_[id];
}
