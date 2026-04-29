#include "world.h"
#include "light_engine.h"

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

    // 记录旧方块的光照相关属性，用于判断是否需要重算光照
    const auto& reg = BlockRegistry::instance();
    BlockId oldId = chunk->getBlock(lx, y, lz);
    bool oldOpaque = reg.isOpaque(oldId);
    uint8_t oldEmit = reg.get(oldId).lightEmit;

    chunk->setBlock(lx, y, lz, id);
    chunk->markModified();  // Player-initiated change → needs persistence

    // Mark neighboring chunks dirty if block is on a border
    if (lx == 0)              markChunkDirty(cx - 1, cz);
    if (lx == CHUNK_SIZE - 1) markChunkDirty(cx + 1, cz);
    if (lz == 0)              markChunkDirty(cx, cz - 1);
    if (lz == CHUNK_SIZE - 1) markChunkDirty(cx, cz + 1);

    // 只有当方块的不透明性或发光属性发生变化时，才需要重算光照。
    bool newOpaque = reg.isOpaque(id);
    uint8_t newEmit = reg.get(id).lightEmit;
    if (oldOpaque != newOpaque || oldEmit != newEmit) {
        LightEngine::updateAfterBlockChange(*this, x, y, z);
    }

    // 通知方块更新系统（沙子下落、水流动等）
    if (blockChangeCallback_) {
        blockChangeCallback_(x, y, z, oldId, id);
    }
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
