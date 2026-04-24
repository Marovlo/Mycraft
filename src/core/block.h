#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <glm/glm.hpp>
#include "common.h"

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
}

// ========== Block Properties ==========

enum class BlockRenderType : uint8_t {
    Opaque,         // Standard solid block
    Transparent,    // Like glass — see through but still has faces
    Liquid,         // Water/lava — special rendering
    Foliage,        // Leaves, tall grass — may use alpha cutoff
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

    // Convenience checks
    bool isAir() const { return renderType == BlockRenderType::None; }
    bool blocksRendering() const { return isOpaque; }
};

// ========== Block Registry ==========
// Singleton that holds all block type definitions.
// New blocks are registered at startup; runtime lookup is O(1) by ID.

class BlockRegistry {
public:
    static BlockRegistry& instance();

    // Register a new block type, returns assigned BlockId
    BlockId registerBlock(BlockProperties props);

    // Lookup (no bounds check in release for speed)
    const BlockProperties& get(BlockId id) const {
        return blocks_[id];
    }

    // Convenience shortcuts
    bool isSolid(BlockId id) const   { return blocks_[id].isSolid; }
    bool isOpaque(BlockId id) const  { return blocks_[id].isOpaque; }
    bool isAir(BlockId id) const     { return blocks_[id].isAir(); }
    bool isLiquid(BlockId id) const  { return blocks_[id].isLiquid; }

    uint16_t blockCount() const { return static_cast<uint16_t>(blocks_.size()); }

    // Initialize all built-in block types
    void registerDefaults();

    // Resolve texture names to atlas tile indices after atlas is built.
    // texNameToId maps texture name (e.g., "grass_top") to atlas tile index.
    void resolveTextures(const std::unordered_map<std::string, uint16_t>& texNameToId);

private:
    BlockRegistry() = default;
    std::vector<BlockProperties> blocks_;
};
