#include "chunk.h"

BlockId Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return Block::Air;
    }
    return blocks_[blockIndex(x, y, z)];
}

void Chunk::setBlock(int x, int y, int z, BlockId id) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE) {
        return;
    }
    blocks_[blockIndex(x, y, z)] = id;
    meshDirty_ = true;
}
