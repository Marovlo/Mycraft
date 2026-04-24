#pragma once

#include "chunk.h"
#include <FastNoiseLite.h>
#include <cstdint>

// Abstract base for terrain generation strategies.
// This allows swapping generators (flat world, custom seeds, biome-based, etc.)
class TerrainGenerator {
public:
    virtual ~TerrainGenerator() = default;
    virtual void generate(Chunk& chunk) = 0;
    virtual void setSeed(int seed) = 0;
};

// Default overworld-style generator
class OverworldGenerator : public TerrainGenerator {
public:
    OverworldGenerator(int seed = 42);

    void generate(Chunk& chunk) override;
    void setSeed(int seed) override;

private:
    int seed_;
    FastNoiseLite continentNoise_;    // Large-scale landmass shape
    FastNoiseLite erosionNoise_;      // Medium-scale terrain variation
    FastNoiseLite detailNoise_;       // Small-scale roughness
    FastNoiseLite treeNoise_;         // Tree placement

    void initNoise();

    int getTerrainHeight(int wx, int wz) const;
    void generateTerrain(Chunk& chunk);
    void generateTrees(Chunk& chunk);
    void generateBedrock(Chunk& chunk);
};
