#include "item.h"
#include "block.h"

ItemRegistry& ItemRegistry::instance() {
    static ItemRegistry reg;
    return reg;
}

ItemId ItemRegistry::registerItem(ItemProperties props) {
    ItemId id = static_cast<ItemId>(items_.size());
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
    });

    // Wooden tools
    auto regTool = [this](const std::string& name, const std::string& display,
                          ToolType tool, float speed, int level, uint16_t dur, float atk) {
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
        });
    };

    regTool("wooden_pickaxe", "Wooden Pickaxe", ToolType::Pickaxe, 2.0f, 0, 59, 2);    // 12
    regTool("wooden_axe", "Wooden Axe", ToolType::Axe, 2.0f, 0, 59, 7);                 // 13
    regTool("wooden_shovel", "Wooden Shovel", ToolType::Shovel, 2.0f, 0, 59, 2.5f);     // 14
    regTool("wooden_sword", "Wooden Sword", ToolType::Sword, 1.0f, 0, 59, 4);            // 15
    regTool("wooden_hoe", "Wooden Hoe", ToolType::Hoe, 1.0f, 0, 59, 1);                 // 16

    regTool("stone_pickaxe", "Stone Pickaxe", ToolType::Pickaxe, 4.0f, 1, 131, 3);      // 17
    regTool("stone_axe", "Stone Axe", ToolType::Axe, 4.0f, 1, 131, 9);                  // 18
    regTool("stone_shovel", "Stone Shovel", ToolType::Shovel, 4.0f, 1, 131, 3.5f);      // 19
    regTool("stone_sword", "Stone Sword", ToolType::Sword, 1.0f, 1, 131, 5);             // 20
    regTool("stone_hoe", "Stone Hoe", ToolType::Hoe, 1.0f, 1, 131, 1);                  // 21
}
