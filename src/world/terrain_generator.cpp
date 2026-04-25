#include "terrain_generator.h"
#include "light_engine.h"
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

    caveNoise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    caveNoise_.SetSeed(seed_ + 200);
    caveNoise_.SetFrequency(0.025f);

    caveNoise2_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    caveNoise2_.SetSeed(seed_ + 201);
    caveNoise2_.SetFrequency(0.015f);

    temperatureNoise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    temperatureNoise_.SetSeed(seed_ + 300);
    temperatureNoise_.SetFrequency(0.002f);  // very large scale

    humidityNoise_.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    humidityNoise_.SetSeed(seed_ + 301);
    humidityNoise_.SetFrequency(0.003f);
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

OverworldGenerator::Biome OverworldGenerator::getBiome(int wx, int wz) const {
    float temp = temperatureNoise_.GetNoise(static_cast<float>(wx), static_cast<float>(wz));
    float humid = humidityNoise_.GetNoise(static_cast<float>(wx), static_cast<float>(wz));
    // temp: -1..1, humid: -1..1
    if (temp < -0.3f) return Biome::Snowy;    // cold
    if (temp > 0.3f && humid < -0.1f) return Biome::Desert;  // hot + dry
    if (humid > 0.2f) return Biome::Forest;   // warm + wet
    return Biome::Plains;
}

void OverworldGenerator::generate(Chunk& chunk) {
    generateBedrock(chunk);
    generateTerrain(chunk);
    generateCaves(chunk);
    generateOres(chunk);
    generateTrees(chunk);
    generateVegetation(chunk);
    LightEngine::initSkyLight(chunk);
    LightEngine::initBlockLight(chunk);
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
            Biome biome = getBiome(wx, wz);

            for (int y = 1; y < height; y++) {
                if (chunk.getBlock(x, y, z) == Block::Bedrock) continue;

                if (y == height - 1) {
                    // Surface block — biome-dependent
                    if (height <= SEA_LEVEL + 1) {
                        chunk.setBlock(x, y, z, Block::Sand);
                    } else if (biome == Biome::Desert) {
                        chunk.setBlock(x, y, z, Block::Sand);
                    } else if (biome == Biome::Snowy) {
                        chunk.setBlock(x, y, z, Block::Snow);
                    } else {
                        chunk.setBlock(x, y, z, Block::Grass);
                    }
                } else if (y >= height - 4) {
                    // Sub-surface
                    if (height <= SEA_LEVEL + 1 || biome == Biome::Desert) {
                        chunk.setBlock(x, y, z, Block::Sandstone);
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

            Biome biome = getBiome(wx, wz);
            // No trees in desert
            if (biome == Biome::Desert) continue;

            float treeDensity = treeNoise_.GetNoise(static_cast<float>(wx), static_cast<float>(wz));

            // Forest biome has denser trees (lower threshold)
            float threshold = (biome == Biome::Forest) ? 0.1f : 0.3f;
            if (treeDensity < threshold) continue;

            // Spacing: forest=every 4, others=every 5
            int spacing = (biome == Biome::Forest) ? 4 : 5;
            if ((wx % spacing != 0) || (wz % spacing != 0)) continue;

            int height = getTerrainHeight(wx, wz);
            if (height <= SEA_LEVEL + 1) continue;

            BlockId surface = chunk.getBlock(x, height - 1, z);
            // Accept grass or snow as tree-plantable surface
            if (surface != Block::Grass && surface != Block::Snow) continue;

            int trunkHeight = 4 + static_cast<int>((treeDensity - threshold) * 5.0f);
            trunkHeight = std::min(trunkHeight, 7);

            // Use biome-appropriate wood/leaves
            BlockId trunkBlock = (biome == Biome::Snowy) ? Block::SpruceLog : Block::Wood;
            BlockId leafBlock  = (biome == Biome::Snowy) ? Block::SpruceLeaves : Block::Leaves;

            for (int ty = height; ty < height + trunkHeight; ty++) {
                if (ty < CHUNK_HEIGHT) {
                    chunk.setBlock(x, ty, z, trunkBlock);
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
                                chunk.setBlock(bx, ly, bz, leafBlock);
                            }
                        }
                    }
                }
            }
        }
    }
}

// ============================================================
// Cave generation — 3D noise carving
// ============================================================

void OverworldGenerator::generateCaves(Chunk& chunk) {
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            float wx = static_cast<float>(chunk.worldX() + x);
            float wz = static_cast<float>(chunk.worldZ() + z);

            int surfaceY = getTerrainHeight(chunk.worldX() + x, chunk.worldZ() + z);

            // Allow caves up to surfaceY-2 so they can break through hillsides
            // and create visible openings on the surface.
            for (int y = 5; y < surfaceY - 2; ++y) {
                BlockId current = chunk.getBlock(x, y, z);
                if (current != Block::Stone && current != Block::Dirt &&
                    current != Block::Gravel && current != Block::Grass) continue;

                float fy = static_cast<float>(y);

                float n1 = caveNoise_.GetNoise(wx, fy, wz);
                float n2 = caveNoise2_.GetNoise(wx, fy * 0.7f, wz);
                float combined = n1 * n1 + n2 * n2;

                // Larger threshold = bigger caves; 0.035 gives visible cave systems
                if (combined < 0.035f) {
                    chunk.setBlock(x, y, z, Block::Air);
                }
            }
        }
    }
}

// ============================================================
// Ore generation
// ============================================================

