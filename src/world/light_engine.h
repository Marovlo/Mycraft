#pragma once

#include "chunk.h"
#include "world.h"
#include <vector>

class LightEngine {
public:
    static void initSkyLight(Chunk& chunk);
    static void initBlockLight(Chunk& chunk);
    static uint8_t getLight(const World& world, int wx, int wy, int wz);
    static float lightToFloat(uint8_t level);

    // Called after a block is placed or removed at world position (wx, wy, wz).
    // Recalculates both sky and block light in the affected area.
    // Marks affected chunks as meshDirty.
    static void updateAfterBlockChange(World& world, int wx, int wy, int wz);

private:
    // BFS propagate a single light type in a local region of the world.
    // Used by updateAfterBlockChange for targeted re-propagation.
    static void propagateSkyLightAt(World& world, int wx, int wy, int wz);
    static void propagateBlockLightAt(World& world, int wx, int wy, int wz);
};
