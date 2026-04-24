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

    // Chunk position in chunk coordinates
    int chunkX() const { return cx_; }
    int chunkZ() const { return cz_; }

    // World-space origin of this chunk (block coordinates of its (0,0,0) corner)
    int worldX() const { return cx_ * CHUNK_SIZE; }
    int worldZ() const { return cz_ * CHUNK_SIZE; }

    // Mesh management (owned by the chunk for lifetime tracking)
    Mesh& getMesh() { return mesh_; }
    const Mesh& getMesh() const { return mesh_; }
    void setMesh(Mesh mesh) { mesh_ = mesh; }
    bool hasMesh() const { return mesh_.indexCount > 0; }

    // Dirty flag for re-meshing
    bool isMeshDirty() const { return meshDirty_; }
    void markMeshDirty() { meshDirty_ = true; }
    void clearMeshDirty() { meshDirty_ = false; }

    // Data generation flag
    bool hasData() const { return hasData_; }
    void markHasData() { hasData_ = true; }

private:
    static constexpr int BLOCK_COUNT = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;

    // Flat array indexed as [x * CHUNK_HEIGHT * CHUNK_SIZE + y * CHUNK_SIZE + z]
    std::array<BlockId, BLOCK_COUNT> blocks_ = {};

    int cx_ = 0, cz_ = 0;

    Mesh mesh_;
    bool meshDirty_ = true;
    bool hasData_ = false;

    int blockIndex(int x, int y, int z) const {
        return x * CHUNK_HEIGHT * CHUNK_SIZE + y * CHUNK_SIZE + z;
    }
};
