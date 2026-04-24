#include "block_model.h"
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <algorithm>

void BlockModelRenderer::init(VulkanEngine& engine, const TextureAtlas& atlas) {
}

void BlockModelRenderer::destroy(VulkanEngine& engine) {
    if (cubeMesh_.indexCount > 0) {
        engine.destroyMesh(cubeMesh_);
        cubeMesh_ = {};
    }
}

glm::mat4 BlockModelRenderer::getIconViewProj() const {
    float scale = 0.6f;
    glm::mat4 proj = glm::ortho(-scale, scale, -scale, scale, -5.0f, 5.0f);
    // Don't flip Y here — we'll handle it in screen mapping

    glm::mat4 view = glm::mat4(1.0f);
    view = glm::rotate(view, glm::radians(30.0f), glm::vec3(1, 0, 0));
    view = glm::rotate(view, glm::radians(225.0f), glm::vec3(0, 1, 0));
    view = glm::translate(view, glm::vec3(-0.5f, -0.5f, -0.5f));

    return proj * view;
}

void BlockModelRenderer::renderBlockIcon(VkCommandBuffer cmd, VulkanEngine& engine,
                                          BlockId blockId, const TextureAtlas& atlas,
                                          float screenX, float screenY, float size,
                                          uint32_t fullScreenW, uint32_t fullScreenH) {
    if (blockId == Block::Air) return;

    const auto& props = BlockRegistry::instance().get(blockId);
    glm::mat4 mvp = getIconViewProj();

    struct Face {
        glm::vec3 v0, v1, v2, v3;
        glm::vec3 normal;
        uint16_t texId;
        float brightness;
    };

    Face faces[] = {
        {{0,1,1},{1,1,1},{1,1,0},{0,1,0}, {0,1,0}, props.textures.top, 1.0f},
        {{0,0,0},{1,0,0},{1,0,1},{0,0,1}, {0,-1,0}, props.textures.bottom, 0.5f},
        {{1,0,0},{1,1,0},{1,1,1},{1,0,1}, {1,0,0}, props.textures.east, 0.85f},
        {{0,0,1},{0,1,1},{0,1,0},{0,0,0}, {-1,0,0}, props.textures.west, 0.7f},
        {{1,0,1},{1,1,1},{0,1,1},{0,0,1}, {0,0,1}, props.textures.south, 0.7f},
        {{0,0,0},{0,1,0},{1,1,0},{1,0,0}, {0,0,-1}, props.textures.north, 0.8f},
    };

    // Transform world pos to screen pixel coords
    auto toScreen = [&](glm::vec3 worldPos) -> glm::vec2 {
        glm::vec4 clip = mvp * glm::vec4(worldPos, 1.0f);
        // NDC [-1,1] → [0,1] → screen pixel within icon rect
        float px = (clip.x * 0.5f + 0.5f) * size + screenX;
        float py = (-clip.y * 0.5f + 0.5f) * size + screenY;  // flip Y for screen
        return {px, py};
    };

    // Sort faces back-to-front using average Z in view space
    struct SortedFace { int index; float z; };
    SortedFace sorted[6];
    for (int i = 0; i < 6; i++) {
        glm::vec3 center = (faces[i].v0 + faces[i].v1 + faces[i].v2 + faces[i].v3) * 0.25f;
        glm::vec4 viewPos = mvp * glm::vec4(center, 1.0f);
        sorted[i] = {i, viewPos.z};
    }
    std::sort(sorted, sorted + 6, [](auto& a, auto& b) { return a.z > b.z; });

    // Build UI vertices for each visible face
    std::vector<UIVertex> uiVerts;
    std::vector<uint32_t> uiIdx;

    // Only draw front-facing faces (dot product of normal with view direction)
    glm::vec3 viewDir = glm::normalize(glm::vec3(
        glm::rotate(glm::mat4(1), glm::radians(225.0f), glm::vec3(0,1,0))
        * glm::rotate(glm::mat4(1), glm::radians(30.0f), glm::vec3(1,0,0))
        * glm::vec4(0, 0, 1, 0)));

    for (int si = 0; si < 6; si++) {
        auto& f = faces[sorted[si].index];

        // Back-face cull
        if (glm::dot(f.normal, viewDir) <= 0.0f) continue;

        glm::vec4 uvRect = atlas.getTileUV(f.texId);
        glm::vec2 uv0(uvRect.x, uvRect.y);
        glm::vec2 uv1(uvRect.x, uvRect.w);
        glm::vec2 uv2(uvRect.z, uvRect.w);
        glm::vec2 uv3(uvRect.z, uvRect.y);

        glm::vec2 sv0 = toScreen(f.v0);
        glm::vec2 sv1 = toScreen(f.v1);
        glm::vec2 sv2 = toScreen(f.v2);
        glm::vec2 sv3 = toScreen(f.v3);

        glm::vec4 tint(f.brightness, f.brightness, f.brightness, 1.0f);

        uint32_t base = static_cast<uint32_t>(uiVerts.size());
        uiVerts.push_back({sv0, uv0, tint});
        uiVerts.push_back({sv1, uv1, tint});
        uiVerts.push_back({sv2, uv2, tint});
        uiVerts.push_back({sv3, uv3, tint});

        uiIdx.push_back(base); uiIdx.push_back(base+1); uiIdx.push_back(base+2);
        uiIdx.push_back(base); uiIdx.push_back(base+2); uiIdx.push_back(base+3);
    }

    if (uiVerts.empty()) return;

    // Upload as UI mesh and draw with UI pipeline
    Mesh mesh = engine.uploadUIMesh(uiVerts, uiIdx);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine.getUIPipeline());

    VkViewport vp{};
    vp.width = static_cast<float>(fullScreenW);
    vp.height = static_cast<float>(fullScreenH);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.offset = {static_cast<int32_t>(screenX), static_cast<int32_t>(screenY)};
    scissor.extent = {static_cast<uint32_t>(size), static_cast<uint32_t>(size)};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    UIPushConstants pc{};
    pc.screenSize = {static_cast<float>(fullScreenW), static_cast<float>(fullScreenH)};
    vkCmdPushConstants(cmd, engine.getUIPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UIPushConstants), &pc);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine.getUIPipelineLayout(), 0, 1,
        &engine.getCurrentFrame().descriptorSet, 0, nullptr);

    VkBuffer vb[] = {mesh.vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);

    // Restore 3D pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine.getPipeline());

    // Cleanup (deferred to avoid GPU sync issues)
    // Store for cleanup next frame
    if (cubeMesh_.indexCount > 0) engine.destroyMesh(cubeMesh_);
    cubeMesh_ = mesh;
    lastBuiltBlock_ = blockId;
}
