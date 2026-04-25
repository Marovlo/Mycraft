#include "item.h"
#include "block.h"

ItemRegistry& ItemRegistry::instance() {
    static ItemRegistry reg;
    return reg;
}

ItemId ItemRegistry::registerItem(ItemProperties props) {
    ItemId id = static_cast<ItemId>(items_.size());
    if (!props.name.empty()) {
        nameToId_[props.name] = id;
    }
    items_.push_back(std::move(props));
    return id;
}

uint16_t ItemStack::merge(const ItemStack& other, uint16_t maxStack) {
    if (isEmpty()) {
        *this = other;
        if (count > maxStack) {
            uint16_t leftover = count - maxStack;
            count = maxStack;
            return leftover;
        }
        return 0;
    }
    if (id != other.id) return other.count;  // Can't merge different items

    uint16_t space = maxStack - count;
    uint16_t toAdd = std::min(space, other.count);
    count += toAdd;
    return other.count - toAdd;
}

bool ItemStack::useDurability(uint16_t amount, uint16_t maxDurability) {
    if (isEmpty() || maxDurability == 0) return false;
    // ItemStack::durability is the *damage* counter (0 = pristine, max = broken),
    // matching MC NBT semantics. When damage >= max, the tool breaks.
    durability += amount;
    if (durability >= maxDurability) {
        clear();
        return true;
    }
    return false;
}

