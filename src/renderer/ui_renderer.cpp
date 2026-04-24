#include "ui_renderer.h"

void UIRenderer::init(VulkanEngine* engine) {
    engine_ = engine;

    // Pre-allocate dynamic buffers
    vertexBufferSize_ = sizeof(UIVertex) * INITIAL_VERTS;
    indexBufferSize_ = sizeof(uint32_t) * INITIAL_INDICES;
    vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indexBuffer_ = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void UIRenderer::destroy() {
    if (engine_) {
        if (vertexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
            vertexBuffer_ = {};
        }
        if (indexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
            indexBuffer_ = {};
        }
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

    glm::vec4 color(1.0f, 1.0f, 1.0f, 0.9f);

    // Horizontal bar
    drawRect(cx - hs, cy - ht, size, thickness, color);
    // Vertical bar
    drawRect(cx - ht, cy - hs, thickness, size, color);
}

void UIRenderer::ensureBufferCapacity() {
    VkDeviceSize neededVB = sizeof(UIVertex) * vertices_.size();
    VkDeviceSize neededIB = sizeof(uint32_t) * indices_.size();

    // Grow vertex buffer if needed
    if (neededVB > vertexBufferSize_) {
        if (vertexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        }
        vertexBufferSize_ = neededVB * 2;  // double to amortize
        vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    // Grow index buffer if needed
    if (neededIB > indexBufferSize_) {
        if (indexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        }
        indexBufferSize_ = neededIB * 2;
        indexBuffer_ = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }
}

void UIRenderer::flush(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight) {
    if (vertices_.empty() || !engine_) {
        vertices_.clear();
        indices_.clear();
        return;
    }

    ensureBufferCapacity();

    // Direct memcpy — no staging, no command submit, no GPU stall
    void* vData = engine_->mapBuffer(vertexBuffer_);
    memcpy(vData, vertices_.data(), sizeof(UIVertex) * vertices_.size());
    engine_->unmapBuffer(vertexBuffer_);

    void* iData = engine_->mapBuffer(indexBuffer_);
    memcpy(iData, indices_.data(), sizeof(uint32_t) * indices_.size());
    engine_->unmapBuffer(indexBuffer_);

    uint32_t indexCount = static_cast<uint32_t>(indices_.size());

    // Switch to UI pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_->getUIPipeline());

    VkViewport viewport{};
    viewport.width = static_cast<float>(screenWidth);
    viewport.height = static_cast<float>(screenHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = {screenWidth, screenHeight};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    UIPushConstants pc{};
    pc.screenSize = {static_cast<float>(screenWidth), static_cast<float>(screenHeight)};
    vkCmdPushConstants(cmd, engine_->getUIPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UIPushConstants), &pc);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine_->getUIPipelineLayout(), 0, 1,
        &engine_->getCurrentFrame().descriptorSet, 0, nullptr);

    VkBuffer vb[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

    // Restore 3D pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_->getPipeline());

    vertices_.clear();
    indices_.clear();
}
