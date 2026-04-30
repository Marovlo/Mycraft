#include "block_sound_map.h"

BlockSoundMap& BlockSoundMap::instance() {
    static BlockSoundMap inst;
    return inst;
}

void BlockSoundMap::init() {
    if (initialized_) return;

    // 默认所有方块使用石头音效
    for (int i = 0; i < MAX_BLOCKS; ++i) {
        materials_[i] = SoundMaterial::Stone;
    }

    // Air 无音效（不会被播放，但保持一致性）
    materials_[Block::Air] = SoundMaterial::Stone;

    // 草方块、泥土 → Grass
    materials_[Block::Grass] = SoundMaterial::Grass;
    materials_[Block::Dirt]  = SoundMaterial::Grass; // MC 原版泥土也是 grass 音效

    // 石头类 → Stone
    materials_[Block::Stone]       = SoundMaterial::Stone;
    materials_[Block::Cobblestone] = SoundMaterial::Stone;
    materials_[Block::Bedrock]     = SoundMaterial::Stone;
    materials_[Block::CoalOre]     = SoundMaterial::Stone;
    materials_[Block::IronOre]     = SoundMaterial::Stone;
    materials_[Block::GoldOre]     = SoundMaterial::Stone;
    materials_[Block::DiamondOre]  = SoundMaterial::Stone;
    materials_[Block::RedstoneOre] = SoundMaterial::Stone;
    materials_[Block::LapisOre]    = SoundMaterial::Stone;
    materials_[Block::EmeraldOre]  = SoundMaterial::Stone;
    materials_[Block::CopperOre]   = SoundMaterial::Stone;
    materials_[Block::Furnace]     = SoundMaterial::Stone;
    materials_[Block::Sandstone]   = SoundMaterial::Stone;

    // 沙子 → Sand
    materials_[Block::Sand] = SoundMaterial::Sand;

    // 木头类 → Wood
    materials_[Block::Wood]          = SoundMaterial::Wood;
    materials_[Block::OakPlanks]     = SoundMaterial::Wood;
    materials_[Block::CraftingTable] = SoundMaterial::Wood;
    materials_[Block::Chest]         = SoundMaterial::Wood;
    materials_[Block::SpruceLog]     = SoundMaterial::Wood;

    // 树叶 → Grass（MC 原版树叶用 grass 音效）
    materials_[Block::Leaves]       = SoundMaterial::Grass;
    materials_[Block::SpruceLeaves] = SoundMaterial::Grass;

    // 沙砾 → Gravel
    materials_[Block::Gravel] = SoundMaterial::Gravel;

    // 雪 → Snow
    materials_[Block::Snow] = SoundMaterial::Snow;

    // 羊毛 → Cloth
    materials_[Block::WhiteWool] = SoundMaterial::Cloth;

    // 植物类（非固体，但破坏时有音效）→ Grass
    materials_[Block::TallGrass]     = SoundMaterial::Grass;
    materials_[Block::Poppy]         = SoundMaterial::Grass;
    materials_[Block::Dandelion]     = SoundMaterial::Grass;
    materials_[Block::BlueOrchid]    = SoundMaterial::Grass;
    materials_[Block::BrownMushroom] = SoundMaterial::Grass;
    materials_[Block::RedMushroom]   = SoundMaterial::Grass;
    materials_[Block::DeadBush]      = SoundMaterial::Grass;

    // 仙人掌 → Cloth（MC 原版仙人掌用 cloth 音效）
    materials_[Block::Cactus] = SoundMaterial::Cloth;

    // 火把 → Wood
    materials_[Block::Torch] = SoundMaterial::Wood;

    // 水 → 无（水有自己的环境音效，不走方块音效系统）
    // materials_[Block::Water] 保持默认 Stone，但水不会被破坏/放置

    initialized_ = true;
}

SoundMaterial BlockSoundMap::getMaterial(BlockId id) const {
    if (id >= MAX_BLOCKS) return SoundMaterial::Stone;
    return materials_[id];
}
