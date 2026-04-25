#include "serialization.h"

#include <cstring>
#include <algorithm>

// ========== BinaryWriter ==========

BinaryWriter::BinaryWriter(const std::string& filepath)
    : useBuffer_(false)
{
    stream_.open(filepath, std::ios::binary | std::ios::trunc);
}

BinaryWriter::BinaryWriter()
    : useBuffer_(true)
{
    buffer_.reserve(4096);
}

BinaryWriter::~BinaryWriter() {
    close();
}

void BinaryWriter::writeHeader(VCFile::Type type, uint16_t version) {
    writeU32(VCFile::MAGIC);
    writeU16(version);
    writeU16(static_cast<uint16_t>(type));
}

void BinaryWriter::writeU8(uint8_t val) { write(&val, 1); }
void BinaryWriter::writeU16(uint16_t val) { write(&val, 2); }
void BinaryWriter::writeU32(uint32_t val) { write(&val, 4); }
void BinaryWriter::writeU64(uint64_t val) { write(&val, 8); }
void BinaryWriter::writeI32(int32_t val) { write(&val, 4); }
void BinaryWriter::writeI64(int64_t val) { write(&val, 8); }

void BinaryWriter::writeF32(float val) {
    uint32_t bits;
    std::memcpy(&bits, &val, 4);
    write(&bits, 4);
}

void BinaryWriter::writeF64(double val) {
    uint64_t bits;
    std::memcpy(&bits, &val, 8);
    write(&bits, 8);
}

void BinaryWriter::writeString(const std::string& val) {
    uint16_t len = static_cast<uint16_t>(std::min<size_t>(val.size(), 65535));
    writeU16(len);
    write(val.data(), len);
}

void BinaryWriter::writeBytes(const uint8_t* data, size_t len) {
    write(data, len);
}

void BinaryWriter::writeBytes(const void* data, size_t len) {
    write(data, len);
}

void BinaryWriter::close() {
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

size_t BinaryWriter::position() const {
    if (useBuffer_) return buffer_.size();
    // const_cast because tellp isn't const in all implementations
    return static_cast<size_t>(const_cast<std::ofstream&>(stream_).tellp());
}

void BinaryWriter::write(const void* data, size_t len) {
    if (len == 0) return;
    if (useBuffer_) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + len);
    } else {
        stream_.write(static_cast<const char*>(data), static_cast<std::streamsize>(len));
    }
}

// ========== BinaryReader ==========

BinaryReader::BinaryReader(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        valid_ = false;
        return;
    }

    auto fileSize = file.tellg();
    if (fileSize <= 0) {
        valid_ = false;
        return;
    }

    data_.resize(static_cast<size_t>(fileSize));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data_.data()), fileSize);
    valid_ = file.good();
}

BinaryReader::BinaryReader(const uint8_t* data, size_t len)
    : data_(data, data + len), valid_(len > 0)
{
}

bool BinaryReader::eof() const {
    return pos_ >= data_.size();
}

bool BinaryReader::readHeader(VCFile::Type expectedType, uint16_t maxVersion,
                              uint16_t& outVersion) {
    if (remaining() < 8) return false;

    uint32_t magic = readU32();
    if (magic != VCFile::MAGIC) return false;

    outVersion = readU16();
    if (outVersion > maxVersion) return false;

    uint16_t type = readU16();
    if (type != static_cast<uint16_t>(expectedType)) return false;

    return true;
}

uint8_t BinaryReader::readU8() {
    uint8_t val = 0;
    read(&val, 1);
    return val;
}

uint16_t BinaryReader::readU16() {
    uint16_t val = 0;
    read(&val, 2);
    return val;
}

uint32_t BinaryReader::readU32() {
    uint32_t val = 0;
    read(&val, 4);
    return val;
}

uint64_t BinaryReader::readU64() {
    uint64_t val = 0;
    read(&val, 8);
    return val;
}

int32_t BinaryReader::readI32() {
    int32_t val = 0;
    read(&val, 4);
    return val;
}

int64_t BinaryReader::readI64() {
    int64_t val = 0;
    read(&val, 8);
    return val;
}

float BinaryReader::readF32() {
    uint32_t bits = 0;
    read(&bits, 4);
    float val;
    std::memcpy(&val, &bits, 4);
    return val;
}

double BinaryReader::readF64() {
    uint64_t bits = 0;
    read(&bits, 8);
    double val;
    std::memcpy(&val, &bits, 8);
    return val;
}

std::string BinaryReader::readString() {
    uint16_t len = readU16();
    if (remaining() < len) {
        valid_ = false;
        return {};
    }
    std::string str(reinterpret_cast<const char*>(data_.data() + pos_), len);
    pos_ += len;
    return str;
}

void BinaryReader::readBytes(uint8_t* buf, size_t len) {
    read(buf, len);
}

void BinaryReader::readBytes(void* buf, size_t len) {
    read(buf, len);
}

void BinaryReader::skip(size_t bytes) {
    if (pos_ + bytes > data_.size()) {
        valid_ = false;
        pos_ = data_.size();
    } else {
        pos_ += bytes;
    }
}

void BinaryReader::read(void* dst, size_t len) {
    if (len == 0) return;
    if (pos_ + len > data_.size()) {
        valid_ = false;
        std::memset(dst, 0, len);
        return;
    }
    std::memcpy(dst, data_.data() + pos_, len);
    pos_ += len;
}
