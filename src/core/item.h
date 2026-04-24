#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ========== Item ID ==========
using ItemId = uint16_t;

namespace Item {
    constexpr ItemId None = 0;  // Empty / no item
}

// ========== Item Type ==========
enum class ItemType : uint8_t {
    Block,      // Placeable block item
    Tool,       // Pickaxe, axe, shovel, hoe
    Weapon,     // Sword
    Food,       // Edible
    Material,   // Crafting material (sticks, ingots, etc.)
};

// ========== Tool Type ==========
enum class ToolType : uint8_t {
    None = 0,
    Pickaxe,
    Axe,
    Shovel,
    Hoe,
    Sword,
};

// ========== Item Properties ==========
struct ItemProperties {
    std::string name;           // e.g., "wooden_pickaxe"
    std::string displayName;    // e.g., "Wooden Pickaxe"
    ItemType type = ItemType::Material;

    uint16_t maxStackSize = 64; // Tools/weapons = 1, most items = 64
    uint16_t blockId = 0;       // For block items: which block to place

    // Tool properties
    ToolType toolType = ToolType::None;
    float miningSpeed = 1.0f;   // Multiplier (1.0 = hand speed)
    int miningLevel = 0;        // 0=wood, 1=stone, 2=iron, 3=diamond
    uint16_t durability = 0;    // 0 = infinite (non-tool items)

    // Weapon properties
    float attackDamage = 1.0f;  // Base: fist = 1

    // Food properties
    int hungerRestore = 0;
    float saturationRestore = 0.0f;

    // Texture
    uint16_t textureTileIndex = 0;  // Index into texture atlas
};

// ========== Item Stack ==========
// A stack of items in an inventory slot.
struct ItemStack {
    ItemId id = Item::None;
    uint16_t count = 0;
    uint16_t durability = 0;    // Current durability (for tools)

    bool isEmpty() const { return id == Item::None || count == 0; }

    void clear() { id = Item::None; count = 0; durability = 0; }

    // Try to merge another stack into this one. Returns leftover count.
    uint16_t merge(const ItemStack& other, uint16_t maxStack);
};

// ========== Item Registry ==========
class ItemRegistry {
public:
    static ItemRegistry& instance();

    ItemId registerItem(ItemProperties props);
    const ItemProperties& get(ItemId id) const { return items_[id]; }

    uint16_t itemCount() const { return static_cast<uint16_t>(items_.size()); }

    // Register all built-in items
    void registerDefaults();

private:
    ItemRegistry() = default;
    std::vector<ItemProperties> items_;
};
