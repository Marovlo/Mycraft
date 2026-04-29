#pragma once

#include "engine/vulkan_engine.h"
#include "entity/entity_manager.h"

class TextureAtlas;

// Collects every live ItemEntity into a single dynamic vertex/index buffer
// each frame, then issues ONE draw call reusing the main 3D pipeline.
//
// Rationale (see Batch 3.5 design doc, section 4 — "方案 D"):
// - Chunk meshes are large and static; they live in dedicated GPU buffers.
// - Item entities are tiny (6 faces each) and move every tick, so upload
//   cost dominates. Merging them all into one persistently-mapped CPU→GPU
//   buffer gives us O(1) draw calls regardless of entity count.
//
// The 3D pipeline's UBO (model=identity, view, proj, fog, viewPos) is reused
// as-is: vertex positions are baked into world space on the CPU.
class EntityRenderer {
public:
    void init(VulkanEngine* engine, const TextureAtlas* atlas);
    void destroy();

    // Rebuild the merged mesh from the current entity set. Call each frame
    // before render(). `partialTick` ∈ [0,1] controls render interpolation
    // between the entity's previous-tick snapshot and its current values —
    // MC uses the same factor to keep movement smooth between 20 Hz ticks.
    void buildFrame(const EntityManager& mgr, float partialTick);

    // Issue one drawIndexed for every queued face. Must be called inside the
    // render pass, after chunk draws (so alpha sorting is consistent if any
    // translucent items appear later).
    void render(VkCommandBuffer cmd);

private:
    VulkanEngine* engine_ = nullptr;
    const TextureAtlas* atlas_ = nullptr;

    // CPU staging for per-frame vertex/index streams.
    std::vector<Vertex>   vertices_;
    std::vector<uint32_t> indices_;

    // Persistent dynamic GPU buffers, grown on demand (× 2 amortized).
    AllocatedBuffer vertexBuffer_;
    AllocatedBuffer indexBuffer_;
    VkDeviceSize    vertexBufferSize_ = 0;
    VkDeviceSize    indexBufferSize_  = 0;
    uint32_t        indexCountThisFrame_ = 0;

    void ensureCapacity();
    void appendItemMesh(const class ItemEntity& item, float partialTick);
    void appendArrowMesh(const class ArrowEntity& arrow, float partialTick);
    void appendXPOrbMesh(const class XPOrbEntity& orb, float partialTick);
};
