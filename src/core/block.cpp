#include "block.h"

BlockRegistry& BlockRegistry::instance() {
    static BlockRegistry reg;
    return reg;
}

BlockId BlockRegistry::registerBlock(BlockProperties props) {
    BlockId id = static_cast<BlockId>(blocks_.size());
    if (!props.name.empty()) {
        nameToId_[props.name] = id;
    }
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
    using D = BlockProperties::Drop;

    // Block::Air (id = 0)
    registerBlock({
        .name = "air",
        .displayName = "Air",
        .renderType = BlockRenderType::None,
        .isSolid = false,
        .isOpaque = false,
        .hardness = 0.0f,
    });

    // Block::Grass (id = 1) — drops Dirt by default (silk-touch would drop Grass).
    registerBlock({
        .name = "grass_block",
        .displayName = "Grass Block",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::topBottom("grass_top", "dirt", "grass_side"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.6f,
        .requiredToolType = ToolType::Shovel,
        .requiredMiningLevel = 0,
        .requireToolForDrops = false,
        .drops = { D{Item::Dirt, 1, 1} },
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
        .requiredToolType = ToolType::Shovel,
        .drops = { D{Item::Dirt, 1, 1} },
    });

    // Block::Stone (id = 3) — pickaxe required to obtain cobblestone.
    registerBlock({
        .name = "stone",
        .displayName = "Stone",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("stone"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 1.5f,
        .requiredToolType = ToolType::Pickaxe,
        .requiredMiningLevel = 0,
        .requireToolForDrops = true,
        .drops = { D{Item::Cobblestone, 1, 1} },
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
        .requiredToolType = ToolType::Shovel,
        .drops = { D{Item::Sand, 1, 1} },
    });

    // Block::Wood (id = 5) — Oak Log
    registerBlock({
        .name = "oak_log",
        .displayName = "Oak Log",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::topBottom("oak_log_top", "oak_log_top", "oak_log_side"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 2.0f,
        .requiredToolType = ToolType::Axe,
        .drops = { D{Item::OakLog, 1, 1} },
    });

    // Block::Leaves (id = 6) — drops apple (simplified; MC has probability).
    registerBlock({
        .name = "oak_leaves",
        .displayName = "Oak Leaves",
        .renderType = BlockRenderType::Foliage,
        .textureNames = BlockFaceTextureNames::uniform("oak_leaves"),
        .isSolid = true,
        .isOpaque = false,
        .hardness = 0.2f,
        .requiredToolType = ToolType::None,
        .requireToolForDrops = false,
        .drops = { D{Item::Apple, 0, 1} },   // 0-1 apples (for now always 1; RNG later)
    });

    // Block::Water (id = 7) — unbreakable, no drops.
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
        .requiredToolType = ToolType::Pickaxe,
        .requireToolForDrops = true,
        .drops = { D{Item::Cobblestone, 1, 1} },
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
        .requiredToolType = ToolType::Axe,
        .drops = { D{Item::OakPlanks, 1, 1} },
    });

    // Block::Bedrock (id = 10) — unbreakable.
    registerBlock({
        .name = "bedrock",
        .displayName = "Bedrock",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("bedrock"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = -1.0f,
    });

    // Block::Gravel (id = 11) — drops gravel for now (no flint mechanic yet).
    registerBlock({
        .name = "gravel",
        .displayName = "Gravel",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("gravel"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 0.6f,
        .requiredToolType = ToolType::Shovel,
        .drops = { D{Item::Gravel, 1, 1} },
    });

    // Block::CraftingTable (id = 12)
    registerBlock({
        .name = "crafting_table",
        .displayName = "Crafting Table",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::topBottom("crafting_table_top", "oak_planks", "crafting_table_side"),
        .isSolid = true,
        .isOpaque = true,
        .hardness = 2.5f,
        .isInteractable = true,
        .requiredToolType = ToolType::Axe,
        .drops = { D{Item::CraftingTable, 1, 1} },
    });

    // --- Ores (id 13-20) ---
    // Helper for ore registration — all share similar properties.
    auto regOre = [&](const std::string& name, const std::string& display,
                      const std::string& texName, int miningLevel,
                      ItemId dropItem, uint16_t dropMin, uint16_t dropMax) {
        registerBlock({
            .name = name,
            .displayName = display,
            .renderType = BlockRenderType::Opaque,
            .textureNames = BlockFaceTextureNames::uniform(texName),
            .isSolid = true,
            .isOpaque = true,
            .hardness = 3.0f,
            .requiredToolType = ToolType::Pickaxe,
            .requiredMiningLevel = miningLevel,
            .requireToolForDrops = true,
            .drops = { D{dropItem, dropMin, dropMax} },
        });
    };

    regOre("coal_ore",     "Coal Ore",     "coal_ore",     0, Item::Coal,        1, 1);  // 13
    regOre("iron_ore",     "Iron Ore",     "iron_ore",     1, Item::RawIron,     1, 1);  // 14
    regOre("gold_ore",     "Gold Ore",     "gold_ore",     2, Item::RawGold,     1, 1);  // 15
    regOre("diamond_ore",  "Diamond Ore",  "diamond_ore",  2, Item::Diamond,     1, 1);  // 16
    regOre("redstone_ore", "Redstone Ore", "redstone_ore", 2, Item::Redstone,    4, 5);  // 17
    regOre("lapis_ore",    "Lapis Lazuli Ore", "lapis_ore", 1, Item::LapisLazuli, 4, 8); // 18
    regOre("emerald_ore",  "Emerald Ore",  "emerald_ore",  2, Item::Emerald,     1, 1);  // 19
    regOre("copper_ore",   "Copper Ore",   "copper_ore",   1, Item::RawCopper,   2, 3);  // 20

    // Block::Furnace (id = 21)
    registerBlock({
        .name = "furnace",
        .displayName = "Furnace",
        .renderType = BlockRenderType::Opaque,
        .textureNames = {
            "furnace_top", "furnace_top",
            "furnace_front", "furnace_side", "furnace_side", "furnace_side"
        },
        .isSolid = true,
        .isOpaque = true,
        .hardness = 3.5f,
        .isInteractable = true,
        .requiredToolType = ToolType::Pickaxe,
        .drops = { D{Item::Furnace, 1, 1} },
    });

    // --- Vegetation / decoration (id 22-28) ---
    auto regPlant = [&](const std::string& name, const std::string& display,
                        const std::string& texName, ItemId dropItem = Item::None) {
        registerBlock({
            .name = name,
            .displayName = display,
            .renderType = BlockRenderType::Cross,
            .textureNames = BlockFaceTextureNames::uniform(texName),
            .isSolid = false,
            .isOpaque = false,
            .hardness = 0.0f,  // instant break
            .drops = dropItem != Item::None ? std::vector<D>{ D{dropItem, 1, 1} } : std::vector<D>{},
        });
    };

    regPlant("tall_grass",      "Tall Grass",      "tall_grass");                          // 22 — no drop (MC: seeds, not yet)
    regPlant("poppy",           "Poppy",           "poppy",          Item::Poppy);         // 23
    regPlant("dandelion",       "Dandelion",       "dandelion",      Item::Dandelion);     // 24
    regPlant("blue_orchid",     "Blue Orchid",     "blue_orchid",    Item::BlueOrchid);    // 25
    regPlant("brown_mushroom",  "Brown Mushroom",  "brown_mushroom", Item::BrownMushroom); // 26
    regPlant("red_mushroom",    "Red Mushroom",    "red_mushroom",   Item::RedMushroom);   // 27
    regPlant("dead_bush",       "Dead Bush",       "dead_bush",      Item::Stick);         // 28 — MC: drops 0-2 sticks

    // Light sources (id 29)
    registerBlock({
        .name = "torch",
        .displayName = "Torch",
        .renderType = BlockRenderType::Cross,
        .textureNames = BlockFaceTextureNames::uniform("torch"),
        .isSolid = false,
        .isOpaque = false,
        .hardness = 0.0f,
        .lightEmit = 14,
    });

    // --- Biome blocks (id 30-34) ---
    registerBlock({  // 30 Snow
        .name = "snow", .displayName = "Snow",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("snow"),
        .isSolid = true, .isOpaque = true, .hardness = 0.2f,
        .requiredToolType = ToolType::Shovel,
    });
    registerBlock({  // 31 Sandstone
        .name = "sandstone", .displayName = "Sandstone",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::uniform("sandstone"),
        .isSolid = true, .isOpaque = true, .hardness = 0.8f,
        .requiredToolType = ToolType::Pickaxe,
    });
    registerBlock({  // 32 Spruce Log
        .name = "spruce_log", .displayName = "Spruce Log",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::topBottom("spruce_log_top", "spruce_log_top", "spruce_log_side"),
        .isSolid = true, .isOpaque = true, .hardness = 2.0f,
        .requiredToolType = ToolType::Axe,
        .drops = { D{Item::OakLog, 1, 1} },  // drops generic log for now
    });
    registerBlock({  // 33 Spruce Leaves
        .name = "spruce_leaves", .displayName = "Spruce Leaves",
        .renderType = BlockRenderType::Foliage,
        .textureNames = BlockFaceTextureNames::uniform("spruce_leaves"),
        .isSolid = true, .isOpaque = false, .hardness = 0.2f,
    });
    registerBlock({  // 34 Cactus
        .name = "cactus", .displayName = "Cactus",
        .renderType = BlockRenderType::Opaque,
        .textureNames = BlockFaceTextureNames::topBottom("cactus_top", "cactus_top", "cactus_side"),
        .isSolid = true, .isOpaque = true, .hardness = 0.4f,
    });

    // Block::Chest (id = 35)
    registerBlock({
        .name = "chest",
        .displayName = "Chest",
        .renderType = BlockRenderType::Opaque,
        .textureNames = {
            "chest_top", "chest_top",
            "chest_front", "chest_side", "chest_side", "chest_side"
        },
        .isSolid = true,
        .isOpaque = true,
        .hardness = 2.5f,
        .isInteractable = true,
        .requiredToolType = ToolType::Axe,
        .drops = { D{Item::Chest, 1, 1} },
    });
}
