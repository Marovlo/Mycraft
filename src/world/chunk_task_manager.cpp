#include "chunk_task_manager.h"
#include "world/light_engine.h"
#include "world/biome_colormap.h"
#include "core/debug.h"

#include <algorithm>
#include <cassert>

void ChunkTaskManager::init(int numThreads, TerrainGenerator* terrainGen,
                            SaveManager* saveManager, const TextureAtlas* atlas,
                            const BiomeColorMap* biomeColorMap) {
    terrainGen_ = terrainGen;
    saveManager_ = saveManager;
    atlas_ = atlas;
    biomeColorMap_ = biomeColorMap;

    pool_.init(numThreads);

    // 预创建 MeshBuilder 池（每个工作线程一个）
    int poolSize = pool_.threadCount();
    meshBuilderPool_.resize(poolSize);
    meshBuilderInUse_.resize(poolSize, false);
    for (int i = 0; i < poolSize; ++i) {
        meshBuilderPool_[i] = std::make_unique<MeshBuilder>();
        meshBuilderPool_[i]->setAtlas(atlas_);
        meshBuilderPool_[i]->setBiomeColorMap(biomeColorMap_);
        meshBuilderPool_[i]->setTerrainGenerator(dynamic_cast<OverworldGenerator*>(terrainGen_));
    }

    VLOG(DebugCat::General, "ChunkTaskManager: initialized with %d worker threads", poolSize);
}

void ChunkTaskManager::shutdown() {
    pool_.shutdown();
    meshBuilderPool_.clear();
    meshBuilderInUse_.clear();
    {
        std::lock_guard<std::mutex> lock(inflightMutex_);
        inflightChunks_.clear();
    }
}

// ============================================================
// Inflight tracking
// ============================================================

void ChunkTaskManager::addInflight(int cx, int cz) {
    std::lock_guard<std::mutex> lock(inflightMutex_);
    inflightChunks_.insert(packKey(cx, cz));
}

void ChunkTaskManager::removeInflight(int cx, int cz) {
    std::lock_guard<std::mutex> lock(inflightMutex_);
    inflightChunks_.erase(packKey(cx, cz));
}

bool ChunkTaskManager::isInFlight(int cx, int cz) const {
    std::lock_guard<std::mutex> lock(inflightMutex_);
    return inflightChunks_.count(packKey(cx, cz)) > 0;
}

// ============================================================
// MeshBuilder pool
// ============================================================

MeshBuilder* ChunkTaskManager::acquireMeshBuilder() {
    std::lock_guard<std::mutex> lock(meshBuilderPoolMutex_);
    for (size_t i = 0; i < meshBuilderInUse_.size(); ++i) {
        if (!meshBuilderInUse_[i]) {
            meshBuilderInUse_[i] = true;
            return meshBuilderPool_[i].get();
        }
    }
    // 所有 builder 都在使用中 — 动态扩展（不应该发生，但安全起见）
    auto newBuilder = std::make_unique<MeshBuilder>();
    newBuilder->setAtlas(atlas_);
    newBuilder->setBiomeColorMap(biomeColorMap_);
    newBuilder->setTerrainGenerator(dynamic_cast<OverworldGenerator*>(terrainGen_));
    MeshBuilder* ptr = newBuilder.get();
    meshBuilderPool_.push_back(std::move(newBuilder));
    meshBuilderInUse_.push_back(true);
    return ptr;
}

void ChunkTaskManager::releaseMeshBuilder(MeshBuilder* builder) {
    std::lock_guard<std::mutex> lock(meshBuilderPoolMutex_);
    for (size_t i = 0; i < meshBuilderPool_.size(); ++i) {
        if (meshBuilderPool_[i].get() == builder) {
            meshBuilderInUse_[i] = false;
            return;
        }
    }
}

// ============================================================
// 提交地形生成/加载任务
// ============================================================

void ChunkTaskManager::submitGenTask(Chunk& chunk) {
    int cx = chunk.chunkX();
    int cz = chunk.chunkZ();

    // 标记为 Pending，防止重复提交
    chunk.setState(ChunkState::Pending);
    addInflight(cx, cz);

    // 捕获 chunk 指针 — 安全性保证：
    // chunk 在 World::chunks_ 的 unordered_map 中，只要不被 removeChunk，
    // 指针稳定。主线程在 unloadDistantChunks 时会检查状态，不会卸载正在处理的区块。
    Chunk* chunkPtr = &chunk;

    pool_.submitTask([this, chunkPtr, cx, cz]() {
        chunkPtr->setState(ChunkState::Generating);

        // 尝试从磁盘加载
        bool fromDisk = false;
        {
            std::lock_guard<std::mutex> lock(saveMutex_);
            fromDisk = saveManager_->loadChunk(cx, cz, *chunkPtr);
        }

        if (fromDisk) {
            // 从磁盘加载成功，重算光照
            chunkPtr->updateHeightMap();
            LightEngine::initSkyLight(*chunkPtr);
            LightEngine::initBlockLight(*chunkPtr);
        } else {
            // 从种子生成（terrainGen_->generate 内部已包含光照初始化）
            terrainGen_->generate(*chunkPtr);
        }

        // 推入完成队列
        genResultQueue_.push({cx, cz, fromDisk});
    });
}

// ============================================================
// 提交 mesh 构建任务
// ============================================================

void ChunkTaskManager::submitMeshTask(Chunk& chunk, const World& world) {
    int cx = chunk.chunkX();
    int cz = chunk.chunkZ();

    chunk.setState(ChunkState::MeshBuilding);
    addInflight(cx, cz);

    // 在主线程中捕获邻居区块指针（只读访问，数据已生成完毕不会变）。
    // 这避免了工作线程访问 World 的 unordered_map（非线程安全）。
    // 指针稳定性保证：unordered_map 中已有元素的指针不会因插入新元素而失效。
    ChunkNeighbors neighbors;
    neighbors.self = &chunk;
    neighbors.posX = world.getChunk(cx + 1, cz);
    neighbors.negX = world.getChunk(cx - 1, cz);
    neighbors.posZ = world.getChunk(cx, cz + 1);
    neighbors.negZ = world.getChunk(cx, cz - 1);

    pool_.submitTask([this, neighbors, cx, cz]() {
        MeshBuilder* builder = acquireMeshBuilder();

        builder->build(neighbors);

        MeshResult result;
        result.cx = cx;
        result.cz = cz;

        // 移动顶点数据到结果中（避免拷贝）
        if (!builder->isEmpty()) {
            result.opaqueVerts = builder->takeVertices();
            result.opaqueIndices = builder->takeIndices();
        }
        if (!builder->isTransparentEmpty()) {
            result.transVerts = builder->takeTransparentVertices();
            result.transIndices = builder->takeTransparentIndices();
        }

        releaseMeshBuilder(builder);

        meshResultQueue_.push(std::move(result));
    });
}

// ============================================================
// 轮询结果
// ============================================================

int ChunkTaskManager::pollGenResults(std::vector<GenResult>& out, int maxCount) {
    return genResultQueue_.tryPopBatch(out, maxCount);
}

int ChunkTaskManager::pollMeshResults(std::vector<MeshResult>& out, int maxCount) {
    return meshResultQueue_.tryPopBatch(out, maxCount);
}
