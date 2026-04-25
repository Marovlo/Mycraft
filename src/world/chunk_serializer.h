#pragma once

#include "chunk.h"
#include <vector>
#include <cstdint>

class BinaryWriter;
class BinaryReader;

// ========== ChunkSerializer ==========
// Handles chunk block data serialization/deserialization.
// Format (v1, no compression):
//   [4 bytes] chunkX (int32)
//   [4 bytes] chunkZ (int32)
//   [1 byte]  flags  (bit 0 = hasData)
//   [BLOCK_COUNT × 2 bytes] blocks (uint16 per block, 128 KB)
//
// Light data is NOT saved — recomputed on load via LightEngine::initSkyLight/initBlockLight.

class ChunkSerializer {
public:
    // Serialize chunk block data to a byte buffer.
    // Only call on modified chunks (isModified() == true).
    static std::vector<uint8_t> serialize(const Chunk& chunk);

    // Deserialize chunk block data from a byte buffer.
    // Fills chunk.blocks_, marks hasData, but does NOT compute lighting.
    // Caller must run LightEngine::initSkyLight + initBlockLight afterwards.
    static bool deserialize(const uint8_t* data, size_t len, Chunk& chunk);

    // Serialize with palette compression (Batch 6).
    static std::vector<uint8_t> compressSerialize(const Chunk& chunk);
    static bool compressDeserialize(const uint8_t* data, size_t len, Chunk& chunk);
};