const OverworldGenerator::OreConfig OverworldGenerator::kOreConfigs[] = {
    // blockId              minY  maxY  veinSize  veinsPerChunk
    { Block::CoalOre,         5,  128,    17,        20 },
    { Block::IronOre,         5,   64,     9,        20 },
    { Block::CopperOre,       5,   96,     9,        10 },
    { Block::GoldOre,         5,   32,     9,         2 },
    { Block::DiamondOre,      5,   16,     8,         1 },
    { Block::RedstoneOre,     5,   16,     8,         8 },
    { Block::LapisOre,        5,   32,     7,         1 },
    { Block::EmeraldOre,      5,   32,     1,         1 },
};
const int OverworldGenerator::kOreConfigCount =
    static_cast<int>(sizeof(kOreConfigs) / sizeof(kOreConfigs[0]));

void OverworldGenerator::generateOres(Chunk& chunk) {
    uint32_t chunkHash = static_cast<uint32_t>(
        static_cast<int64_t>(seed_) * 6364136223846793005LL
        + chunk.chunkX() * 1664525 + chunk.chunkZ() * 1013904223);

    auto nextRand = [&]() -> uint32_t {
        chunkHash = chunkHash * 1664525u + 1013904223u;
        return chunkHash;
    };

    for (int oi = 0; oi < kOreConfigCount; ++oi) {
        const auto& cfg = kOreConfigs[oi];
        for (int v = 0; v < cfg.veinsPerChunk; ++v) {
            int cx = static_cast<int>(nextRand() % CHUNK_SIZE);
            int cy = cfg.minY + static_cast<int>(nextRand() % static_cast<uint32_t>(cfg.maxY - cfg.minY + 1));
            int cz = static_cast<int>(nextRand() % CHUNK_SIZE);
            placeVein(chunk, cfg, cx, cy, cz);
        }
    }
}

void OverworldGenerator::placeVein(Chunk& chunk, const OreConfig& cfg,
                                   int startX, int startY, int startZ) const {
    auto canReplace = [&](int x, int y, int z) -> bool {
        if (x < 0 || x >= CHUNK_SIZE || y < 1 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_SIZE)
            return false;
        return chunk.getBlock(x, y, z) == Block::Stone;
    };

    if (!canReplace(startX, startY, startZ)) return;
    chunk.setBlock(startX, startY, startZ, cfg.blockId);

    if (cfg.veinSize <= 1) return;

    int x = startX, y = startY, z = startZ;
    uint32_t rng = static_cast<uint32_t>(startX * 73856093 + startY * 19349663
                                          + startZ * 83492791 + cfg.blockId * 39916801);

    for (int i = 1; i < cfg.veinSize; ++i) {
        rng = rng * 1664525u + 1013904223u;
        int dir = static_cast<int>(rng % 6);
        switch (dir) {
            case 0: x++; break;
            case 1: x--; break;
            case 2: y++; break;
            case 3: y--; break;
            case 4: z++; break;
            case 5: z--; break;
        }
        if (canReplace(x, y, z)) {
            chunk.setBlock(x, y, z, cfg.blockId);
        }
    }
}

// ============================================================
// Vegetation generation
// ============================================================

void OverworldGenerator::generateVegetation(Chunk& chunk) {
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int wx = chunk.worldX() + x;
            int wz = chunk.worldZ() + z;
            int height = getTerrainHeight(wx, wz);

            if (height <= SEA_LEVEL + 1) continue;
            if (height >= CHUNK_HEIGHT - 1) continue;
            if (chunk.getBlock(x, height, z) != Block::Air) continue;

            Biome biome = getBiome(wx, wz);
            BlockId surface = chunk.getBlock(x, height - 1, z);

            float n = detailNoise_.GetNoise(
                static_cast<float>(wx * 3 + 1000),
                static_cast<float>(wz * 3 + 1000));

            if (biome == Biome::Desert) {
                // Desert: dead bushes and cacti on sand
                if (surface != Block::Sand) continue;
                if (n > 0.3f) {
                    chunk.setBlock(x, height, z, Block::DeadBush);
                } else if (n < -0.35f) {
                    // Cactus: 1-3 blocks tall
                    int cactusH = 1 + static_cast<int>((n + 0.5f) * 4);
                    cactusH = std::clamp(cactusH, 1, 3);
                    for (int cy = 0; cy < cactusH && height + cy < CHUNK_HEIGHT; ++cy) {
                        chunk.setBlock(x, height + cy, z, Block::Cactus);
                    }
                }
            } else if (biome == Biome::Snowy) {
                // Snow: sparse grass, no flowers
                if (surface != Block::Snow) continue;
                if (n > 0.35f) {
                    chunk.setBlock(x, height, z, Block::TallGrass);
                }
            } else {
                // Plains / Forest: normal vegetation
                if (surface != Block::Grass) continue;

            if (n > 0.2f) {
                // Tall grass (most common)
                chunk.setBlock(x, height, z, Block::TallGrass);
            } else if (n > 0.05f && n < 0.1f) {
                // Flowers (rarer)
                float flowerType = detailNoise_.GetNoise(
                    static_cast<float>(wx * 7 + 500),
                    static_cast<float>(wz * 7 + 500));
                if (flowerType > 0.3f)
                    chunk.setBlock(x, height, z, Block::Poppy);
                else if (flowerType > 0.0f)
                    chunk.setBlock(x, height, z, Block::Dandelion);
                else
                    chunk.setBlock(x, height, z, Block::BlueOrchid);
            } else if (n < -0.4f) {
                // Mushrooms (rare, in darker spots)
                float mushroomType = detailNoise_.GetNoise(
                    static_cast<float>(wx * 11),
                    static_cast<float>(wz * 11));
                if (mushroomType > 0.0f)
                    chunk.setBlock(x, height, z, Block::BrownMushroom);
                else
                    chunk.setBlock(x, height, z, Block::RedMushroom);
            }
            } // end Plains/Forest
        }
    }
}
