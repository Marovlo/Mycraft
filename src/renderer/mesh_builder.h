#pragma once

#include "engine/vulkan_engine.h"
#include "core/common.h"
#include "core/block.h"
#include "world/chunk.h"
#include "world/world.h"

#include <vector>

class TextureAtlas;

// Builds the vertex/index data for a chunk by inspecting neighbors
// and culling hidden faces.
class MeshBuilder {
public:
    // Set the texture atlas for UV coordinate computation (must be called before build)
    void setAtlas(const TextureAtlas* atlas) { atlas_ = atlas; }

    // Build mesh data for a chunk.
    // Requires access to the world for cross-chunk neighbor lookups.
    void build(const World& world, const Chunk& chunk);

    // Get results after build()
    const std::vector<Vertex>& getVertices() const { return vertices_; }
    const std::vector<uint32_t>& getIndices() const { return indices_; }

    bool isEmpty() const { return indices_.empty(); }

private:
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    const TextureAtlas* atlas_ = nullptr;

    // Face geometry for each direction
    struct FaceQuad {
        glm::vec3 v0, v1, v2, v3;  // Counter-clockwise winding
    };

    static FaceQuad getFaceQuad(Direction dir);

    void addFace(const glm::vec3& blockPos, Direction dir, uint16_t texId);
};
