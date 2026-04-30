#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <glm/glm.hpp>

// ============================================================
// PacketBuffer - 二进制序列化/反序列化工具
// 紧凑格式，无对齐填充，网络字节序（大端）
// ============================================================

class PacketBuffer {
public:
    // === 写入模式构造 ===
    PacketBuffer() = default;

    // === 读取模式构造（从已有数据） ===
    explicit PacketBuffer(const uint8_t* data, size_t size)
        : data_(data, data + size), readPos_(0) {}

    explicit PacketBuffer(const std::vector<uint8_t>& data)
        : data_(data), readPos_(0) {}

    PacketBuffer(std::vector<uint8_t>&& data)
        : data_(std::move(data)), readPos_(0) {}

    // === 基础写入 ===
    void writeU8(uint8_t val) {
        data_.push_back(val);
    }

    void writeU16(uint16_t val) {
        data_.push_back(static_cast<uint8_t>(val >> 8));
        data_.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    void writeU32(uint32_t val) {
        data_.push_back(static_cast<uint8_t>(val >> 24));
        data_.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        data_.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        data_.push_back(static_cast<uint8_t>(val & 0xFF));
    }

    void writeU64(uint64_t val) {
        for (int i = 7; i >= 0; --i) {
            data_.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }
    }

    void writeI8(int8_t val) { writeU8(static_cast<uint8_t>(val)); }
    void writeI16(int16_t val) { writeU16(static_cast<uint16_t>(val)); }
    void writeI32(int32_t val) { writeU32(static_cast<uint32_t>(val)); }
    void writeI64(int64_t val) { writeU64(static_cast<uint64_t>(val)); }

    void writeFloat(float val) {
        uint32_t bits;
        std::memcpy(&bits, &val, sizeof(bits));
        writeU32(bits);
    }

    void writeDouble(double val) {
        uint64_t bits;
        std::memcpy(&bits, &val, sizeof(bits));
        writeU64(bits);
    }

    void writeBool(bool val) {
        writeU8(val ? 1 : 0);
    }

    // 写入字符串：[长度U16][UTF-8数据]
    void writeString(const std::string& str) {
        if (str.size() > 65535) {
            throw std::runtime_error("PacketBuffer: string too long");
        }
        writeU16(static_cast<uint16_t>(str.size()));
        data_.insert(data_.end(), str.begin(), str.end());
    }

    // 写入原始字节
    void writeBytes(const uint8_t* ptr, size_t len) {
        data_.insert(data_.end(), ptr, ptr + len);
    }

    // 写入 vec3（3 个 float）
    void writeVec3(const glm::vec3& v) {
        writeFloat(v.x);
        writeFloat(v.y);
        writeFloat(v.z);
    }

    // 写入压缩角度（单字节，256 级精度）
    void writeAngle(float degrees) {
        int val = static_cast<int>(degrees * 256.0f / 360.0f) & 0xFF;
        writeU8(static_cast<uint8_t>(val));
    }

    // 写入定点数坐标（精度 1/4096，用 int32 存储）
    void writeFixedPoint(double val) {
        writeI32(static_cast<int32_t>(val * 4096.0));
    }

    // === 基础读取 ===
    uint8_t readU8() {
        checkRead(1);
        return data_[readPos_++];
    }

    uint16_t readU16() {
        checkRead(2);
        uint16_t val = (static_cast<uint16_t>(data_[readPos_]) << 8) |
                       static_cast<uint16_t>(data_[readPos_ + 1]);
        readPos_ += 2;
        return val;
    }

    uint32_t readU32() {
        checkRead(4);
        uint32_t val = (static_cast<uint32_t>(data_[readPos_]) << 24) |
                       (static_cast<uint32_t>(data_[readPos_ + 1]) << 16) |
                       (static_cast<uint32_t>(data_[readPos_ + 2]) << 8) |
                       static_cast<uint32_t>(data_[readPos_ + 3]);
        readPos_ += 4;
        return val;
    }

    uint64_t readU64() {
        checkRead(8);
        uint64_t val = 0;
        for (int i = 0; i < 8; ++i) {
            val = (val << 8) | static_cast<uint64_t>(data_[readPos_ + i]);
        }
        readPos_ += 8;
        return val;
    }

    int8_t readI8() { return static_cast<int8_t>(readU8()); }
    int16_t readI16() { return static_cast<int16_t>(readU16()); }
    int32_t readI32() { return static_cast<int32_t>(readU32()); }
    int64_t readI64() { return static_cast<int64_t>(readU64()); }

    float readFloat() {
        uint32_t bits = readU32();
        float val;
        std::memcpy(&val, &bits, sizeof(val));
        return val;
    }

    double readDouble() {
        uint64_t bits = readU64();
        double val;
        std::memcpy(&val, &bits, sizeof(val));
        return val;
    }

    bool readBool() {
        return readU8() != 0;
    }

    std::string readString() {
        uint16_t len = readU16();
        checkRead(len);
        std::string str(reinterpret_cast<const char*>(data_.data() + readPos_), len);
        readPos_ += len;
        return str;
    }

    void readBytes(uint8_t* dst, size_t len) {
        checkRead(len);
        std::memcpy(dst, data_.data() + readPos_, len);
        readPos_ += len;
    }

    glm::vec3 readVec3() {
        float x = readFloat();
        float y = readFloat();
        float z = readFloat();
        return glm::vec3(x, y, z);
    }

    float readAngle() {
        uint8_t val = readU8();
        return static_cast<float>(val) * 360.0f / 256.0f;
    }

    double readFixedPoint() {
        int32_t val = readI32();
        return static_cast<double>(val) / 4096.0;
    }

    // === 工具方法 ===
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    size_t remaining() const { return data_.size() - readPos_; }
    bool hasMore() const { return readPos_ < data_.size(); }

    // 获取内部数据的可移动引用
    std::vector<uint8_t>& buffer() { return data_; }
    const std::vector<uint8_t>& buffer() const { return data_; }

    // 重置读取位置
    void resetRead() { readPos_ = 0; }

    // 清空所有数据
    void clear() {
        data_.clear();
        readPos_ = 0;
    }

    // 预留空间（减少 realloc）
    void reserve(size_t capacity) {
        data_.reserve(capacity);
    }

private:
    void checkRead(size_t bytes) const {
        if (readPos_ + bytes > data_.size()) {
            throw std::runtime_error("PacketBuffer: read past end of buffer");
        }
    }

    std::vector<uint8_t> data_;
    size_t readPos_ = 0;
};
