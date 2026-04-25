#pragma once

#include "engine/vulkan_engine.h"
#include "ui_renderer.h"
#include "texture_atlas.h"
#include "core/block.h"
#include <glm/glm.hpp>

// Renders a single block as a 3D-looking icon by CPU-projecting cube faces
// into screen-space triangles, then enqueueing them into the shared UIRenderer.
//
// This avoids:
//   * Per-icon staging buffer uploads (no GPU stalls per frame)
//   * Per-icon descriptor / pipeline switches
//   * Multiple UBOs (the icon never touches the 3D MVP UBO)
//
// All icons share the block-atlas texture, exactly like other UI textured rects.
class BlockModelRenderer {
public:
    void init(VulkanEngine& engine, const TextureAtlas& atlas);
    void destroy(VulkanEngine& engine);

    // Queue a block icon into the UI vertex stream.
    // Must be called BEFORE UIRenderer::flush() so the icon participates in the
    // single UI draw call. screenX/screenY = top-left of icon rect, size = pixel size.
    void enqueueBlockIcon(UIRenderer& ui, BlockId blockId,
                          const TextureAtlas& atlas,
                          float screenX, float screenY, float size) const;

private:
    // Isometric view-projection matrix (fixed angle, orthographic, MC-style)
    glm::mat4 getIconViewProj() const;
};