void ItemRegistry::registerDefaults() {
    // Item::None (id = 0) — empty placeholder
    registerItem({
        .name = "none",
        .displayName = "",
        .maxStackSize = 0,
    });

    // Block items (id 1-11, matching BlockId 1-11)
    auto regBlockItem = [this](const std::string& name, const std::string& display, uint16_t blockId) {
        registerItem({
            .name = name,
            .displayName = display,
            .type = ItemType::Block,
            .maxStackSize = 64,
            .blockId = blockId,
        });
    };

    regBlockItem("grass_block", "Grass Block", Block::Grass);       // 1
    regBlockItem("dirt", "Dirt", Block::Dirt);                       // 2
    regBlockItem("cobblestone", "Cobblestone", Block::Cobblestone);  // 3 (stone drops cobblestone)
    regBlockItem("sand", "Sand", Block::Sand);                       // 4
    regBlockItem("oak_log", "Oak Log", Block::Wood);                 // 5
    regBlockItem("oak_leaves", "Oak Leaves", Block::Leaves);         // 6
    regBlockItem("oak_planks", "Oak Planks", Block::OakPlanks);      // 7
    regBlockItem("bedrock", "Bedrock", Block::Bedrock);              // 8
    regBlockItem("gravel", "Gravel", Block::Gravel);                 // 9
    regBlockItem("stone", "Stone", Block::Stone);                    // 10

    // Material items
    registerItem({  // 11
        .name = "stick",
        .displayName = "Stick",
        .type = ItemType::Material,
        .maxStackSize = 64,
        .iconTileName = "item_stick",
    });

    // Wooden tools
    auto regTool = [this](const std::string& name, const std::string& display,
                          ToolType tool, float speed, int level, uint16_t dur, float atk,
                          const std::string& iconName) {
        registerItem({
            .name = name,
            .displayName = display,
            .type = (tool == ToolType::Sword) ? ItemType::Weapon : ItemType::Tool,
            .maxStackSize = 1,
            .toolType = tool,
            .miningSpeed = speed,
            .miningLevel = level,
            .durability = dur,
            .attackDamage = atk,
            .iconTileName = iconName,
        });
    };

    regTool("wooden_pickaxe", "Wooden Pickaxe", ToolType::Pickaxe, 2.0f, 0, 59, 2,    "tool_wooden_pickaxe");
    regTool("wooden_axe", "Wooden Axe", ToolType::Axe, 2.0f, 0, 59, 7,                 "tool_wooden_axe");
    regTool("wooden_shovel", "Wooden Shovel", ToolType::Shovel, 2.0f, 0, 59, 2.5f,     "tool_wooden_shovel");
    regTool("wooden_sword", "Wooden Sword", ToolType::Sword, 1.0f, 0, 59, 4,            "tool_wooden_sword");
    regTool("wooden_hoe", "Wooden Hoe", ToolType::Hoe, 1.0f, 0, 59, 1,                 "tool_wooden_hoe");

    regTool("stone_pickaxe", "Stone Pickaxe", ToolType::Pickaxe, 4.0f, 1, 131, 3,      "tool_stone_pickaxe");
    regTool("stone_axe", "Stone Axe", ToolType::Axe, 4.0f, 1, 131, 9,                  "tool_stone_axe");
    regTool("stone_shovel", "Stone Shovel", ToolType::Shovel, 4.0f, 1, 131, 3.5f,      "tool_stone_shovel");
    regTool("stone_sword", "Stone Sword", ToolType::Sword, 1.0f, 1, 131, 5,             "tool_stone_sword");
    regTool("stone_hoe", "Stone Hoe", ToolType::Hoe, 1.0f, 1, 131, 1,                  "tool_stone_hoe");

    // Food items
    registerItem({  // 22 = Apple
        .name = "apple",
        .displayName = "Apple",
        .type = ItemType::Food,
        .maxStackSize = 64,
        .hungerRestore = 4,
        .saturationRestore = 2.4f,
        .iconTileName = "item_apple",
    });

    // Functional block items
    regBlockItem("crafting_table", "Crafting Table", Block::CraftingTable);  // 23

    // Ore block items (24-31)
    regBlockItem("coal_ore",     "Coal Ore",          Block::CoalOre);      // 24
    regBlockItem("iron_ore",     "Iron Ore",          Block::IronOre);      // 25
    regBlockItem("gold_ore",     "Gold Ore",          Block::GoldOre);      // 26
    regBlockItem("diamond_ore",  "Diamond Ore",       Block::DiamondOre);   // 27
    regBlockItem("redstone_ore", "Redstone Ore",      Block::RedstoneOre);  // 28
    regBlockItem("lapis_ore",    "Lapis Lazuli Ore",  Block::LapisOre);     // 29
    regBlockItem("emerald_ore",  "Emerald Ore",       Block::EmeraldOre);   // 30
    regBlockItem("copper_ore",   "Copper Ore",        Block::CopperOre);    // 31

    // Mineral drops (32-39) — items obtained by mining ores
    auto regMineral = [this](const std::string& name, const std::string& display,
                             const std::string& iconName) {
        registerItem({
            .name = name,
            .displayName = display,
            .type = ItemType::Material,
            .maxStackSize = 64,
            .iconTileName = iconName,
        });
    };

    regMineral("coal",          "Coal",           "item_coal");           // 32
    regMineral("raw_iron",      "Raw Iron",       "item_raw_iron");      // 33
    regMineral("raw_gold",      "Raw Gold",       "item_raw_gold");      // 34
    regMineral("raw_copper",    "Raw Copper",     "item_raw_copper");    // 35
    regMineral("diamond",       "Diamond",        "item_diamond");       // 36
    regMineral("emerald",       "Emerald",        "item_emerald");       // 37
    regMineral("lapis_lazuli",  "Lapis Lazuli",   "item_lapis_lazuli");  // 38
    regMineral("redstone",      "Redstone",       "item_redstone");      // 39

    // Ingots (40-42) — smelted from raw ores
    regMineral("iron_ingot",    "Iron Ingot",     "item_iron_ingot");    // 40
    regMineral("gold_ingot",    "Gold Ingot",     "item_gold_ingot");    // 41
    regMineral("copper_ingot",  "Copper Ingot",   "item_copper_ingot");  // 42

    // Iron tools (43-47) — MC: speed=6, level=2, durability=250
    regTool("iron_pickaxe", "Iron Pickaxe", ToolType::Pickaxe, 6.0f, 2, 250, 4,   "tool_iron_pickaxe");
    regTool("iron_axe",     "Iron Axe",     ToolType::Axe,     6.0f, 2, 250, 9,   "tool_iron_axe");
    regTool("iron_shovel",  "Iron Shovel",  ToolType::Shovel,  6.0f, 2, 250, 4.5f,"tool_iron_shovel");
    regTool("iron_sword",   "Iron Sword",   ToolType::Sword,   1.0f, 2, 250, 6,   "tool_iron_sword");
    regTool("iron_hoe",     "Iron Hoe",     ToolType::Hoe,     1.0f, 2, 250, 1,   "tool_iron_hoe");

    // Gold tools (48-52) — MC: speed=12, level=0, durability=32 (fast but fragile)
    regTool("gold_pickaxe", "Gold Pickaxe", ToolType::Pickaxe, 12.0f, 0, 32, 2,   "tool_gold_pickaxe");
    regTool("gold_axe",     "Gold Axe",     ToolType::Axe,     12.0f, 0, 32, 7,   "tool_gold_axe");
    regTool("gold_shovel",  "Gold Shovel",  ToolType::Shovel,  12.0f, 0, 32, 2.5f,"tool_gold_shovel");
    regTool("gold_sword",   "Gold Sword",   ToolType::Sword,   1.0f,  0, 32, 4,   "tool_gold_sword");
    regTool("gold_hoe",     "Gold Hoe",     ToolType::Hoe,     1.0f,  0, 32, 1,   "tool_gold_hoe");

    // Diamond tools (53-57) — MC: speed=8, level=3, durability=1561
    regTool("diamond_pickaxe","Diamond Pickaxe",ToolType::Pickaxe,8.0f,3,1561,5,  "tool_diamond_pickaxe");
    regTool("diamond_axe",    "Diamond Axe",    ToolType::Axe,    8.0f,3,1561,9,  "tool_diamond_axe");
    regTool("diamond_shovel", "Diamond Shovel", ToolType::Shovel, 8.0f,3,1561,5.5f,"tool_diamond_shovel");
    regTool("diamond_sword",  "Diamond Sword",  ToolType::Sword,  1.0f,3,1561,7,  "tool_diamond_sword");
    regTool("diamond_hoe",    "Diamond Hoe",    ToolType::Hoe,    1.0f,3,1561,1,  "tool_diamond_hoe");

    // Functional block items (continued)
    regBlockItem("furnace", "Furnace", Block::Furnace);  // 58
    regBlockItem("torch",   "Torch",   Block::Torch);    // 59

    // Vegetation / decoration block items (60-66)
    // These are Cross-render blocks — use iconTileName for 2D display.
    auto regPlantItem = [this](const std::string& name, const std::string& display,
                               uint16_t blockId, const std::string& texName) {
        registerItem({
            .name = name,
            .displayName = display,
            .type = ItemType::Block,
            .maxStackSize = 64,
            .blockId = blockId,
            .iconTileName = texName,
        });
    };

    regPlantItem("tall_grass",     "Tall Grass",     Block::TallGrass,     "tall_grass");      // 60
    regPlantItem("poppy",          "Poppy",          Block::Poppy,         "poppy");           // 61
    regPlantItem("dandelion",      "Dandelion",      Block::Dandelion,     "dandelion");       // 62
    regPlantItem("blue_orchid",    "Blue Orchid",    Block::BlueOrchid,    "blue_orchid");     // 63
    regPlantItem("brown_mushroom", "Brown Mushroom", Block::BrownMushroom, "brown_mushroom");  // 64
    regPlantItem("red_mushroom",   "Red Mushroom",   Block::RedMushroom,   "red_mushroom");    // 65
    regPlantItem("dead_bush",      "Dead Bush",      Block::DeadBush,      "dead_bush");       // 66

    // Storage block items
    regBlockItem("chest", "Chest", Block::Chest);  // 67

    // ========== Phase 5: 生物掉落物 + 新物品 ==========

    // 食物 — 生肉
    registerItem({  // 68 = RawPorkchop
        .name = "raw_porkchop",
        .displayName = "Raw Porkchop",
        .type = ItemType::Food,
        .maxStackSize = 64,
        .hungerRestore = 3,
        .saturationRestore = 1.8f,
        .iconTileName = "item_raw_porkchop",
    });
    registerItem({  // 69 = CookedPorkchop
        .name = "cooked_porkchop",
        .displayName = "Cooked Porkchop",
        .type = ItemType::Food,
        .maxStackSize = 64,
        .hungerRestore = 8,
        .saturationRestore = 12.8f,
        .iconTileName = "item_cooked_porkchop",
    });
    registerItem({  // 70 = RawBeef
        .name = "raw_beef",
        .displayName = "Raw Beef",
        .type = ItemType::Food,
        .maxStackSize = 64,
        .hungerRestore = 3,
        .saturationRestore = 1.8f,
        .iconTileName = "item_raw_beef",
    });
    registerItem({  // 71 = CookedBeef
        .name = "cooked_beef",
        .displayName = "Steak",
        .type = ItemType::Food,
        .maxStackSize = 64,
        .hungerRestore = 8,
        .saturationRestore = 12.8f,
        .iconTileName = "item_cooked_beef",
    });
    registerItem({  // 72 = RawChicken
        .name = "raw_chicken",
        .displayName = "Raw Chicken",
        .type = ItemType::Food,
        .maxStackSize = 64,
        .hungerRestore = 2,
        .saturationRestore = 1.2f,
        .iconTileName = "item_raw_chicken",
    });
    registerItem({  // 73 = CookedChicken
        .name = "cooked_chicken",
        .displayName = "Cooked Chicken",
        .type = ItemType::Food,
        .maxStackSize = 64,
        .hungerRestore = 6,
        .saturationRestore = 7.2f,
        .iconTileName = "item_cooked_chicken",
    });

    // 材料
    registerItem({  // 74 = Leather
        .name = "leather",
        .displayName = "Leather",
        .type = ItemType::Material,
        .maxStackSize = 64,
        .iconTileName = "item_leather",
    });
    registerItem({  // 75 = Feather
        .name = "feather",
        .displayName = "Feather",
        .type = ItemType::Material,
        .maxStackSize = 64,
        .iconTileName = "item_feather",
    });

    // 羊毛方块物品
    regBlockItem("white_wool", "White Wool", Block::WhiteWool);  // 76

    registerItem({  // 77 = Bone
        .name = "bone",
        .displayName = "Bone",
        .type = ItemType::Material,
        .maxStackSize = 64,
        .iconTileName = "item_bone",
    });
    registerItem({  // 78 = Arrow
        .name = "arrow",
        .displayName = "Arrow",
        .type = ItemType::Material,
        .maxStackSize = 64,
        .iconTileName = "item_arrow",
    });
    registerItem({  // 79 = Bow
        .name = "bow",
        .displayName = "Bow",
        .type = ItemType::Weapon,
        .maxStackSize = 1,
        .durability = 384,
        .attackDamage = 1.0f,
        .iconTileName = "item_bow",
    });
    registerItem({  // 80 = Gunpowder
        .name = "gunpowder",
        .displayName = "Gunpowder",
        .type = ItemType::Material,
        .maxStackSize = 64,
        .iconTileName = "item_gunpowder",
    });
    registerItem({  // 81 = StringItem
        .name = "string",
        .displayName = "String",
        .type = ItemType::Material,
        .maxStackSize = 64,
        .iconTileName = "item_string",
    });
    registerItem({  // 82 = SpiderEye
        .name = "spider_eye",
        .displayName = "Spider Eye",
        .type = ItemType::Food,
        .maxStackSize = 64,
        .hungerRestore = 2,
        .saturationRestore = 3.2f,
        .iconTileName = "item_spider_eye",
    });
}
