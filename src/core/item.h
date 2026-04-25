#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// ========== Item ID ==========
using ItemId = uint16_t;

namespace Item {
    constexpr ItemId None = 0;  // Empty / no item

    // Block items (registered in ItemRegistry::registerDefaults in this exact order).
    constexpr ItemId GrassBlock  = 1;
    constexpr ItemId Dirt        = 2;
    constexpr ItemId Cobblestone = 3;   // Stone drops cobblestone (pickaxe required)
    constexpr ItemId Sand        = 4;
    constexpr ItemId OakLog      = 5;
    constexpr ItemId OakLeaves   = 6;
    constexpr ItemId OakPlanks   = 7;
    constexpr ItemId Bedrock     = 8;
    constexpr ItemId Gravel      = 9;
    constexpr ItemId Stone       = 10;  // Silk-touched stone (no silk-touch yet — unused)

    // Materials
    constexpr ItemId Stick       = 11;

    // Wooden tools
    constexpr ItemId WoodenPickaxe = 12;
    constexpr ItemId WoodenAxe     = 13;
    constexpr ItemId WoodenShovel  = 14;
    constexpr ItemId WoodenSword   = 15;
    constexpr ItemId WoodenHoe     = 16;

    // Stone tools
    constexpr ItemId StonePickaxe  = 17;
    constexpr ItemId StoneAxe      = 18;
    constexpr ItemId StoneShovel   = 19;
    constexpr ItemId StoneSword    = 20;
    constexpr ItemId StoneHoe      = 21;

    // Food
    constexpr ItemId Apple         = 22;

    // Functional blocks
    constexpr ItemId CraftingTable = 23;

    // Ore block items (correspond to Block IDs 13-20)
    constexpr ItemId CoalOre     = 24;
    constexpr ItemId IronOre     = 25;
    constexpr ItemId GoldOre     = 26;
    constexpr ItemId DiamondOre  = 27;
    constexpr ItemId RedstoneOre = 28;
    constexpr ItemId LapisOre    = 29;
    constexpr ItemId EmeraldOre  = 30;
    constexpr ItemId CopperOre   = 31;

    // Mineral drops
    constexpr ItemId Coal         = 32;
    constexpr ItemId RawIron      = 33;
    constexpr ItemId RawGold      = 34;
    constexpr ItemId RawCopper    = 35;
    constexpr ItemId Diamond      = 36;
    constexpr ItemId Emerald      = 37;
    constexpr ItemId LapisLazuli  = 38;
    constexpr ItemId Redstone     = 39;

    // Ingots (smelted from raw ores)
    constexpr ItemId IronIngot    = 40;
    constexpr ItemId GoldIngot    = 41;
    constexpr ItemId CopperIngot  = 42;

    // Iron tools
    constexpr ItemId IronPickaxe  = 43;
    constexpr ItemId IronAxe      = 44;
    constexpr ItemId IronShovel   = 45;
    constexpr ItemId IronSword    = 46;
    constexpr ItemId IronHoe      = 47;

    // Gold tools
    constexpr ItemId GoldPickaxe  = 48;
    constexpr ItemId GoldAxe      = 49;
    constexpr ItemId GoldShovel   = 50;
    constexpr ItemId GoldSword    = 51;
    constexpr ItemId GoldHoe      = 52;

    // Diamond tools
    constexpr ItemId DiamondPickaxe = 53;
    constexpr ItemId DiamondAxe    = 54;
    constexpr ItemId DiamondShovel = 55;
    constexpr ItemId DiamondSword  = 56;
    constexpr ItemId DiamondHoe    = 57;

    // Functional blocks (continued)
    constexpr ItemId Furnace      = 58;
    constexpr ItemId Torch        = 59;

    // Vegetation / decoration block items
    constexpr ItemId TallGrass     = 60;
    constexpr ItemId Poppy         = 61;
    constexpr ItemId Dandelion     = 62;
    constexpr ItemId BlueOrchid    = 63;
    constexpr ItemId BrownMushroom = 64;
    constexpr ItemId RedMushroom   = 65;
    constexpr ItemId DeadBush      = 66;

    // Storage blocks
    constexpr ItemId Chest         = 67;

    // Phase 5: 生物掉落物 + 新物品
    constexpr ItemId RawPorkchop    = 68;
    constexpr ItemId CookedPorkchop = 69;
    constexpr ItemId RawBeef        = 70;
    constexpr ItemId CookedBeef     = 71;
    constexpr ItemId RawChicken     = 72;
    constexpr ItemId CookedChicken  = 73;
    constexpr ItemId Leather        = 74;
    constexpr ItemId Feather        = 75;
    constexpr ItemId WhiteWool      = 76;
    constexpr ItemId Bone           = 77;
    constexpr ItemId Arrow          = 78;
    constexpr ItemId Bow            = 79;
    constexpr ItemId Gunpowder      = 80;
    constexpr ItemId StringItem     = 81;
    constexpr ItemId SpiderEye      = 82;
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
    std::string iconTileName;       // Atlas tile name for 2D icon (tools, materials)
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

    // Tool durability bookkeeping. Returns true if the stack was consumed entirely
    // (durability reached 0 → caller should clear() the slot). Non-tool stacks
    // (max durability == 0) are unaffected.
    bool useDurability(uint16_t amount = 1, uint16_t maxDurability = 0);
};

// ========== Item Registry ==========
class ItemRegistry {
public:
    static ItemRegistry& instance();

    ItemId registerItem(ItemProperties props);
    const ItemProperties& get(ItemId id) const { return items_[id]; }

    // Lookup by name. Returns Item::None (0) if not found.
    ItemId getIdByName(const std::string& name) const {
        auto it = nameToId_.find(name);
        return (it != nameToId_.end()) ? it->second : 0;
    }

    uint16_t itemCount() const { return static_cast<uint16_t>(items_.size()); }

    void registerDefaults();

private:
    ItemRegistry() = default;
    std::vector<ItemProperties> items_;
    std::unordered_map<std::string, ItemId> nameToId_;
};
