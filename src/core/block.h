#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <glm/glm.hpp>
#include "common.h"
#include "item.h"     // ToolType, ItemId, ItemStack

// ========== Block ID ==========
// Using uint16_t to support up to 65535 block types (MC has ~900)
using BlockId = uint16_t;

namespace Block {
    constexpr BlockId Air    = 0;
    constexpr BlockId Grass  = 1;
    constexpr BlockId Dirt   = 2;
    constexpr BlockId Stone  = 3;
    constexpr BlockId Sand   = 4;
    constexpr BlockId Wood   = 5;
    constexpr BlockId Leaves = 6;
    constexpr BlockId Water  = 7;
    constexpr BlockId Cobblestone = 8;
    constexpr BlockId OakPlanks   = 9;
    constexpr BlockId Bedrock     = 10;
    constexpr BlockId Gravel      = 11;
    constexpr BlockId CraftingTable = 12;

    // Ores
    constexpr BlockId CoalOre     = 13;
    constexpr BlockId IronOre     = 14;
    constexpr BlockId GoldOre     = 15;
    constexpr BlockId DiamondOre  = 16;
    constexpr BlockId RedstoneOre = 17;
    constexpr BlockId LapisOre    = 18;
    constexpr BlockId EmeraldOre  = 19;
    constexpr BlockId CopperOre   = 20;

    // Functional blocks (continued)
    constexpr BlockId Furnace     = 21;

    // Vegetation / decoration (non-solid, Cross render)
    constexpr BlockId TallGrass     = 22;
    constexpr BlockId Poppy         = 23;
    constexpr BlockId Dandelion     = 24;
    constexpr BlockId BlueOrchid    = 25;
    constexpr BlockId BrownMushroom = 26;
    constexpr BlockId RedMushroom   = 27;
    constexpr BlockId DeadBush      = 28;

    // Light sources
    constexpr BlockId Torch         = 29;

    // Biome blocks
    constexpr BlockId Snow          = 30;
    constexpr BlockId Sandstone     = 31;
    constexpr BlockId SpruceLog     = 32;
    constexpr BlockId SpruceLeaves  = 33;
    constexpr BlockId Cactus        = 34;

    // Storage blocks
    constexpr BlockId Chest         = 35;

    // Wool
    constexpr BlockId WhiteWool     = 36;
}

// ========== Block Properties ==========

enum class BlockRenderType : uint8_t {
    Opaque,         // Standard solid block
    Transparent,    // Like glass — see through but still has faces
    Liquid,         // Water/lava — special rendering
    Foliage,        // Leaves, tall grass — may use alpha cutoff
    Cross,          // X-shaped cross faces (flowers, grass, mushrooms)
    None,           // Air — not rendered
};

// Which face texture to use (allows per-face textures like grass block)
struct BlockFaceTextures {
    uint16_t top;
    uint16_t bottom;
    uint16_t north;
    uint16_t south;
    uint16_t east;
    uint16_t west;

    // Convenience: all same texture
    static BlockFaceTextures uniform(uint16_t texId) {
        return {texId, texId, texId, texId, texId, texId};
    }

    // Top/bottom different from sides
    static BlockFaceTextures topBottom(uint16_t top, uint16_t bottom, uint16_t side) {
        return {top, bottom, side, side, side, side};
    }

    uint16_t forDirection(Direction dir) const {
        switch (dir) {
            case Direction::PosY: return top;
            case Direction::NegY: return bottom;
            case Direction::NegZ: return north;
            case Direction::PosZ: return south;
            case Direction::PosX: return east;
            case Direction::NegX: return west;
            default: return top;
        }
    }
};

// Per-face texture names (resolved to tile indices after atlas is built)
struct BlockFaceTextureNames {
    std::string top, bottom, north, south, east, west;

    static BlockFaceTextureNames uniform(const std::string& name) {
        return {name, name, name, name, name, name};
    }
    static BlockFaceTextureNames topBottom(const std::string& top, const std::string& bottom, const std::string& side) {
        return {top, bottom, side, side, side, side};
    }
};

struct BlockProperties {
    std::string name;               // e.g., "grass_block"
    std::string displayName;        // e.g., "Grass Block"
    BlockRenderType renderType = BlockRenderType::None;
    BlockFaceTextures textures = {};
    BlockFaceTextureNames textureNames = {};  // resolved to textures by resolveTextures()

    bool isSolid       = false;     // Has collision
    bool isOpaque      = false;     // Blocks light / occludes neighbors
    bool isLiquid      = false;
    float hardness     = 1.0f;      // Break time multiplier (0 = instant, -1 = unbreakable)
    uint8_t lightEmit  = 0;         // Light level emitted (0-15)
    bool isInteractable = false;    // Right-click opens a GUI (crafting table, furnace, etc.)

    // --- Mining (Batch 3) ---
    // Tool type that *accelerates* mining (and is required to obtain drops if
    // requireToolForDrops=true). ToolType::None = breakable by hand at full speed.
    ToolType requiredToolType  = ToolType::None;
    int     requiredMiningLevel = 0;     // 0=wooden, 1=stone, 2=iron, 3=diamond
    bool    requireToolForDrops = false; // Stone needs pickaxe to drop cobblestone

    // Drops list. Evaluated in order; first matching rule wins. Empty = no drops.
    // Each rule: which item, how many, and (if requireToolForDrops) gating on tool.
    struct Drop {
        ItemId   item     = 0; // 0 = nothing
        uint16_t minCount = 1;
        uint16_t maxCount = 1;
    };
    std::vector<Drop> drops;

    // Convenience checks
    bool isAir() const { return renderType == BlockRenderType::None; }
    bool blocksRendering() const { return isOpaque; }
    bool isBreakable() const { return hardness >= 0.0f; }
};

// ========== Block Registry ==========
// Singleton that holds all block type definitions.
// New blocks are registered at startup; runtime lookup is O(1) by ID.

class BlockRegistry {
public:
    static BlockRegistry& instance();

    // Register a new block type, returns assigned BlockId.
    // The name field must be unique; duplicates trigger an assertion.
    BlockId registerBlock(BlockProperties props);

    // Lookup by ID (no bounds check in release for speed)
    const BlockProperties& get(BlockId id) const {
        return blocks_[id];
    }

    // Lookup by name. Returns Block::Air (0) if not found.
    BlockId getIdByName(const std::string& name) const {
        auto it = nameToId_.find(name);
        return (it != nameToId_.end()) ? it->second : 0;
    }

    // Convenience shortcuts
    bool isSolid(BlockId id) const   { return blocks_[id].isSolid; }
    bool isOpaque(BlockId id) const  { return blocks_[id].isOpaque; }
    bool isAir(BlockId id) const     { return blocks_[id].isAir(); }
    bool isLiquid(BlockId id) const  { return blocks_[id].isLiquid; }

    uint16_t blockCount() const { return static_cast<uint16_t>(blocks_.size()); }

    void registerDefaults();

    void resolveTextures(const std::unordered_map<std::string, uint16_t>& texNameToId);

private:
    BlockRegistry() = default;
    std::vector<BlockProperties> blocks_;
    std::unordered_map<std::string, BlockId> nameToId_;
};
