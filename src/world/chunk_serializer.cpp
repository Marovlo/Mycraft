#include "chunk_serializer.h"
#include "core/serialization.h"

#include <cstring>
#include <unordered_map>
#include <algorithm>
#include <cmath>

// ========== Format flags (stored in the chunk data) ==========
// Byte after chunkX/Z: flags
//   bit 0: hasData
//   bit 1: isPaletteCompressed (0 = raw, 1 = palette)
static constexpr uint8_t FLAG_HAS_DATA   = 0x01;
static constexpr uint8_t FLAG_COMPRESSED = 0x02;

// ========== Palette compression helpers ==========

static uint8_t bitsNeeded(uint16_t paletteSize) {
    if (paletteSize <= 1) return 1;  // minimum 1 bit
    // ceil(log2(paletteSize))
    uint8_t bits = 0;
    uint16_t n = paletteSize - 1;
    while (n > 0) { n >>= 1; ++bits; }
    return bits;
}

// Pack block indices into a bit array. indices[i] uses `bitsPerBlock` bits.
// Output is tightly packed, no padding between blocks, but padded to full bytes at end.
static std::vector<uint8_t> packBits(const uint16_t* indices, int count, uint8_t bitsPerBlock) {
    size_t totalBits = static_cast<size_t>(count) * bitsPerBlock;
    size_t totalBytes = (totalBits + 7) / 8;
    std::vector<uint8_t> packed(totalBytes, 0);

    size_t bitPos = 0;
    for (int i = 0; i < count; ++i) {
        uint16_t val = indices[i];
        for (uint8_t b = 0; b < bitsPerBlock; ++b) {
            if (val & (1u << b)) {
                packed[bitPos / 8] |= (1u << (bitPos % 8));
            }
            ++bitPos;
        }
    }
    return packed;
}

// Unpack bit array back to block indices.
static void unpackBits(const uint8_t* packed, int count, uint8_t bitsPerBlock, uint16_t* indices) {
    size_t bitPos = 0;
    uint16_t mask = (1u << bitsPerBlock) - 1;
    for (int i = 0; i < count; ++i) {
        uint16_t val = 0;
        for (uint8_t b = 0; b < bitsPerBlock; ++b) {
            if (packed[bitPos / 8] & (1u << (bitPos % 8))) {
                val |= (1u << b);
            }
            ++bitPos;
        }
        indices[i] = val & mask;
    }
}

// ========== Serialize (always uses palette compression) ==========

std::vector<uint8_t> ChunkSerializer::serialize(const Chunk& chunk) {
    return compressSerialize(chunk);
}

bool ChunkSerializer::deserialize(const uint8_t* data, size_t len, Chunk& chunk) {
    if (len < 9) return false;

    // Peek at flags to determine format
    BinaryReader peek(data, len);
    peek.readI32();  // chunkX
    peek.readI32();  // chunkZ
    uint8_t flags = peek.readU8();

    if (flags & FLAG_COMPRESSED) {
        return compressDeserialize(data, len, chunk);
    } else {
        // Raw format (legacy/fallback)
        BinaryReader r(data, len);
        r.readI32();  // skip chunkX
        r.readI32();  // skip chunkZ
        r.readU8();   // skip flags

        size_t blockBytes = static_cast<size_t>(Chunk::blockCount()) * sizeof(BlockId);
        if (r.remaining() < blockBytes) return false;
        r.readBytes(reinterpret_cast<uint8_t*>(chunk.blocksData()), blockBytes);
        chunk.markHasData();
        chunk.markMeshDirty();
        return r.isValid();
    }
}

// ========== Palette-compressed serialize ==========

std::vector<uint8_t> ChunkSerializer::compressSerialize(const Chunk& chunk) {
    const int count = Chunk::blockCount();
    const BlockId* blocks = chunk.blocksData();

    // Build palette: unique block IDs
    std::unordered_map<BlockId, uint16_t> idToIndex;
    std::vector<BlockId> palette;
    palette.reserve(32);

    for (int i = 0; i < count; ++i) {
        BlockId id = blocks[i];
        if (idToIndex.find(id) == idToIndex.end()) {
            idToIndex[id] = static_cast<uint16_t>(palette.size());
            palette.push_back(id);
        }
    }

    uint16_t paletteSize = static_cast<uint16_t>(palette.size());
    uint8_t bpb = bitsNeeded(paletteSize);

    // If palette is too large (>= 256 types), the compression ratio is poor.
    // Fall back to raw format if bitsPerBlock >= 16 (no gain).
    if (bpb >= 16) {
        // Raw format
        BinaryWriter w;
        w.writeI32(chunk.chunkX());
        w.writeI32(chunk.chunkZ());
        uint8_t flags = FLAG_HAS_DATA;
        w.writeU8(flags);
        w.writeBytes(reinterpret_cast<const uint8_t*>(blocks),
                     static_cast<size_t>(count) * sizeof(BlockId));
        return std::move(w.getBuffer());
    }

    // Build index array
    std::vector<uint16_t> indices(count);
    for (int i = 0; i < count; ++i) {
        indices[i] = idToIndex[blocks[i]];
    }

    // Pack bits
    auto packed = packBits(indices.data(), count, bpb);

    // Write output
    BinaryWriter w;
    w.writeI32(chunk.chunkX());
    w.writeI32(chunk.chunkZ());

    uint8_t flags = FLAG_HAS_DATA | FLAG_COMPRESSED;
    w.writeU8(flags);

    w.writeU16(paletteSize);
    for (BlockId pid : palette) {
        w.writeU16(pid);
    }
    w.writeU8(bpb);
    w.writeU32(static_cast<uint32_t>(packed.size()));
    w.writeBytes(packed.data(), packed.size());

    return std::move(w.getBuffer());
}

// ========== Palette-compressed deserialize ==========

bool ChunkSerializer::compressDeserialize(const uint8_t* data, size_t len, Chunk& chunk) {
    BinaryReader r(data, len);
    if (!r.isValid()) return false;

    r.readI32();  // chunkX (already set)
    r.readI32();  // chunkZ (already set)
    r.readU8();   // flags (already checked)

    uint16_t paletteSize = r.readU16();
    if (paletteSize == 0) return false;

    std::vector<BlockId> palette(paletteSize);
    for (uint16_t i = 0; i < paletteSize; ++i) {
        palette[i] = r.readU16();
    }

    uint8_t bpb = r.readU8();
    if (bpb == 0 || bpb > 16) return false;

    uint32_t packedSize = r.readU32();
    if (r.remaining() < packedSize) return false;

    const int count = Chunk::blockCount();
    size_t expectedBits = static_cast<size_t>(count) * bpb;
    size_t expectedBytes = (expectedBits + 7) / 8;
    if (packedSize < expectedBytes) return false;

    // Unpack indices
    const uint8_t* packed = data + r.position();
    std::vector<uint16_t> indices(count);
    unpackBits(packed, count, bpb, indices.data());

    // Map indices back to block IDs
    BlockId* blocks = chunk.blocksData();
    for (int i = 0; i < count; ++i) {
        uint16_t idx = indices[i];
        if (idx >= paletteSize) return false;  // corrupt data
        blocks[i] = palette[idx];
    }

    chunk.markHasData();
    chunk.markMeshDirty();
    return true;
}
