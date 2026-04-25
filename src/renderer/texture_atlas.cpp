#include "texture_atlas.h"

#include <stb_image.h>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>

namespace fs = std::filesystem;

// Collect all PNG files from a directory (sorted for deterministic order).
static void collectPngs(const std::string& dir, std::vector<fs::path>& out) {
    if (!fs::exists(dir)) return;
    for (auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            out.push_back(entry.path());
        }
    }
}

bool TextureAtlas::build(VulkanEngine& engine, const std::string& textureDir, uint32_t tileSize) {
    tileSize_ = tileSize;
    nameToIndex_.clear();
    indexToName_.clear();

    // Collect PNGs from multiple directories:
    //   blocks/  — block face textures (grass_top, stone, etc.)
    //   items/   — tool/material 2D icons
    //   hud/     — HUD icons (hearts, drumsticks, apple)
    //   font/    — bitmap font digits
    // All share the same atlas; callers look up by filename stem.
    std::vector<fs::path> allPngs;
    collectPngs(textureDir, allPngs);  // blocks/

    // Derive base textures path: textureDir is ".../textures/blocks",
    // so parent is ".../textures"
    fs::path texBase = fs::path(textureDir).parent_path();
    collectPngs((texBase / "items").string(), allPngs);
    collectPngs((texBase / "hud").string(), allPngs);
    collectPngs((texBase / "font").string(), allPngs);

    // Sort all by filename stem for deterministic tile ordering
    std::sort(allPngs.begin(), allPngs.end(), [](const fs::path& a, const fs::path& b) {
        return a.stem().string() < b.stem().string();
    });

    if (allPngs.empty()) {
        std::cerr << "TextureAtlas: no PNG files found\n";
        return false;
    }

    totalTiles_ = static_cast<uint32_t>(allPngs.size());

    // Compute atlas dimensions
    tilesPerRow_ = 1;
    while (tilesPerRow_ * tilesPerRow_ < totalTiles_) {
        tilesPerRow_ *= 2;
    }
    invTilesPerRow_ = 1.0f / static_cast<float>(tilesPerRow_);

    uint32_t atlasPixelSize = tilesPerRow_ * tileSize_;
    std::vector<uint8_t> atlasPixels(atlasPixelSize * atlasPixelSize * 4, 0);

    // Load each PNG and blit into atlas
    for (uint32_t i = 0; i < totalTiles_; i++) {
        const auto& path = allPngs[i];
        std::string name = path.stem().string();

        nameToIndex_[name] = static_cast<uint16_t>(i);
        indexToName_.push_back(name);

        int w, h, ch;
        uint8_t* data = stbi_load(path.string().c_str(), &w, &h, &ch, 4);
        uint32_t tx = (i % tilesPerRow_) * tileSize_;
        uint32_t ty = (i / tilesPerRow_) * tileSize_;

        if (!data) {
            std::cerr << "TextureAtlas: failed to load " << path << "\n";
            // Fill with magenta
            for (uint32_t py = 0; py < tileSize_; py++) {
                for (uint32_t px = 0; px < tileSize_; px++) {
                    uint32_t idx = ((ty + py) * atlasPixelSize + (tx + px)) * 4;
                    atlasPixels[idx+0] = 255; atlasPixels[idx+1] = 0;
                    atlasPixels[idx+2] = 255; atlasPixels[idx+3] = 255;
                }
            }
            continue;
        }

        uint32_t copyW = std::min(static_cast<uint32_t>(w), tileSize_);
        uint32_t copyH = std::min(static_cast<uint32_t>(h), tileSize_);

        for (uint32_t py = 0; py < copyH; py++) {
            uint32_t srcOffset = py * w * 4;
            uint32_t dstOffset = ((ty + py) * atlasPixelSize + tx) * 4;
            std::memcpy(&atlasPixels[dstOffset], &data[srcOffset], copyW * 4);
        }

        stbi_image_free(data);
    }

    // Upload to GPU
    image_ = engine.uploadTexture(atlasPixels.data(),
        static_cast<int>(atlasPixelSize), static_cast<int>(atlasPixelSize), 4);

    cpuPixels_ = std::move(atlasPixels);
    atlasPixelSize_ = atlasPixelSize;

    std::cout << "TextureAtlas: loaded " << totalTiles_ << " tiles ("
              << tilesPerRow_ << "x" << tilesPerRow_ << " grid, "
              << atlasPixelSize << "x" << atlasPixelSize << " px)\n";

    return true;
}

void TextureAtlas::destroy(VulkanEngine& engine) {
    engine.destroyTexture(image_);
}

uint16_t TextureAtlas::getTileIndex(const std::string& name) const {
    auto it = nameToIndex_.find(name);
    if (it != nameToIndex_.end()) return it->second;
    return 0;
}

glm::vec4 TextureAtlas::getTileUV(uint16_t tileIndex) const {
    float col = static_cast<float>(tileIndex % tilesPerRow_);
    float row = static_cast<float>(tileIndex / tilesPerRow_);
    float uMin = col * invTilesPerRow_;
    float vMin = row * invTilesPerRow_;
    float uMax = (col + 1.0f) * invTilesPerRow_;
    float vMax = (row + 1.0f) * invTilesPerRow_;
    return {uMin, vMin, uMax, vMax};
}
