#pragma once

#include "core/common.h"
#include "core/block.h"
#include "chunk.h"

#include <unordered_map>
#include <vector>
#include <functional>

class World {
public:
    // 方块变更回调（用于方块更新系统的邻居通知）
    using BlockChangeCallback = std::function<void(int x, int y, int z, BlockId oldId, BlockId newId)>;

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

    // 设置方块变更回调
    void setBlockChangeCallback(BlockChangeCallback cb) { blockChangeCallback_ = std::move(cb); }

    // Iterate all loaded chunks
    using ChunkMap = std::unordered_map<ChunkKey, Chunk, ChunkKeyHash>;
    ChunkMap& chunks() { return chunks_; }
    const ChunkMap& chunks() const { return chunks_; }

private:
    ChunkMap chunks_;
    BlockChangeCallback blockChangeCallback_;
};
