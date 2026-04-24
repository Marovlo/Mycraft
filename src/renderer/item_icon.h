#pragma once

#include "engine/vulkan_engine.h"
#include "texture_atlas.h"
#include "core/block.h"
#include <vector>
#include <glm/glm.hpp>

// Pre-renders isometric 3D block thumbnails into a CPU-side icon atlas.
// Each block gets a small icon (e.g., 32x32) showing 3 faces in isometric view.
// The atlas is uploaded as a separate texture for UI rendering.
class ItemIconAtlas {
public:
    static constexpr int ICON_SIZE = 32;

    // Generate icons for all registered blocks.
    // blockAtlas: the block texture atlas (for reading face textures)
    // blockAtlasPixels: CPU-side pixel data of the block atlas (needed to sample textures)
    bool build(VulkanEngine& engine, const TextureAtlas& blockAtlas,
               const std::vector<uint8_t>& blockAtlasPixels, uint32_t blockAtlasWidth);

    void destroy(VulkanEngine& engine);

    // Get UV rect for an item icon in the icon atlas
    // For block items: uses the block's ID to find the icon
    // For non-block items: returns a default or uses textureTileIndex
    glm::vec4 getIconUV(uint16_t itemId) const;

    const AllocatedImage& getImage() const { return image_; }

private:
    AllocatedImage image_;
    uint32_t iconsPerRow_ = 0;
    uint32_t totalIcons_ = 0;
    float invIconsPerRow_ = 0.0f;

    // Render a single isometric block icon into the pixel buffer
    void renderBlockIcon(uint8_t* iconPixels,
                         const TextureAtlas& blockAtlas,
                         const std::vector<uint8_t>& atlasPixels, uint32_t atlasWidth,
                         uint16_t topTile, uint16_t frontTile, uint16_t sideTile);

    // Sample a pixel from the block atlas
    glm::vec4 sampleAtlas(const std::vector<uint8_t>& pixels, uint32_t atlasWidth,
                          uint16_t tileIndex, const TextureAtlas& atlas,
                          float u, float v) const;
};
