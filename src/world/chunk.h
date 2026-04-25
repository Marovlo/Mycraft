#pragma once

#include "core/common.h"
#include "core/block.h"
#include "engine/vulkan_engine.h"

#include <array>

// ========== Chunk ==========
// A 16x256x16 column of blocks.
// Uses flat array for cache-friendly access (Y is the fast-changing axis
// since vertical operations like lighting are common).
// Access pattern: blocks_[x * CHUNK_HEIGHT * CHUNK_SIZE + y * CHUNK_SIZE + z]

class Chunk {
public:
    Chunk() = default;
    Chunk(int cx, int cz) : cx_(cx), cz_(cz) {}

    // Block access (local coordinates: x,z in [0, CHUNK_SIZE), y in [0, CHUNK_HEIGHT))
    BlockId getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockId id);

    // Light access — packed: upper 4 bits = skyLight, lower 4 bits = blockLight
    uint8_t getSkyLight(int x, int y, int z) const;
    uint8_t getBlockLight(int x, int y, int z) const;
    uint8_t getMaxLight(int x, int y, int z) const;
    void setSkyLight(int x, int y, int z, uint8_t val);
    void setBlockLight(int x, int y, int z, uint8_t val);

    // Heightmap: highest non-transparent Y per column (for sky light fast-path)
    uint8_t getHeight(int x, int z) const { return heightMap_[x * CHUNK_SIZE + z]; }
    void updateHeightMap();

    // Chunk position in chunk coordinates
    int chunkX() const { return cx_; }
    int chunkZ() const { return cz_; }

    int worldX() const { return cx_ * CHUNK_SIZE; }
    int worldZ() const { return cz_ * CHUNK_SIZE; }

    // Mesh management — opaque pass
    Mesh& getMesh() { return mesh_; }
    const Mesh& getMesh() const { return mesh_; }
    void setMesh(Mesh mesh) { mesh_ = mesh; }
    bool hasMesh() const { return mesh_.indexCount > 0; }

    // Mesh management — transparent pass (water, glass, etc.)
    Mesh& getTransparentMesh() { return transparentMesh_; }
    const Mesh& getTransparentMesh() const { return transparentMesh_; }
    void setTransparentMesh(Mesh mesh) { transparentMesh_ = mesh; }
    bool hasTransparentMesh() const { return transparentMesh_.indexCount > 0; }

    bool isMeshDirty() const { return meshDirty_; }
    void markMeshDirty() { meshDirty_ = true; }
    void clearMeshDirty() { meshDirty_ = false; }

    bool hasData() const { return hasData_; }
    void markHasData() { hasData_ = true; }

    // Light dirty flag (separate from mesh dirty — light recalc then triggers mesh rebuild)
    bool isLightDirty() const { return lightDirty_; }
    void markLightDirty() { lightDirty_ = true; }
    void clearLightDirty() { lightDirty_ = false; }

    // Modified flag — set when player places/breaks blocks. Used by save system
    // to decide which chunks need persistence (unmodified = regenerated from seed).
    bool isModified() const { return isModified_; }
    void markModified() { isModified_ = true; }
    void clearModified() { isModified_ = false; }

    // Direct access to blocks array (for serialization — avoid per-block copies)
    const BlockId* blocksData() const { return blocks_.data(); }
    BlockId* blocksData() { return blocks_.data(); }
    static constexpr int blockCount() { return BLOCK_COUNT; }

private:
    static constexpr int BLOCK_COUNT = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;

    std::array<BlockId, BLOCK_COUNT> blocks_ = {};
    std::array<uint8_t, BLOCK_COUNT> lightData_ = {};  // packed: sky<<4 | block
    std::array<uint8_t, CHUNK_SIZE * CHUNK_SIZE> heightMap_ = {};

    int cx_ = 0, cz_ = 0;

    Mesh mesh_;
    Mesh transparentMesh_;
    bool meshDirty_ = true;
    bool hasData_ = false;
    bool lightDirty_ = true;
    bool isModified_ = false;

    int blockIndex(int x, int y, int z) const {
        return x * CHUNK_HEIGHT * CHUNK_SIZE + y * CHUNK_SIZE + z;
    }
};
