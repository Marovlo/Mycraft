#include "region_file.h"

#include <cstring>
#include <algorithm>
#include <iostream>

RegionFile::~RegionFile() {
    close();
}

bool RegionFile::open(const std::string& filepath) {
    filepath_ = filepath;

    // Try opening existing file for read+write
    file_.open(filepath, std::ios::in | std::ios::out | std::ios::binary);

    if (!file_.is_open()) {
        // File doesn't exist — create it with an empty header
        file_.open(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file_.is_open()) return false;

        // Write empty header (4096 bytes of zeros)
        std::vector<uint8_t> emptyHeader(SECTOR_SIZE, 0);
        file_.write(reinterpret_cast<const char*>(emptyHeader.data()), SECTOR_SIZE);
        file_.flush();
        file_.close();

        // Reopen for read+write
        file_.open(filepath, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) return false;

        totalSectors_ = HEADER_SECTORS;
        return true;
    }

    // Read header from existing file
    if (!readHeader()) {
        file_.close();
        return false;
    }

    return true;
}

void RegionFile::close() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

bool RegionFile::readHeader() {
    file_.seekg(0, std::ios::end);
    auto fileEnd = file_.tellg();
    size_t fileSize = static_cast<size_t>(fileEnd);
    file_.seekg(0, std::ios::beg);

    if (fileSize < SECTOR_SIZE) {
        // Corrupt or empty file
        totalSectors_ = HEADER_SECTORS;
        return false;
    }

    totalSectors_ = static_cast<uint32_t>((fileSize + SECTOR_SIZE - 1) / SECTOR_SIZE);

    uint8_t headerBuf[SECTOR_SIZE];
    file_.read(reinterpret_cast<char*>(headerBuf), SECTOR_SIZE);
    if (!file_.good()) return false;

    for (int i = 0; i < REGION_SIZE * REGION_SIZE; ++i) {
        int off = i * 4;
        // 3 bytes big-endian offset + 1 byte size (MC convention)
        uint32_t offset = (static_cast<uint32_t>(headerBuf[off]) << 16) |
                          (static_cast<uint32_t>(headerBuf[off + 1]) << 8) |
                          (static_cast<uint32_t>(headerBuf[off + 2]));
        uint8_t  size   = headerBuf[off + 3];
        header_[i].offsetSectors = offset;
        header_[i].sizeSectors   = size;
    }

    return true;
}

void RegionFile::writeHeaderEntry(int index) {
    uint8_t buf[4];
    uint32_t offset = header_[index].offsetSectors;
    buf[0] = static_cast<uint8_t>((offset >> 16) & 0xFF);
    buf[1] = static_cast<uint8_t>((offset >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>(offset & 0xFF);
    buf[3] = header_[index].sizeSectors;

    file_.seekp(index * 4, std::ios::beg);
    file_.write(reinterpret_cast<const char*>(buf), 4);
    file_.flush();
}

void RegionFile::ensureFileSize(uint32_t sectors) {
    size_t needed = static_cast<size_t>(sectors) * SECTOR_SIZE;

    file_.seekp(0, std::ios::end);
    auto current = file_.tellp();

    if (static_cast<size_t>(current) < needed) {
        size_t pad = needed - static_cast<size_t>(current);
        std::vector<uint8_t> zeros(pad, 0);
        file_.write(reinterpret_cast<const char*>(zeros.data()),
                    static_cast<std::streamsize>(pad));
    }
}

bool RegionFile::hasChunk(int localX, int localZ) const {
    int idx = headerIndex(localX, localZ);
    return header_[idx].offsetSectors != 0;
}

std::vector<uint8_t> RegionFile::readChunk(int localX, int localZ) {
    int idx = headerIndex(localX, localZ);
    if (header_[idx].offsetSectors == 0) return {};

    uint32_t offset = header_[idx].offsetSectors;

    // Seek to sector
    file_.seekg(static_cast<std::streamoff>(offset) * SECTOR_SIZE, std::ios::beg);

    // Read data length (first 4 bytes of the sector)
    uint8_t lenBuf[4];
    file_.read(reinterpret_cast<char*>(lenBuf), 4);
    if (!file_.good()) return {};

    uint32_t dataLen = static_cast<uint32_t>(lenBuf[0]) |
                       (static_cast<uint32_t>(lenBuf[1]) << 8) |
                       (static_cast<uint32_t>(lenBuf[2]) << 16) |
                       (static_cast<uint32_t>(lenBuf[3]) << 24);

    if (dataLen == 0 || dataLen > 16 * 1024 * 1024) return {};  // sanity check: max 16 MB

    std::vector<uint8_t> data(dataLen);
    file_.read(reinterpret_cast<char*>(data.data()), dataLen);
    if (!file_.good()) return {};

    return data;
}

bool RegionFile::writeChunk(int localX, int localZ, const std::vector<uint8_t>& data) {
    if (data.empty()) return true;

    int idx = headerIndex(localX, localZ);

    // Total bytes: 4 (length prefix) + data
    uint32_t totalBytes = 4 + static_cast<uint32_t>(data.size());
    uint32_t sectorsNeeded = (totalBytes + SECTOR_SIZE - 1) / SECTOR_SIZE;

    if (sectorsNeeded > 255) {
        std::cerr << "[Region] Chunk data too large: " << data.size() << " bytes\n";
        return false;
    }

    uint32_t offset;

    // Can we reuse the existing allocation?
    if (header_[idx].offsetSectors != 0 && header_[idx].sizeSectors >= sectorsNeeded) {
        offset = header_[idx].offsetSectors;
    } else {
        // Allocate new sectors at end of file
        offset = totalSectors_;
        totalSectors_ += sectorsNeeded;
        // Note: old sectors become "garbage" — MC does the same, region files
        // only grow. Compaction can be added later if needed.
    }

    // Ensure file is large enough
    ensureFileSize(offset + sectorsNeeded);

    // Write data length (little-endian) + data
    file_.seekp(static_cast<std::streamoff>(offset) * SECTOR_SIZE, std::ios::beg);

    uint32_t dataLen = static_cast<uint32_t>(data.size());
    uint8_t lenBuf[4] = {
        static_cast<uint8_t>(dataLen & 0xFF),
        static_cast<uint8_t>((dataLen >> 8) & 0xFF),
        static_cast<uint8_t>((dataLen >> 16) & 0xFF),
        static_cast<uint8_t>((dataLen >> 24) & 0xFF)
    };
    file_.write(reinterpret_cast<const char*>(lenBuf), 4);
    file_.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));

    // Pad remaining sector space with zeros
    uint32_t written = totalBytes;
    uint32_t padBytes = sectorsNeeded * SECTOR_SIZE - written;
    if (padBytes > 0) {
        std::vector<uint8_t> pad(padBytes, 0);
        file_.write(reinterpret_cast<const char*>(pad.data()),
                    static_cast<std::streamsize>(padBytes));
    }

    // Update header
    header_[idx].offsetSectors = offset;
    header_[idx].sizeSectors   = static_cast<uint8_t>(sectorsNeeded);
    writeHeaderEntry(idx);

    file_.flush();
    return true;
}
