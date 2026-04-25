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

uint8_t Chunk::getSkyLight(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE)
        return 15;  // outside = full sky
    return (lightData_[blockIndex(x, y, z)] >> 4) & 0xF;
}

uint8_t Chunk::getBlockLight(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE)
        return 0;
    return lightData_[blockIndex(x, y, z)] & 0xF;
}

uint8_t Chunk::getMaxLight(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE)
        return 15;
    uint8_t d = lightData_[blockIndex(x, y, z)];
    uint8_t sky = (d >> 4) & 0xF;
    uint8_t blk = d & 0xF;
    return sky > blk ? sky : blk;
}

void Chunk::setSkyLight(int x, int y, int z, uint8_t val) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE)
        return;
    int idx = blockIndex(x, y, z);
    lightData_[idx] = (val << 4) | (lightData_[idx] & 0xF);
}

void Chunk::setBlockLight(int x, int y, int z, uint8_t val) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE)
        return;
    int idx = blockIndex(x, y, z);
    lightData_[idx] = (lightData_[idx] & 0xF0) | (val & 0xF);
}

void Chunk::updateHeightMap() {
    const auto& reg = BlockRegistry::instance();
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            uint8_t h = 0;
            for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                BlockId b = blocks_[blockIndex(x, y, z)];
                if (reg.isOpaque(b)) {
                    h = static_cast<uint8_t>(y + 1);
                    break;
                }
            }
            heightMap_[x * CHUNK_SIZE + z] = h;
        }
    }
}
