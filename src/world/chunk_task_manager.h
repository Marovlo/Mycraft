#pragma once

#include "core/thread_pool.h"
#include "core/common.h"
#include "world/chunk.h"
#include "world/terrain_generator.h"
#include "world/save_manager.h"
#include "renderer/mesh_builder.h"

#include <vector>
#include <unordered_set>
#include <mutex>
#include <memory>

// ========== ChunkTaskManager ==========
// 管理区块的异步生成、加载和 mesh 构建。
// 主线程提交任务，工作线程执行 CPU 密集型工作，
// 主线程轮询结果并执行 GPU 上传（Vulkan 必须在主线程）。
//
// 设计原则（对标 MC Java Edition）：
// - 地形生成和 mesh 构建在工作线程池中并行执行
// - 每个工作线程拥有独立的 MeshBuilder（避免锁）
// - TerrainGenerator 的 GetNoise() 是 const 线程安全的
// - SaveManager 的 loadChunk 需要加锁（共享 RegionFile 缓存）
// - GPU 上传（uploadMesh）仅在主线程执行
// - 每帧限制 GPU 上传数量，避免单帧卡顿

class ChunkTaskManager {
public:
    // 地形生成完成的结果
    struct GenResult {
        int cx, cz;
        // 区块数据直接写入 Chunk 对象（通过指针），
        // 这里只传递坐标用于主线程后续处理
        bool fromDisk;  // true = 从磁盘加载, false = 新生成
    };

    // Mesh 构建完成的结果
    struct MeshResult {
        int cx, cz;
        std::vector<Vertex>   opaqueVerts;
        std::vector<uint32_t> opaqueIndices;
        std::vector<Vertex>   transVerts;
        std::vector<uint32_t> transIndices;
    };

    ChunkTaskManager() = default;
    ~ChunkTaskManager() { shutdown(); }

    // 初始化线程池和工作资源
    // numThreads=0 表示自动检测
    void init(int numThreads, TerrainGenerator* terrainGen,
              SaveManager* saveManager, const TextureAtlas* atlas);

    void shutdown();

    // === 主线程调用 ===

    // 提交区块生成/加载任务（主线程调用）
    // chunk 必须已经在 World 中创建好（getOrCreateChunk），状态为 Empty
    void submitGenTask(Chunk& chunk);

    // 提交 mesh 构建任务（主线程调用）
    // 要求区块及其4个邻居都已有数据
    void submitMeshTask(Chunk& chunk, const World& world);

    // 轮询生成完成的结果（主线程每帧调用）
    // 返回本次取出的结果数量
    int pollGenResults(std::vector<GenResult>& out, int maxCount = 16);

    // 轮询 mesh 构建完成的结果（主线程每帧调用）
    int pollMeshResults(std::vector<MeshResult>& out, int maxCount = 8);

    // 检查某个区块是否正在处理中（避免重复提交）
    bool isInFlight(int cx, int cz) const;

    // 统计信息
    int pendingGenTasks() const { return static_cast<int>(genResultQueue_.size()); }
    int pendingMeshTasks() const { return static_cast<int>(meshResultQueue_.size()); }
    int threadCount() const { return pool_.threadCount(); }

private:
    ThreadPool pool_;

    // 生成结果队列（工作线程写入，主线程读取）
    ConcurrentQueue<GenResult> genResultQueue_;

    // Mesh 结果队列（工作线程写入，主线程读取）
    ConcurrentQueue<MeshResult> meshResultQueue_;

    // 正在处理中的区块集合（避免重复提交）
    mutable std::mutex inflightMutex_;
    std::unordered_set<uint64_t> inflightChunks_;

    // 共享资源
    TerrainGenerator* terrainGen_ = nullptr;
    SaveManager* saveManager_ = nullptr;
    const TextureAtlas* atlas_ = nullptr;

    // SaveManager 的 loadChunk 不是线程安全的（共享 RegionFile 缓存），需要加锁
    std::mutex saveMutex_;

    // 每个工作线程的 MeshBuilder（thread_local 替代方案：用 pool 索引）
    // 由于 std::function 不支持 thread_local，我们用 mutex + pool 的方式
    std::mutex meshBuilderPoolMutex_;
    std::vector<std::unique_ptr<MeshBuilder>> meshBuilderPool_;
    std::vector<bool> meshBuilderInUse_;

    MeshBuilder* acquireMeshBuilder();
    void releaseMeshBuilder(MeshBuilder* builder);

    // 辅助：打包/解包区块坐标为 uint64_t key
    static uint64_t packKey(int cx, int cz) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
               static_cast<uint64_t>(static_cast<uint32_t>(cz));
    }

    void addInflight(int cx, int cz);
    void removeInflight(int cx, int cz);
};
