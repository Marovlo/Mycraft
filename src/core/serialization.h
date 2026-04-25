#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

// ========== Mycraft Binary File Header ==========
// All Mycraft data files start with this 8-byte header.

namespace VCFile {
// Magic: "VCFT" (Mycraft File)
    constexpr uint32_t MAGIC = 0x54464356;  // "VCFT" in little-endian

    // File types
    enum class Type : uint16_t {
        Level    = 0,   // level.dat (world metadata)
        Region   = 1,   // r.X.Z.mca (region file)
        Player   = 2,   // player.dat
        Entities = 3,   // entities.dat (dropped items, etc.)
    };

    // Current format versions (increment when breaking changes occur)
    constexpr uint16_t VERSION_LEVEL    = 1;
    constexpr uint16_t VERSION_REGION   = 1;
    constexpr uint16_t VERSION_PLAYER   = 1;
    constexpr uint16_t VERSION_ENTITIES = 1;
}

// ========== BinaryWriter ==========
// Writes primitive types in little-endian to a binary file.
// Usage:
//   BinaryWriter w("path/to/file");
//   w.writeU32(magic); w.writeString("hello");
//   w.close();  // or let destructor close

class BinaryWriter {
public:
    // Opens file for writing. Truncates if exists.
    explicit BinaryWriter(const std::string& filepath);

    // Opens to an internal buffer (no file). Use getBuffer() to retrieve.
    BinaryWriter();

    ~BinaryWriter();

    BinaryWriter(const BinaryWriter&) = delete;
    BinaryWriter& operator=(const BinaryWriter&) = delete;

    bool isValid() const { return useBuffer_ || (stream_.is_open() && stream_.good()); }

// Write the standard Mycraft file header
    void writeHeader(VCFile::Type type, uint16_t version);

    // Primitive writers (all little-endian — native on x86/ARM)
    void writeU8(uint8_t val);
    void writeU16(uint16_t val);
    void writeU32(uint32_t val);
    void writeU64(uint64_t val);
    void writeI32(int32_t val);
    void writeI64(int64_t val);
    void writeF32(float val);
    void writeF64(double val);

    // Length-prefixed string (uint16 length + UTF-8 bytes)
    void writeString(const std::string& val);

    // Raw bytes
    void writeBytes(const uint8_t* data, size_t len);
    void writeBytes(const void* data, size_t len);

    // Flush and close the file stream
    void close();

    // For buffer mode: get the accumulated data
    const std::vector<uint8_t>& getBuffer() const { return buffer_; }
    std::vector<uint8_t>& getBuffer() { return buffer_; }

    // Current write position (bytes written so far)
    size_t position() const;

private:
    std::ofstream stream_;
    bool useBuffer_ = false;
    std::vector<uint8_t> buffer_;

    void write(const void* data, size_t len);
};

// ========== BinaryReader ==========
// Reads primitive types in little-endian from a binary file or buffer.
// Usage:
//   BinaryReader r("path/to/file");
//   if (!r.isValid()) { /* file not found */ }
//   uint32_t magic = r.readU32();

class BinaryReader {
public:
    // Open from file
    explicit BinaryReader(const std::string& filepath);

    // Open from in-memory buffer (does NOT copy — caller must keep data alive)
    BinaryReader(const uint8_t* data, size_t len);

    ~BinaryReader() = default;

    BinaryReader(const BinaryReader&) = delete;
    BinaryReader& operator=(const BinaryReader&) = delete;

    bool isValid() const { return valid_; }
    bool eof() const;

// Read and validate the standard Mycraft file header.
    // Returns true if magic matches and version <= maxVersion.
    // On success, fills outType and outVersion.
    bool readHeader(VCFile::Type expectedType, uint16_t maxVersion,
                    uint16_t& outVersion);

    // Primitive readers
    uint8_t  readU8();
    uint16_t readU16();
    uint32_t readU32();
    uint64_t readU64();
    int32_t  readI32();
    int64_t  readI64();
    float    readF32();
    double   readF64();

    // Length-prefixed string
    std::string readString();

    // Raw bytes
    void readBytes(uint8_t* buf, size_t len);
    void readBytes(void* buf, size_t len);

    // Skip N bytes
    void skip(size_t bytes);

    // Current read position
    size_t position() const { return pos_; }

    // Total size of the data
    size_t size() const { return data_.size(); }

    // Remaining bytes
    size_t remaining() const { return pos_ < data_.size() ? data_.size() - pos_ : 0; }

private:
    std::vector<uint8_t> data_;
    size_t pos_ = 0;
    bool valid_ = false;

    void read(void* dst, size_t len);
};
