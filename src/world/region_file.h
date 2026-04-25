#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>

// ========== RegionFile ==========
// Manages a single region file (r.X.Z.mca) containing up to 32×32 chunks.
//
// File format (simplified MC region format):
//   Header: 4096 bytes (1024 entries × 4 bytes each)
//     Each entry: [3 bytes offset_sectors | 1 byte size_sectors]
//     offset=0 means chunk not present in file.
//   Sector size: 4096 bytes
//   Data sectors (appended after header):
//     Per chunk: [4 bytes data_length] + [raw chunk data]
//
// Coordinates:
//   regionX = chunkX >> 5    (divide by 32, floor)
//   regionZ = chunkZ >> 5
//   localX  = chunkX & 31    (mod 32)
//   localZ  = chunkZ & 31

class RegionFile {
public:
    static constexpr int REGION_SIZE = 32;   // 32×32 chunks per region
    static constexpr int SECTOR_SIZE = 4096; // 4 KB sectors
    static constexpr int HEADER_SECTORS = 1; // 1 sector = 4096 bytes for header

    RegionFile() = default;

    // Open or create a region file. Returns false on I/O error.
    bool open(const std::string& filepath);

    // Close file (flushes). Called by destructor.
    void close();

    ~RegionFile();

    RegionFile(const RegionFile&) = delete;
    RegionFile& operator=(const RegionFile&) = delete;

    bool isOpen() const { return file_.is_open(); }

    // Read chunk data from region file. Returns empty vector if chunk not saved.
    // localX, localZ in [0, 31].
    std::vector<uint8_t> readChunk(int localX, int localZ);

    // Write chunk data to region file. Allocates or reuses sectors as needed.
    // localX, localZ in [0, 31].
    bool writeChunk(int localX, int localZ, const std::vector<uint8_t>& data);

    // Check if a chunk exists in this region file.
    bool hasChunk(int localX, int localZ) const;

    // Static helpers for coordinate conversion
    static int chunkToRegion(int chunkCoord) {
        return chunkCoord >> 5;  // floor division by 32
    }
    static int chunkToLocal(int chunkCoord) {
        return chunkCoord & 31;  // mod 32
    }
    static std::string regionFilename(int regionX, int regionZ) {
        return "r." + std::to_string(regionX) + "." + std::to_string(regionZ) + ".mca";
    }

private:
    std::fstream file_;
    std::string filepath_;

    // Header: offset (in sectors) and size (in sectors) for each chunk slot.
    struct ChunkEntry {
        uint32_t offsetSectors = 0;  // 0 = not present
        uint8_t  sizeSectors   = 0;
    };
    ChunkEntry header_[REGION_SIZE * REGION_SIZE] = {};

    // Total sectors in file (for appending new data)
    uint32_t totalSectors_ = 0;

    int headerIndex(int localX, int localZ) const {
        return localZ * REGION_SIZE + localX;
    }

    bool readHeader();
    void writeHeaderEntry(int index);
    void ensureFileSize(uint32_t sectors);
};
