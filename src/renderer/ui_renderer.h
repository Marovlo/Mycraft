#pragma once

#include "engine/vulkan_engine.h"
#include "texture_atlas.h"
#include "gui_atlas.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

// 2D UI renderer using dynamic CPU-to-GPU buffers.
// 支持两种纹理源：方块图集（TextureAtlas）和 GUI 图集（GuiAtlas）。
// flush() 时先渲染方块图集内容，再渲染 GUI 图集内容。
class UIRenderer {
public:
    void init(VulkanEngine* engine);
    void destroy();

    // Attach the block-atlas (which also contains font_digit_0..9 glyph tiles).
    // Required before drawNumber() calls resolve glyph UVs.
    void setAtlas(const TextureAtlas* atlas) { atlas_ = atlas; }

    // Attach the GUI atlas for GUI sprite rendering.
    void setGuiAtlas(const GuiAtlas* guiAtlas) { guiAtlas_ = guiAtlas; }

    // --- Draw commands (queue for this frame, uses block atlas texture) ---
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
    void drawNumber(int value, float rightX, float y, float glyphH,
                    const glm::vec4& color = glm::vec4(1.0f));

    // Draw a text string using bitmap font glyphs (A-Z, 0-9, space).
    void drawText(const std::string& text, float centerX, float y, float glyphH,
                  const glm::vec4& color = glm::vec4(1.0f));

    // Draw a text string left-aligned at (leftX, y).
    void drawTextLeft(const std::string& text, float leftX, float y, float glyphH,
                      const glm::vec4& color = glm::vec4(1.0f));

    // --- GUI sprite draw commands (uses GUI atlas texture) ---
    // 绘制 GUI 精灵（使用 GUI 图集纹理）
    void drawGuiSprite(const std::string& spriteName, float x, float y, float w, float h,
                       const glm::vec4& tint = glm::vec4(1.0f));

    // 绘制 GUI 精灵的子区域（UV 偏移，用于从大贴图中裁剪部分区域）
    void drawGuiSpriteRegion(const std::string& spriteName,
                             float x, float y, float w, float h,
                             float srcX, float srcY, float srcW, float srcH,
                             const glm::vec4& tint = glm::vec4(1.0f));

    // 直接用 GuiSprite 的 UV 绘制（避免重复查找）
    void drawGuiSpriteUV(const GuiSprite& sprite, float x, float y, float w, float h,
                         const glm::vec4& tint = glm::vec4(1.0f));

    // Submit all queued draws. Call during render callback.
    // textureView/sampler: which texture to use for this batch (default = block atlas)
    void flush(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight);

    // Flush with a specific texture (for icon atlas etc.)
    void flushWithTexture(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight,
                          VkImageView textureView, VkSampler sampler);

private:
    VulkanEngine* engine_ = nullptr;
    const TextureAtlas* atlas_ = nullptr;
    const GuiAtlas* guiAtlas_ = nullptr;

    // 方块图集顶点/索引（主缓冲区）
    std::vector<UIVertex> vertices_;
    std::vector<uint32_t> indices_;

    // GUI 图集顶点/索引（第二缓冲区）
    std::vector<UIVertex> guiVertices_;
    std::vector<uint32_t> guiIndices_;

    // Persistent dynamic buffers (CPU writable, GPU readable)
    // 方块图集使用的 buffer
    AllocatedBuffer vertexBuffer_;
    AllocatedBuffer indexBuffer_;
    VkDeviceSize vertexBufferSize_ = 0;
    VkDeviceSize indexBufferSize_ = 0;

    // GUI 图集使用的独立 buffer（避免与方块图集 buffer 竞争导致闪烁）
    AllocatedBuffer guiVertexBuffer_;
    AllocatedBuffer guiIndexBuffer_;
    VkDeviceSize guiVertexBufferSize_ = 0;
    VkDeviceSize guiIndexBufferSize_ = 0;

    // GUI 图集的 descriptor set（独立于方块图集）
    VkDescriptorSet guiDescriptorSet_ = VK_NULL_HANDLE;
    bool guiDescriptorAllocated_ = false;

    static constexpr size_t INITIAL_VERTS = 256;
    static constexpr size_t INITIAL_INDICES = 384;

    void ensureBufferCapacity(size_t vertCount, size_t idxCount);
    void ensureGuiBufferCapacity(size_t vertCount, size_t idxCount);

    void addQuad(float x0, float y0, float x1, float y1,
                 float u0, float v0, float u1, float v1,
                 const glm::vec4& color);

    // 向 GUI 缓冲区添加四边形
    void addGuiQuad(float x0, float y0, float x1, float y1,
                    float u0, float v0, float u1, float v1,
                    const glm::vec4& color);

    // 内部：渲染一批顶点/索引（方块图集 buffer）
    void flushBatch(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight,
                    const std::vector<UIVertex>& verts, const std::vector<uint32_t>& idxs,
                    VkDescriptorSet descriptorSet);

    // 内部：渲染一批 GUI 顶点/索引（GUI 图集独立 buffer）
    void flushGuiBatch(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight,
                       const std::vector<UIVertex>& verts, const std::vector<uint32_t>& idxs,
                       VkDescriptorSet descriptorSet);
};
