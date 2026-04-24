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

void BlockRegistry::resolveTextures(const std::unordered_map<std::string, uint16_t>& texNameToId) {
    auto resolve = [&](const std::string& name) -> uint16_t {
        auto it = texNameToId.find(name);
        return (it != texNameToId.end()) ? it->second : 0;
    };

    for (auto& block : blocks_) {
        auto& tn = block.textureNames;
        if (tn.top.empty()) continue;  // Air or no textures
        block.textures.top    = resolve(tn.top);
        block.textures.bottom = resolve(tn.bottom);
        block.textures.north  = resolve(tn.north);
        block.textures.south  = resolve(tn.south);
        block.textures.east   = resolve(tn.east);
        block.textures.west   = resolve(tn.west);
    }
}

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
        .textureNames = BlockFaceTextureNames::topBottom("grass_top", "dirt", "grass_side"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.6f,
    });

    // Block::Dirt (id = 2)
    registerBlock({
        .name = "dirt",
        .displayName = "Dirt",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("dirt"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.5f,
    });

    // Block::Stone (id = 3)
    registerBlock({
        .name = "stone",
        .displayName = "Stone",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("stone"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 1.5f,
    });

    // Block::Sand (id = 4)
    registerBlock({
        .name = "sand",
        .displayName = "Sand",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("sand"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.5f,
    });

    // Block::Wood (id = 5)
    registerBlock({
        .name = "oak_log",
        .displayName = "Oak Log",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::topBottom("oak_log_top", "oak_log_top", "oak_log_side"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 2.0f,
    });

    // Block::Leaves (id = 6)
    registerBlock({
        .name = "oak_leaves",
        .displayName = "Oak Leaves",
        .renderType = BlockRenderType::Foliage,
        .textureNames = BlockFaceTextureNames::uniform("oak_leaves"),
        .isSolid = true,
        .isOpaque = false,
        .hardness = 0.2f,
    });

    // Block::Water (id = 7)
    registerBlock({
        .name = "water",
        .displayName = "Water",
        .renderType = BlockRenderType::Liquid,
        .textureNames = BlockFaceTextureNames::uniform("water_still"),
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
        .textureNames = BlockFaceTextureNames::uniform("cobblestone"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 2.0f,
    });

    // Block::OakPlanks (id = 9)
    registerBlock({
        .name = "oak_planks",
        .displayName = "Oak Planks",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("oak_planks"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 2.0f,
    });

    // Block::Bedrock (id = 10)
    registerBlock({
        .name = "bedrock",
        .displayName = "Bedrock",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("bedrock"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = -1.0f,
    });

    // Block::Gravel (id = 11)
    registerBlock({
        .name = "gravel",
        .displayName = "Gravel",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("gravel"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.6f,
    });
}
