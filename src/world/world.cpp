#include "world.h"

BlockId World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return Block::Air;

    int cx = blockToChunk(x);
    int cz = blockToChunk(z);
    int lx = blockToLocal(x);
    int lz = blockToLocal(z);

    const auto* chunk = getChunk(cx, cz);
    if (!chunk) return Block::Air;
    return chunk->getBlock(lx, y, lz);
}

void World::setBlock(int x, int y, int z, BlockId id) {
    if (y < 0 || y >= CHUNK_HEIGHT) return;

    int cx = blockToChunk(x);
    int cz = blockToChunk(z);
    int lx = blockToLocal(x);
    int lz = blockToLocal(z);

    auto* chunk = getChunk(cx, cz);
    if (!chunk) return;

    chunk->setBlock(lx, y, lz, id);

    // Mark neighboring chunks dirty if block is on a border
    if (lx == 0)              markChunkDirty(cx - 1, cz);
    if (lx == CHUNK_SIZE - 1) markChunkDirty(cx + 1, cz);
    if (lz == 0)              markChunkDirty(cx, cz - 1);
    if (lz == CHUNK_SIZE - 1) markChunkDirty(cx, cz + 1);
}

Chunk* World::getChunk(int cx, int cz) {
    auto it = chunks_.find({cx, cz});
    return (it != chunks_.end()) ? &it->second : nullptr;
}

const Chunk* World::getChunk(int cx, int cz) const {
    auto it = chunks_.find({cx, cz});
    return (it != chunks_.end()) ? &it->second : nullptr;
}

Chunk& World::getOrCreateChunk(int cx, int cz) {
    auto [it, inserted] = chunks_.try_emplace(ChunkKey{cx, cz}, Chunk(cx, cz));
    return it->second;
}

void World::removeChunk(int cx, int cz) {
    chunks_.erase({cx, cz});
}

void World::markChunkDirty(int cx, int cz) {
    auto* chunk = getChunk(cx, cz);
    if (chunk) chunk->markMeshDirty();
}
