#pragma once

#include "engine/vulkan_engine.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

// Manages a 2D texture atlas built from individual tile PNG files,
// plus procedurally-generated glyph tiles for a minimal bitmap font.
//
// Glyph tiles are added as `font_digit_0` ... `font_digit_9` so callers can
// request them via getTileIndex("font_digit_3") and reuse the existing block
// atlas descriptor/pipeline (no second texture slot needed).

class TextureAtlas {
public:
    // Build atlas from all PNG files in the given directory.
    // Each PNG must be tileSize x tileSize RGBA.
    // Returns false on failure.
    bool build(VulkanEngine& engine, const std::string& textureDir, uint32_t tileSize = 16);

    // Destroy GPU resources
    void destroy(VulkanEngine& engine);

    // Lookup tile index by name (filename without extension)
    // Returns 0 (first tile) if not found.
    uint16_t getTileIndex(const std::string& name) const;

    // Get normalized UV rect for a tile: {uMin, vMin, uMax, vMax}
    glm::vec4 getTileUV(uint16_t tileIndex) const;

    // Getters
    const AllocatedImage& getImage() const { return image_; }
    uint32_t getTileSize() const { return tileSize_; }
    uint32_t getTilesPerRow() const { return tilesPerRow_; }
    uint32_t getTotalTiles() const { return totalTiles_; }

private:
    AllocatedImage image_;
    uint32_t tileSize_ = 16;
    uint32_t tilesPerRow_ = 0;
    uint32_t totalTiles_ = 0;
    float invTilesPerRow_ = 0.0f;  // cached 1.0/tilesPerRow for UV calc

    std::unordered_map<std::string, uint16_t> nameToIndex_;
    std::vector<std::string> indexToName_;

    // CPU-side pixel data (kept for icon pre-rendering)
    std::vector<uint8_t> cpuPixels_;
    uint32_t atlasPixelSize_ = 0;

public:
    const std::vector<uint8_t>& getCpuPixels() const { return cpuPixels_; }
    uint32_t getAtlasPixelSize() const { return atlasPixelSize_; }

    // 动态更新图集中某个 tile 的像素数据（用于纹理动画）
    void updateTile(VulkanEngine& engine, uint16_t tileIndex, const uint8_t* rgbaPixels);
};
