#include "terrain_generator.h"
#include "core/common.h"
#include "core/block.h"

#include <algorithm>
#include <cmath>

OverworldGenerator::OverworldGenerator(int seed) : seed_(seed) {
    initNoise();
}

void OverworldGenerator::setSeed(int seed) {
    seed_ = seed;
    initNoise();
}

void OverworldGenerator::initNoise() {
    continentNoise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    continentNoise_.SetSeed(seed_);
    continentNoise_.SetFrequency(0.003f);  // Very large scale

    erosionNoise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    erosionNoise_.SetSeed(seed_ + 1);
    erosionNoise_.SetFrequency(0.01f);

    detailNoise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    detailNoise_.SetSeed(seed_ + 2);
    detailNoise_.SetFrequency(0.05f);

    treeNoise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    treeNoise_.SetSeed(seed_ + 100);
    treeNoise_.SetFrequency(0.5f);
}

int OverworldGenerator::getTerrainHeight(int wx, int wz) const {
    float fx = static_cast<float>(wx);
    float fz = static_cast<float>(wz);

    // Multi-octave terrain height
    float continent = continentNoise_.GetNoise(fx, fz);  // [-1, 1]
    float erosion   = erosionNoise_.GetNoise(fx, fz);
    float detail    = detailNoise_.GetNoise(fx, fz);

    // Combine: continent sets base height, erosion modulates amplitude, detail adds roughness
    float baseHeight = SEA_LEVEL + continent * 30.0f;
    float variation  = erosion * 12.0f;
    float rough      = detail * 4.0f;

    int height = static_cast<int>(baseHeight + variation + rough);
    return std::clamp(height, 2, CHUNK_HEIGHT - 2);
}

void OverworldGenerator::generate(Chunk& chunk) {
    generateBedrock(chunk);
    generateTerrain(chunk);
    generateTrees(chunk);
    chunk.markHasData();
}

void OverworldGenerator::generateBedrock(Chunk& chunk) {
    // Flat bedrock at y=0, irregular at y=1..4
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            chunk.setBlock(x, 0, z, Block::Bedrock);

            // Random bedrock layers 1-4
            int wx = chunk.worldX() + x;
            int wz = chunk.worldZ() + z;
            float n = detailNoise_.GetNoise(static_cast<float>(wx * 7), static_cast<float>(wz * 7));
            int bedrockHeight = 1 + static_cast<int>((n + 1.0f) * 2.0f); // 1..4
            for (int y = 1; y < bedrockHeight; y++) {
                chunk.setBlock(x, y, z, Block::Bedrock);
            }
        }
    }
}

void OverworldGenerator::generateTerrain(Chunk& chunk) {
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            int wx = chunk.worldX() + x;
            int wz = chunk.worldZ() + z;
            int height = getTerrainHeight(wx, wz);

            for (int y = 1; y < height; y++) {
                // Skip bedrock layer (already filled)
                if (chunk.getBlock(x, y, z) == Block::Bedrock) continue;

                if (y == height - 1) {
                    // Surface block
                    if (height <= SEA_LEVEL + 1) {
                        chunk.setBlock(x, y, z, Block::Sand);
                    } else {
                        chunk.setBlock(x, y, z, Block::Grass);
                    }
                } else if (y >= height - 4) {
                    // Sub-surface
                    if (height <= SEA_LEVEL + 1) {
                        chunk.setBlock(x, y, z, Block::Sand);
                    } else {
                        chunk.setBlock(x, y, z, Block::Dirt);
                    }
                } else {
                    chunk.setBlock(x, y, z, Block::Stone);
                }
            }

            // Water fill
            for (int y = height; y < SEA_LEVEL; y++) {
                if (chunk.getBlock(x, y, z) == Block::Air) {
                    chunk.setBlock(x, y, z, Block::Water);
                }
            }
        }
    }
}

void OverworldGenerator::generateTrees(Chunk& chunk) {
    // Only place trees away from chunk borders to avoid cross-chunk issues
    for (int x = 2; x < CHUNK_SIZE - 2; x++) {
        for (int z = 2; z < CHUNK_SIZE - 2; z++) {
            int wx = chunk.worldX() + x;
            int wz = chunk.worldZ() + z;

            // Tree density controlled by noise
            float treeDensity = treeNoise_.GetNoise(static_cast<float>(wx), static_cast<float>(wz));
            if (treeDensity < 0.3f) continue;

            // Check spacing (simple grid-based check)
            if ((wx % 5 != 0) || (wz % 5 != 0)) continue;

            int height = getTerrainHeight(wx, wz);

            // Only on grass above sea level
            if (height <= SEA_LEVEL + 1) continue;
            if (chunk.getBlock(x, height - 1, z) != Block::Grass) continue;

            int trunkHeight = 4 + static_cast<int>((treeDensity - 0.3f) * 5.0f); // 4-7
            trunkHeight = std::min(trunkHeight, 7);

            // Trunk
            for (int ty = height; ty < height + trunkHeight; ty++) {
                if (ty < CHUNK_HEIGHT) {
                    chunk.setBlock(x, ty, z, Block::Wood);
                }
            }

            // Leaves (sphere-ish)
            int leafStart = height + trunkHeight - 2;
            int leafEnd   = height + trunkHeight + 1;
            for (int ly = leafStart; ly <= leafEnd; ly++) {
                int radius = (ly < height + trunkHeight) ? 2 : 1;
                for (int lx = -radius; lx <= radius; lx++) {
                    for (int lz = -radius; lz <= radius; lz++) {
                        // Skip corners for rounder shape
                        if (std::abs(lx) == radius && std::abs(lz) == radius && ly >= height + trunkHeight)
                            continue;

                        int bx = x + lx, bz = z + lz;
                        if (bx >= 0 && bx < CHUNK_SIZE && bz >= 0 && bz < CHUNK_SIZE && ly < CHUNK_HEIGHT) {
                            if (chunk.getBlock(bx, ly, bz) == Block::Air) {
                                chunk.setBlock(bx, ly, bz, Block::Leaves);
                            }
                        }
                    }
                }
            }
        }
    }
}
