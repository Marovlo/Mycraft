#pragma once

#include "engine/vulkan_engine.h"
#include "texture_atlas.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

// 2D UI renderer using dynamic CPU-to-GPU buffers.
// No per-frame staging buffer or command submit — just memcpy + draw.
class UIRenderer {
public:
    void init(VulkanEngine* engine);
    void destroy();

    // Attach the block-atlas (which also contains font_digit_0..9 glyph tiles).
    // Required before drawNumber() calls resolve glyph UVs.
    void setAtlas(const TextureAtlas* atlas) { atlas_ = atlas; }

    // --- Draw commands (queue for this frame) ---
    void drawRect(float x, float y, float w, float h, const glm::vec4& color);
    void drawTexturedRect(float x, float y, float w, float h,
                          float u0, float v0, float u1, float v1,
                          const glm::vec4& tint = glm::vec4(1.0f));
    void drawCrosshair(float screenW, float screenH, float size = 20.0f, float thickness = 2.0f);

    // Submit a textured triangle (raw screen-space coords + UV + per-vertex tint).
    // Used by 3D block icon renderer which projects cube faces on CPU.
    void drawTexturedTri(const UIVertex& a, const UIVertex& b, const UIVertex& c);

    // Draw a non-negative integer using the atlas font glyphs. The number is
    // right-aligned at (rightX, y): last digit's right edge sits at rightX.
    // glyphH is the pixel height of a single glyph; glyph width scales to half
    // height (matches the 5x7 bitmap). Draws a 1-pixel-offset black shadow.
    void drawNumber(int value, float rightX, float y, float glyphH,
                    const glm::vec4& color = glm::vec4(1.0f));

    // Draw a text string using bitmap font glyphs (A-Z, 0-9, space).
    // Centered horizontally at centerX. Converts to uppercase internally.
    void drawText(const std::string& text, float centerX, float y, float glyphH,
                  const glm::vec4& color = glm::vec4(1.0f));

    // Draw a text string left-aligned at (leftX, y).
    void drawTextLeft(const std::string& text, float leftX, float y, float glyphH,
                      const glm::vec4& color = glm::vec4(1.0f));

    // Submit all queued draws. Call during render callback.
    // textureView/sampler: which texture to use for this batch (default = block atlas)
    void flush(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight);

    // Flush with a specific texture (for icon atlas etc.)
    void flushWithTexture(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight,
                          VkImageView textureView, VkSampler sampler);

private:
    VulkanEngine* engine_ = nullptr;
    const TextureAtlas* atlas_ = nullptr;
    std::vector<UIVertex> vertices_;
    std::vector<uint32_t> indices_;

    // Persistent dynamic buffers (CPU writable, GPU readable)
    AllocatedBuffer vertexBuffer_;
    AllocatedBuffer indexBuffer_;
    VkDeviceSize vertexBufferSize_ = 0;
    VkDeviceSize indexBufferSize_ = 0;

    static constexpr size_t INITIAL_VERTS = 256;
    static constexpr size_t INITIAL_INDICES = 384;

    void ensureBufferCapacity();

    void addQuad(float x0, float y0, float x1, float y1,
                 float u0, float v0, float u1, float v1,
                 const glm::vec4& color);
};
