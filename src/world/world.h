#pragma once

#include "core/common.h"
#include "core/block.h"
#include "chunk.h"

#include <unordered_map>
#include <vector>

class World {
public:
    // Block access by world coordinates
    BlockId getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockId id);

    // Chunk access
    Chunk* getChunk(int cx, int cz);
    const Chunk* getChunk(int cx, int cz) const;
    Chunk& getOrCreateChunk(int cx, int cz);
    void removeChunk(int cx, int cz);

    // Mark a chunk mesh dirty (used when neighbor changes affect borders)
    void markChunkDirty(int cx, int cz);

    // Iterate all loaded chunks
    using ChunkMap = std::unordered_map<ChunkKey, Chunk, ChunkKeyHash>;
    ChunkMap& chunks() { return chunks_; }
    const ChunkMap& chunks() const { return chunks_; }

private:
    ChunkMap chunks_;
};
