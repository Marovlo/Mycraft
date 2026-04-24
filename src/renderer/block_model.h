#pragma once

#include "engine/vulkan_engine.h"
#include "texture_atlas.h"
#include "core/block.h"
#include <glm/glm.hpp>

// Renders a single block as a 3D model in a small viewport.
// Used for inventory/hotbar item icons (MC-style real-time 3D rendering).
class BlockModelRenderer {
public:
    void init(VulkanEngine& engine, const TextureAtlas& atlas);
    void destroy(VulkanEngine& engine);

    // Render a block icon at the given screen rect.
    // Must be called during the render pass (between vkCmdBeginRenderPass and vkCmdEndRenderPass).
    // Temporarily changes viewport, UBO, and draws the block mesh, then restores state.
    void renderBlockIcon(VkCommandBuffer cmd, VulkanEngine& engine,
                         BlockId blockId, const TextureAtlas& atlas,
                         float screenX, float screenY, float size,
                         uint32_t fullScreenW, uint32_t fullScreenH);

private:
    // Pre-built unit cube mesh (6 faces with correct UVs)
    // UVs are set per-draw by rebuilding the mesh for each block type.
    // For performance, we cache the last built block and reuse.
    Mesh cubeMesh_;
    BlockId lastBuiltBlock_ = 0xFFFF;

    void buildCubeMesh(VulkanEngine& engine, const TextureAtlas& atlas, BlockId blockId);

    // Isometric view-projection matrix (fixed angle, orthographic)
    glm::mat4 getIconViewProj() const;
};
