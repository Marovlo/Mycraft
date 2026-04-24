#include "block.h"

BlockRegistry& BlockRegistry::instance() {
    static BlockRegistry reg;
    return reg;
}

BlockId BlockRegistry::registerBlock(BlockProperties props) {
    BlockId id = static_cast<BlockId>(blocks_.size());
    blocks_.push_back(std::move(props));
    return id;
}

// ========== Texture ID convention ==========
// For now, each block type gets a simple sequential texture ID.
// Later this will map to actual texture atlas positions loaded from assets.
// Texture IDs:
//  0 = grass_top, 1 = grass_side, 2 = dirt, 3 = stone,
//  4 = sand, 5 = oak_log_side, 6 = oak_log_top, 7 = leaves,
//  8 = water, 9 = cobblestone, 10 = oak_planks, 11 = bedrock, 12 = gravel

void BlockRegistry::registerDefaults() {
    // Block::Air (id = 0)
    registerBlock({
        .name = "air",
        .displayName = "Air",
        .renderType = BlockRenderType::None,
        .isSolid = false,
        .isOpaque = false,
    });

    // Block::Grass (id = 1)
    registerBlock({
        .name = "grass_block",
        .displayName = "Grass Block",
        .renderType = BlockRenderType::Opaque,
        .textures = BlockFaceTextures::topBottom(0, 2, 1), // top=grass_top, bottom=dirt, side=grass_side
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.6f,
    });

    // Block::Dirt (id = 2)
    registerBlock({
        .name = "dirt",
        .displayName = "Dirt",
        .renderType = BlockRenderType::Opaque,
        .textures = BlockFaceTextures::uniform(2),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.5f,
    });

    // Block::Stone (id = 3)
    registerBlock({
        .name = "stone",
        .displayName = "Stone",
        .renderType = BlockRenderType::Opaque,
        .textures = BlockFaceTextures::uniform(3),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 1.5f,
    });

    // Block::Sand (id = 4)
    registerBlock({
        .name = "sand",
        .displayName = "Sand",
        .renderType = BlockRenderType::Opaque,
        .textures = BlockFaceTextures::uniform(4),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.5f,
    });

    // Block::Wood (id = 5)
    registerBlock({
        .name = "oak_log",
        .displayName = "Oak Log",
        .renderType = BlockRenderType::Opaque,
        .textures = BlockFaceTextures::topBottom(6, 6, 5), // top/bottom=log_top, sides=log_side
        .isSolid = true,
        .isOpaque = true,
        .hardness = 2.0f,
    });

    // Block::Leaves (id = 6)
    registerBlock({
        .name = "oak_leaves",
        .displayName = "Oak Leaves",
        .renderType = BlockRenderType::Foliage,
        .textures = BlockFaceTextures::uniform(7),
        .isSolid = true,
        .isOpaque = false,  // Leaves don't fully occlude
        .hardness = 0.2f,
    });

    // Block::Water (id = 7)
    registerBlock({
        .name = "water",
        .displayName = "Water",
        .renderType = BlockRenderType::Liquid,
        .textures = BlockFaceTextures::uniform(8),
        .isSolid = false,
        .isOpaque = false,
        .isLiquid = true,
        .hardness = -1.0f,
    });

    // Block::Cobblestone (id = 8)
    registerBlock({
        .name = "cobblestone",
        .displayName = "Cobblestone",
        .renderType = BlockRenderType::Opaque,
        .textures = BlockFaceTextures::uniform(9),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 2.0f,
    });

    // Block::OakPlanks (id = 9)
    registerBlock({
        .name = "oak_planks",
        .displayName = "Oak Planks",
        .renderType = BlockRenderType::Opaque,
        .textures = BlockFaceTextures::uniform(10),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 2.0f,
    });

    // Block::Bedrock (id = 10)
    registerBlock({
        .name = "bedrock",
        .displayName = "Bedrock",
        .renderType = BlockRenderType::Opaque,
        .textures = BlockFaceTextures::uniform(11),
        .isSolid = true,
        .isOpaque = true,
        .hardness = -1.0f,  // Unbreakable
    });

    // Block::Gravel (id = 11)
    registerBlock({
        .name = "gravel",
        .displayName = "Gravel",
        .renderType = BlockRenderType::Opaque,
        .textures = BlockFaceTextures::uniform(12),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.6f,
    });
}
