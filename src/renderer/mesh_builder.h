#pragma once

#include "engine/vulkan_engine.h"
#include "core/common.h"
#include "core/block.h"
#include "world/chunk.h"
#include "world/world.h"

#include <vector>

class TextureAtlas;
class BiomeColorMap;
class OverworldGenerator;

// 邻居区块指针集合 — 用于线程安全的 mesh 构建。
// 在主线程中捕获指针，传给工作线程使用（只读访问）。
struct ChunkNeighbors {
    const Chunk* self = nullptr;
    const Chunk* posX = nullptr;  // +X 方向邻居
    const Chunk* negX = nullptr;  // -X 方向邻居
    const Chunk* posZ = nullptr;  // +Z 方向邻居
    const Chunk* negZ = nullptr;  // -Z 方向邻居

    // 通过世界坐标获取方块（只在 self 和 4 个邻居范围内查找）
    BlockId getBlock(int wx, int wy, int wz) const;
};

// Builds the vertex/index data for a chunk by inspecting neighbors
// and culling hidden faces.
class MeshBuilder {
public:
    // Set the texture atlas for UV coordinate computation (must be called before build)
    void setAtlas(const TextureAtlas* atlas) { atlas_ = atlas; }

    // Set the biome colormap for tint color lookup
    void setBiomeColorMap(const BiomeColorMap* colorMap) { biomeColorMap_ = colorMap; }

    // Set the terrain generator for biome queries
    void setTerrainGenerator(const OverworldGenerator* gen) { terrainGen_ = gen; }

    // Build mesh data for a chunk.
    // Requires access to the world for cross-chunk neighbor lookups.
    void build(const World& world, const Chunk& chunk);

    // Thread-safe build: uses pre-captured neighbor pointers instead of World.
    // This is the preferred method for async mesh building in worker threads.
    void build(const ChunkNeighbors& neighbors);

    // Get results after build() — const ref for direct use
    const std::vector<Vertex>& getVertices() const { return vertices_; }
    const std::vector<uint32_t>& getIndices() const { return indices_; }

    // Transparent geometry (water, glass, etc.) — rendered in a separate pass
    const std::vector<Vertex>& getTransparentVertices() const { return transVertices_; }
    const std::vector<uint32_t>& getTransparentIndices() const { return transIndices_; }

    // Move results out (for async mesh building — avoids copy)
    std::vector<Vertex> takeVertices() { return std::move(vertices_); }
    std::vector<uint32_t> takeIndices() { return std::move(indices_); }
    std::vector<Vertex> takeTransparentVertices() { return std::move(transVertices_); }
    std::vector<uint32_t> takeTransparentIndices() { return std::move(transIndices_); }

    bool isEmpty() const { return indices_.empty(); }
    bool isTransparentEmpty() const { return transIndices_.empty(); }

private:
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<Vertex> transVertices_;
    std::vector<uint32_t> transIndices_;
    const TextureAtlas* atlas_ = nullptr;
    const BiomeColorMap* biomeColorMap_ = nullptr;
    const OverworldGenerator* terrainGen_ = nullptr;

    // 根据 TintType 和世界坐标获取 tint 颜色
    glm::vec3 getTintColor(TintType tintType, int wx, int wz) const;

    // Face geometry for each direction
    struct FaceQuad {
        glm::vec3 v0, v1, v2, v3;  // Counter-clockwise winding
    };

    static FaceQuad getFaceQuad(Direction dir);

    void addFace(const glm::vec3& blockPos, Direction dir, uint16_t texId, float light = 1.0f, const glm::vec3& color = glm::vec3(1.0f));
    void addTransparentFace(const glm::vec3& blockPos, Direction dir, uint16_t texId, float light = 1.0f, const glm::vec3& color = glm::vec3(1.0f));
    void addCrossFaces(const glm::vec3& blockPos, uint16_t texId, float light = 1.0f, const glm::vec3& color = glm::vec3(1.0f));
};
