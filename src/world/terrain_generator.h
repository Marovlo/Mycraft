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

    int getTerrainHeight(int wx, int wz) const;

    // Biome system
    enum class Biome : uint8_t { Plains, Forest, Desert, Snowy };
    Biome getBiome(int wx, int wz) const;

private:
    int seed_;
    FastNoiseLite continentNoise_;
    FastNoiseLite erosionNoise_;
    FastNoiseLite detailNoise_;
    FastNoiseLite treeNoise_;
    FastNoiseLite caveNoise_;       // 3D noise for cave carving
    FastNoiseLite caveNoise2_;      // Second octave for larger caves
    FastNoiseLite temperatureNoise_;
    FastNoiseLite humidityNoise_;

    void initNoise();

    void generateTerrain(Chunk& chunk);
    void generateCaves(Chunk& chunk);
    void generateOres(Chunk& chunk);
    void generateTrees(Chunk& chunk);
    void generateVegetation(Chunk& chunk);
    void generateBedrock(Chunk& chunk);

    // Ore vein generation config
    struct OreConfig {
        BlockId blockId;
        int minY, maxY;     // Y range (inclusive)
        int veinSize;       // max blocks per vein
        int veinsPerChunk;  // attempts per chunk
    };
    static const OreConfig kOreConfigs[];
    static const int kOreConfigCount;

    // Generate a single ore vein centered at (cx, cy, cz) within the chunk.
    void placeVein(Chunk& chunk, const OreConfig& cfg, int cx, int cy, int cz) const;
};
