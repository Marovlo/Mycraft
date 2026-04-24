#pragma once

#include "engine/vulkan_engine.h"
#include <vector>
#include <glm/glm.hpp>

// 2D UI renderer using the engine's UI pipeline.
// Usage each frame:
//   1. Call drawRect() / drawCrosshair() etc. to queue draw requests
//   2. Call flush() during render callback to upload and draw everything
//   3. Automatically clears for next frame
class UIRenderer {
public:
    void init(VulkanEngine* engine);
    void destroy();

    // --- Draw commands (queue for this frame) ---

    // Draw a colored rectangle (no texture)
    void drawRect(float x, float y, float w, float h, const glm::vec4& color);

    // Draw a textured rectangle (uses bound texture atlas)
    void drawTexturedRect(float x, float y, float w, float h,
                          float u0, float v0, float u1, float v1,
                          const glm::vec4& tint = glm::vec4(1.0f));

    // Draw crosshair at screen center
    void drawCrosshair(float screenW, float screenH, float size = 10.0f, float thickness = 2.0f);

    // --- Submit ---
    // Call during render callback (inside render pass). Uploads vertex data, binds UI pipeline, draws.
    void flush(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight);

private:
    VulkanEngine* engine_ = nullptr;
    std::vector<UIVertex> vertices_;
    std::vector<uint32_t> indices_;
    Mesh lastMesh_;  // previous frame's mesh, destroyed at next flush

    void addQuad(float x0, float y0, float x1, float y1,
                 float u0, float v0, float u1, float v1,
                 const glm::vec4& color);
};
