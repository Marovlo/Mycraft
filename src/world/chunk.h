#pragma once

#include "core/common.h"
#include "core/block.h"
#include "engine/vulkan_engine.h"

#include <array>
#include <atomic>

// ========== Chunk State Machine ==========
// 区块从创建到可渲染的生命周期状态。
// 状态转换由 ChunkTaskManager 驱动，主线程和工作线程各负责不同阶段。
//
//   Empty → Pending → Generating → DataReady → MeshPending → MeshBuilding → Ready
//                                                                              ↓
//                                                                        (方块修改)
//                                                                              ↓
//                                                                        MeshDirty → MeshPending → ...
enum class ChunkState : uint8_t {
    Empty,          // 刚创建，无数据
    Pending,        // 已提交到工作线程队列，等待生成/加载
    Generating,     // 工作线程正在生成/加载中
    DataReady,      // 数据已生成，等待主线程写入 World
    MeshPending,    // 数据已在 World 中，等待 mesh 构建
    MeshBuilding,   // 工作线程正在构建 mesh
    Ready,          // mesh 已上传 GPU，可渲染
};

// ========== Chunk ==========
// A 16x256x16 column of blocks.
// Uses flat array for cache-friendly access (Y is the fast-changing axis
// since vertical operations like lighting are common).
// Access pattern: blocks_[x * CHUNK_HEIGHT * CHUNK_SIZE + y * CHUNK_SIZE + z]

class Chunk {
public:
    Chunk() = default;
    Chunk(int cx, int cz) : cx_(cx), cz_(cz) {}

    // std::atomic 不可拷贝/移动，需要手动定义移动构造和移动赋值
    Chunk(Chunk&& other) noexcept
        : blocks_(std::move(other.blocks_)),
          lightData_(std::move(other.lightData_)),
          heightMap_(std::move(other.heightMap_)),
          cx_(other.cx_), cz_(other.cz_),
          mesh_(other.mesh_), transparentMesh_(other.transparentMesh_),
          meshDirty_(other.meshDirty_), hasData_(other.hasData_),
          lightDirty_(other.lightDirty_), isModified_(other.isModified_),
          state_(other.state_.load(std::memory_order_relaxed))
    {
        other.mesh_ = {};
        other.transparentMesh_ = {};
    }

    Chunk& operator=(Chunk&& other) noexcept {
        if (this != &other) {
            blocks_ = std::move(other.blocks_);
            lightData_ = std::move(other.lightData_);
            heightMap_ = std::move(other.heightMap_);
            cx_ = other.cx_;
            cz_ = other.cz_;
            mesh_ = other.mesh_;
            transparentMesh_ = other.transparentMesh_;
            meshDirty_ = other.meshDirty_;
            hasData_ = other.hasData_;
            lightDirty_ = other.lightDirty_;
            isModified_ = other.isModified_;
            state_.store(other.state_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            other.mesh_ = {};
            other.transparentMesh_ = {};
        }
        return *this;
    }

    // 禁止拷贝
    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;

    // Block access (local coordinates: x,z in [0, CHUNK_SIZE), y in [0, CHUNK_HEIGHT))
    BlockId getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockId id);

    // Light access — packed: upper 4 bits = skyLight, lower 4 bits = blockLight
    uint8_t getSkyLight(int x, int y, int z) const;
    uint8_t getBlockLight(int x, int y, int z) const;
    uint8_t getMaxLight(int x, int y, int z) const;
    void setSkyLight(int x, int y, int z, uint8_t val);
    void setBlockLight(int x, int y, int z, uint8_t val);

    // Heightmap: highest non-transparent Y per column (for sky light fast-path)
    uint8_t getHeight(int x, int z) const { return heightMap_[x * CHUNK_SIZE + z]; }
    void updateHeightMap();

    // Chunk position in chunk coordinates
    int chunkX() const { return cx_; }
    int chunkZ() const { return cz_; }

    int worldX() const { return cx_ * CHUNK_SIZE; }
    int worldZ() const { return cz_ * CHUNK_SIZE; }

    // Mesh management — opaque pass
    Mesh& getMesh() { return mesh_; }
    const Mesh& getMesh() const { return mesh_; }
    void setMesh(Mesh mesh) { mesh_ = mesh; }
    bool hasMesh() const { return mesh_.indexCount > 0; }

    // Mesh management — transparent pass (water, glass, etc.)
    Mesh& getTransparentMesh() { return transparentMesh_; }
    const Mesh& getTransparentMesh() const { return transparentMesh_; }
    void setTransparentMesh(Mesh mesh) { transparentMesh_ = mesh; }
    bool hasTransparentMesh() const { return transparentMesh_.indexCount > 0; }

    bool isMeshDirty() const { return meshDirty_; }
    void markMeshDirty() { meshDirty_ = true; }
    void clearMeshDirty() { meshDirty_ = false; }

    bool hasData() const { return hasData_; }
    void markHasData() { hasData_ = true; }

    // 区块状态机（线程安全读写）
    ChunkState state() const { return state_.load(std::memory_order_acquire); }
    void setState(ChunkState s) { state_.store(s, std::memory_order_release); }

    // Light dirty flag (separate from mesh dirty — light recalc then triggers mesh rebuild)
    bool isLightDirty() const { return lightDirty_; }
    void markLightDirty() { lightDirty_ = true; }
    void clearLightDirty() { lightDirty_ = false; }

    // Modified flag — set when player places/breaks blocks. Used by save system
    // to decide which chunks need persistence (unmodified = regenerated from seed).
    bool isModified() const { return isModified_; }
    void markModified() { isModified_ = true; }
    void clearModified() { isModified_ = false; }

    // Direct access to blocks array (for serialization — avoid per-block copies)
    const BlockId* blocksData() const { return blocks_.data(); }
    BlockId* blocksData() { return blocks_.data(); }
    static constexpr int blockCount() { return BLOCK_COUNT; }

private:
    static constexpr int BLOCK_COUNT = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;

    std::array<BlockId, BLOCK_COUNT> blocks_ = {};
    std::array<uint8_t, BLOCK_COUNT> lightData_ = {};  // packed: sky<<4 | block
    std::array<uint8_t, CHUNK_SIZE * CHUNK_SIZE> heightMap_ = {};

    int cx_ = 0, cz_ = 0;

    Mesh mesh_;
    Mesh transparentMesh_;
    bool meshDirty_ = true;
    bool hasData_ = false;
    bool lightDirty_ = true;
    bool isModified_ = false;
    std::atomic<ChunkState> state_{ChunkState::Empty};

    int blockIndex(int x, int y, int z) const {
        return x * CHUNK_HEIGHT * CHUNK_SIZE + y * CHUNK_SIZE + z;
    }
};
