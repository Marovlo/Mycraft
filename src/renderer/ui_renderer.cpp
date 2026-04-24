#include "ui_renderer.h"

void UIRenderer::init(VulkanEngine* engine) {
    engine_ = engine;
}

void UIRenderer::destroy() {
    if (engine_ && lastMesh_.indexCount > 0) {
        engine_->destroyMesh(lastMesh_);
        lastMesh_ = {};
    }
    engine_ = nullptr;
}

void UIRenderer::addQuad(float x0, float y0, float x1, float y1,
                         float u0, float v0, float u1, float v1,
                         const glm::vec4& color) {
    uint32_t base = static_cast<uint32_t>(vertices_.size());

    vertices_.push_back({{x0, y0}, {u0, v0}, color});
    vertices_.push_back({{x1, y0}, {u1, v0}, color});
    vertices_.push_back({{x1, y1}, {u1, v1}, color});
    vertices_.push_back({{x0, y1}, {u0, v1}, color});

    indices_.push_back(base + 0);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);
    indices_.push_back(base + 0);
    indices_.push_back(base + 2);
    indices_.push_back(base + 3);
}

void UIRenderer::drawRect(float x, float y, float w, float h, const glm::vec4& color) {
    addQuad(x, y, x + w, y + h, 0.0f, 0.0f, 0.001f, 0.001f, color);
}

void UIRenderer::drawTexturedRect(float x, float y, float w, float h,
                                  float u0, float v0, float u1, float v1,
                                  const glm::vec4& tint) {
    addQuad(x, y, x + w, y + h, u0, v0, u1, v1, tint);
}

void UIRenderer::drawCrosshair(float screenW, float screenH, float size, float thickness) {
    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;
    float hs = size * 0.5f;
    float ht = thickness * 0.5f;

    glm::vec4 color(1.0f, 1.0f, 1.0f, 0.85f);

    // Horizontal bar
    drawRect(cx - hs, cy - ht, size, thickness, color);
    // Vertical bar
    drawRect(cx - ht, cy - hs, thickness, size, color);
}

void UIRenderer::flush(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight) {
    if (vertices_.empty() || !engine_) {
        vertices_.clear();
        indices_.clear();
        return;
    }

    // Upload UI vertices to GPU (small per-frame upload, acceptable for UI)
    Mesh uiMesh = engine_->uploadUIMesh(vertices_, indices_);

    // Switch to UI pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_->getUIPipeline());

    // Set viewport and scissor (same as 3D)
    VkViewport viewport{};
    viewport.width = static_cast<float>(screenWidth);
    viewport.height = static_cast<float>(screenHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = {screenWidth, screenHeight};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Push screen size
    UIPushConstants pc{};
    pc.screenSize = {static_cast<float>(screenWidth), static_cast<float>(screenHeight)};
    vkCmdPushConstants(cmd, engine_->getUIPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UIPushConstants), &pc);

    // Bind descriptor set (same as 3D — has the texture sampler)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine_->getUIPipelineLayout(), 0, 1,
        &engine_->getCurrentFrame().descriptorSet, 0, nullptr);

    // Draw
    VkBuffer vb[] = {uiMesh.vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, uiMesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, uiMesh.indexCount, 1, 0, 0, 0);

    // Cleanup temp mesh (will be freed next frame... actually we need to defer this)
    // For safety, destroy after queue is idle. But that's expensive.
    // Better: keep the mesh alive until next frame's flush.
    // Simple approach: store last frame's mesh and destroy it at start of next flush.
    if (lastMesh_.indexCount > 0) {
        engine_->destroyMesh(lastMesh_);
    }
    lastMesh_ = uiMesh;

    // Re-bind 3D pipeline for any subsequent 3D draws (shouldn't happen, but safe)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_->getPipeline());

    vertices_.clear();
    indices_.clear();
}
