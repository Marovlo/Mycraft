#include "block_model.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>

void BlockModelRenderer::init(VulkanEngine& engine, const TextureAtlas& atlas) {
    // Mesh is built on first use per block type
}

void BlockModelRenderer::destroy(VulkanEngine& engine) {
    if (cubeMesh_.indexCount > 0) {
        engine.destroyMesh(cubeMesh_);
        cubeMesh_ = {};
    }
}

glm::mat4 BlockModelRenderer::getIconViewProj() const {
    // MC-style isometric view: Y-axis rotation 225°, X-axis tilt 30°
    // Orthographic projection sized to fit a unit cube
    float scale = 0.6f;
    glm::mat4 proj = glm::ortho(-scale, scale, -scale, scale, -5.0f, 5.0f);
    proj[1][1] *= -1;  // Vulkan Y flip

    glm::mat4 view = glm::mat4(1.0f);
    view = glm::rotate(view, glm::radians(30.0f), glm::vec3(1, 0, 0));   // tilt
    view = glm::rotate(view, glm::radians(225.0f), glm::vec3(0, 1, 0));  // rotate
    view = glm::translate(view, glm::vec3(-0.5f, -0.5f, -0.5f));         // center cube at origin

    return proj * view;
}

void BlockModelRenderer::buildCubeMesh(VulkanEngine& engine, const TextureAtlas& atlas, BlockId blockId) {
    if (cubeMesh_.indexCount > 0) {
        engine.destroyMesh(cubeMesh_);
        cubeMesh_ = {};
    }

    const auto& reg = BlockRegistry::instance();
    const auto& props = reg.get(blockId);

    // 6 faces, each with 4 vertices and 6 indices
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;
    verts.reserve(24);
    idx.reserve(36);

    struct Face {
        glm::vec3 v0, v1, v2, v3;
        glm::vec3 normal;
        uint16_t texId;
    };

    // Get texture tile indices for each face
    uint16_t topTex    = props.textures.top;
    uint16_t bottomTex = props.textures.bottom;
    uint16_t northTex  = props.textures.north;
    uint16_t southTex  = props.textures.south;
    uint16_t eastTex   = props.textures.east;
    uint16_t westTex   = props.textures.west;

    Face faces[] = {
        // PosY (top)
        {{0,1,1},{1,1,1},{1,1,0},{0,1,0}, {0,1,0}, topTex},
        // NegY (bottom)
        {{0,0,0},{1,0,0},{1,0,1},{0,0,1}, {0,-1,0}, bottomTex},
        // PosX (east)
        {{1,0,0},{1,1,0},{1,1,1},{1,0,1}, {1,0,0}, eastTex},
        // NegX (west)
        {{0,0,1},{0,1,1},{0,1,0},{0,0,0}, {-1,0,0}, westTex},
        // PosZ (south)
        {{1,0,1},{1,1,1},{0,1,1},{0,0,1}, {0,0,1}, southTex},
        // NegZ (north)
        {{0,0,0},{0,1,0},{1,1,0},{1,0,0}, {0,0,-1}, northTex},
    };

    for (auto& f : faces) {
        glm::vec4 uvRect = atlas.getTileUV(f.texId);
        glm::vec2 uv0(uvRect.x, uvRect.y);
        glm::vec2 uv1(uvRect.x, uvRect.w);
        glm::vec2 uv2(uvRect.z, uvRect.w);
        glm::vec2 uv3(uvRect.z, uvRect.y);

        uint32_t base = static_cast<uint32_t>(verts.size());
        verts.push_back({f.v0, f.normal, uv0});
        verts.push_back({f.v1, f.normal, uv1});
        verts.push_back({f.v2, f.normal, uv2});
        verts.push_back({f.v3, f.normal, uv3});

        idx.push_back(base + 0);
        idx.push_back(base + 1);
        idx.push_back(base + 2);
        idx.push_back(base + 0);
        idx.push_back(base + 2);
        idx.push_back(base + 3);
    }

    cubeMesh_ = engine.uploadMesh(verts, idx);
    lastBuiltBlock_ = blockId;
}

void BlockModelRenderer::renderBlockIcon(VkCommandBuffer cmd, VulkanEngine& engine,
                                          BlockId blockId, const TextureAtlas& atlas,
                                          float screenX, float screenY, float size,
                                          uint32_t fullScreenW, uint32_t fullScreenH) {
    if (blockId == Block::Air) return;

    // Rebuild mesh if block type changed
    if (blockId != lastBuiltBlock_) {
        buildCubeMesh(engine, atlas, blockId);
    }

    if (cubeMesh_.indexCount == 0) return;

    // Set viewport to the icon rect
    VkViewport vp{};
    vp.x = screenX;
    vp.y = screenY;
    vp.width = size;
    vp.height = size;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.offset = {static_cast<int32_t>(screenX), static_cast<int32_t>(screenY)};
    scissor.extent = {static_cast<uint32_t>(size), static_cast<uint32_t>(size)};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Update UBO with isometric view-projection
    glm::mat4 viewProj = getIconViewProj();
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = viewProj;  // combined view-proj in view slot
    ubo.proj = glm::mat4(1.0f);  // identity (already combined)
    ubo.fogColor = glm::vec4(0, 0, 0, 0);
    ubo.viewPos = glm::vec4(0, 0, 0, 0);
    ubo.fogRange = glm::vec2(9999.0f, 99999.0f);  // disable fog for icons
    engine.updateUniformBuffer(ubo);

    // Draw
    VkBuffer vb[] = {cubeMesh_.vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, cubeMesh_.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, cubeMesh_.indexCount, 1, 0, 0, 0);
}
