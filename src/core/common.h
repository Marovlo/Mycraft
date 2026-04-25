#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <cstdint>
#include <functional>

// ========== World constants ==========

constexpr int CHUNK_SIZE    = 16;
constexpr int CHUNK_HEIGHT  = 256;   // Minecraft standard
constexpr int SEA_LEVEL     = 62;    // Minecraft standard

// ========== Rendering ==========

constexpr int RENDER_DISTANCE = 8;

// ========== Physics ==========

constexpr float GRAVITY       = 28.0f;    // Slightly higher than MC for snappier feel
constexpr float JUMP_FORCE    = 9.0f;
constexpr float PLAYER_HEIGHT = 1.8f;
constexpr float PLAYER_EYE_HEIGHT = 1.62f; // Eye offset from feet, same as MC
constexpr float PLAYER_WIDTH  = 0.6f;     // Full width (MC is 0.6)
constexpr float MOVE_SPEED    = 4.317f;   // MC walking speed in blocks/sec
constexpr float SPRINT_SPEED  = 5.612f;   // MC sprinting speed
constexpr float SNEAK_SPEED   = 1.295f;   // MC sneaking speed (~30% of walk)
constexpr float SNEAK_EYE_HEIGHT = 1.50f; // Slightly lower eye when sneaking
constexpr float MAX_REACH     = 6.0f;     // Block interaction range (MC survival=4.5, creative=5, ours=6 for comfort)

// ========== Coordinate utilities ==========

// Convert world block coordinate to chunk coordinate
// Handles negative coordinates correctly (e.g., -1 -> chunk -1, not 0)
inline int blockToChunk(int blockCoord) {
    return (blockCoord >= 0) ? (blockCoord / CHUNK_SIZE)
                             : ((blockCoord - CHUNK_SIZE + 1) / CHUNK_SIZE);
}

// Convert world block coordinate to local coordinate within chunk (always 0..CHUNK_SIZE-1)
inline int blockToLocal(int blockCoord) {
    int local = blockCoord % CHUNK_SIZE;
    if (local < 0) local += CHUNK_SIZE;
    return local;
}

// ========== Chunk Key for hashmap ==========

struct ChunkKey {
    int x, z;

    bool operator==(const ChunkKey& other) const {
        return x == other.x && z == other.z;
    }
};

struct ChunkKeyHash {
    size_t operator()(const ChunkKey& key) const {
        // Combine hashes using a method less prone to collision
        size_t h1 = std::hash<int>()(key.x);
        size_t h2 = std::hash<int>()(key.z);
        return h1 ^ (h2 * 2654435761u);
    }
};

// ========== Direction enum (for face iteration) ==========

enum class Direction : uint8_t {
    PosX = 0,  // East  (+X)
    NegX,      // West  (-X)
    PosY,      // Up    (+Y)
    NegY,      // Down  (-Y)
    PosZ,      // South (+Z)
    NegZ,      // North (-Z)
    COUNT
};

// Direction offsets
inline glm::ivec3 directionOffset(Direction dir) {
    static const glm::ivec3 offsets[] = {
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0},
        { 0,  0,  1}, { 0,  0, -1},
    };
    return offsets[static_cast<int>(dir)];
}

inline glm::vec3 directionNormal(Direction dir) {
    return glm::vec3(directionOffset(dir));
}
