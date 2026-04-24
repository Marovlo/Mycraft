#include "item_icon.h"
#include "core/item.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <iostream>

glm::vec4 ItemIconAtlas::sampleAtlas(const std::vector<uint8_t>& pixels, uint32_t atlasWidth,
                                      uint16_t tileIndex, const TextureAtlas& atlas,
                                      float u, float v) const {
    // Get tile position in atlas
    uint32_t tilesPerRow = atlas.getTilesPerRow();
    uint32_t tileSize = atlas.getTileSize();
    uint32_t tileX = (tileIndex % tilesPerRow) * tileSize;
    uint32_t tileY = (tileIndex / tilesPerRow) * tileSize;

    // Clamp and convert to pixel coords within tile
    int px = std::clamp(static_cast<int>(u * tileSize), 0, static_cast<int>(tileSize) - 1);
    int py = std::clamp(static_cast<int>(v * tileSize), 0, static_cast<int>(tileSize) - 1);

    uint32_t idx = ((tileY + py) * atlasWidth + (tileX + px)) * 4;
    if (idx + 3 >= pixels.size()) return {1, 0, 1, 1}; // magenta error

    return {
        pixels[idx + 0] / 255.0f,
        pixels[idx + 1] / 255.0f,
        pixels[idx + 2] / 255.0f,
        pixels[idx + 3] / 255.0f,
    };
}

void ItemIconAtlas::renderBlockIcon(uint8_t* iconPixels,
                                     const TextureAtlas& blockAtlas,
                                     const std::vector<uint8_t>& atlasPixels, uint32_t atlasWidth,
                                     uint16_t topTile, uint16_t frontTile, uint16_t sideTile) {
    // Render isometric block: top face + front face + side face
    // Icon is ICON_SIZE x ICON_SIZE pixels
    // Isometric projection: top face is a diamond, front and side are parallelograms
    const int S = ICON_SIZE;
    const float halfS = S * 0.5f;

    // Clear to transparent
    std::memset(iconPixels, 0, S * S * 4);

    // For each pixel, determine which face it belongs to and sample accordingly
    for (int py = 0; py < S; py++) {
        for (int px = 0; px < S; px++) {
            // Normalize to [-1, 1] centered
            float nx = (px - halfS + 0.5f) / halfS;  // -1 to 1
            float ny = (py - halfS + 0.5f) / halfS;  // -1 to 1

            glm::vec4 color(0, 0, 0, 0);

            // Isometric cube geometry:
            // Top face: diamond shape in upper half
            // Front face: left parallelogram in lower half
            // Side face: right parallelogram in lower half

            float topY = -0.15f;    // top face center Y offset
            float cubeH = 0.55f;    // half-height of visible side faces
            float cubeW = 0.8f;     // half-width at widest point

            // Top face (diamond): bounded by 4 lines from center
            float topLeft  = -nx * 0.5f + topY;
            float topRight =  nx * 0.5f + topY;
            bool inTopFace = (ny < topRight + cubeH * 0.5f) && (ny < -topRight + cubeH * 0.5f + topY * 2) &&
                             (ny > topLeft - cubeH * 0.5f + topY) && (ny > -topLeft - cubeH * 0.5f + topY);

            // Simpler approach: use a standard isometric mapping
            // Top face: parallelogram from (-0.8, -0.1) to (0, -0.5) to (0.8, -0.1) to (0, 0.3)
            // Front face: parallelogram from (-0.8, -0.1) to (0, 0.3) to (0, 0.95) to (-0.8, 0.55)
            // Side face: parallelogram from (0, 0.3) to (0.8, -0.1) to (0.8, 0.55) to (0, 0.95)

            // Use barycentric/parametric check for each face
            // Top face: defined by 4 corners
            glm::vec2 T0(-0.8f, -0.05f), T1(0.0f, -0.45f), T2(0.8f, -0.05f), T3(0.0f, 0.35f);
            // Front face (left)
            glm::vec2 F0(-0.8f, -0.05f), F1(0.0f, 0.35f), F2(0.0f, 0.95f), F3(-0.8f, 0.55f);
            // Side face (right)
            glm::vec2 S0(0.0f, 0.35f), S1(0.8f, -0.05f), S2(0.8f, 0.55f), S3(0.0f, 0.95f);

            glm::vec2 p(nx, ny);

            // Check if point is in a parallelogram using parametric method
            auto inQuad = [](glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d,
                             float& u, float& v) -> bool {
                // Decompose into two triangles: abc and acd
                auto cross2d = [](glm::vec2 x, glm::vec2 y) { return x.x * y.y - x.y * y.x; };

                // For parallelogram: p = a + u*(b-a) + v*(d-a)
                glm::vec2 ab = b - a;
                glm::vec2 ad = d - a;
                glm::vec2 ap = p - a;

                float denom = cross2d(ab, ad);
                if (std::abs(denom) < 1e-6f) return false;

                u = cross2d(ap, ad) / denom;
                v = cross2d(ab, ap) / denom;

                return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f;
            };

            float u, v;

            if (inQuad(p, T0, T2, T1, T3, u, v)) {
                // Top face
                color = sampleAtlas(atlasPixels, atlasWidth, topTile, blockAtlas, u, v);
                // Brighten top face
                color = glm::vec4(color.r * 1.0f, color.g * 1.0f, color.b * 1.0f, color.a);
            } else if (inQuad(p, F0, F1, F2, F3, u, v)) {
                // Front face (darker)
                color = sampleAtlas(atlasPixels, atlasWidth, frontTile, blockAtlas, u, v);
                color = glm::vec4(color.r * 0.7f, color.g * 0.7f, color.b * 0.7f, color.a);
            } else if (inQuad(p, S0, S1, S2, S3, u, v)) {
                // Side face (medium)
                color = sampleAtlas(atlasPixels, atlasWidth, sideTile, blockAtlas, u, v);
                color = glm::vec4(color.r * 0.85f, color.g * 0.85f, color.b * 0.85f, color.a);
            }

            if (color.a > 0.0f) {
                int idx = (py * S + px) * 4;
                iconPixels[idx + 0] = static_cast<uint8_t>(std::clamp(color.r * 255.0f, 0.0f, 255.0f));
                iconPixels[idx + 1] = static_cast<uint8_t>(std::clamp(color.g * 255.0f, 0.0f, 255.0f));
                iconPixels[idx + 2] = static_cast<uint8_t>(std::clamp(color.b * 255.0f, 0.0f, 255.0f));
                iconPixels[idx + 3] = static_cast<uint8_t>(std::clamp(color.a * 255.0f, 0.0f, 255.0f));
            }
        }
    }
}

bool ItemIconAtlas::build(VulkanEngine& engine, const TextureAtlas& blockAtlas,
                          const std::vector<uint8_t>& blockAtlasPixels, uint32_t blockAtlasWidth) {
    auto& itemReg = ItemRegistry::instance();
    auto& blockReg = BlockRegistry::instance();

    totalIcons_ = itemReg.itemCount();
    iconsPerRow_ = 1;
    while (iconsPerRow_ * iconsPerRow_ < totalIcons_) iconsPerRow_ *= 2;
    iconsPerRow_ = std::max(iconsPerRow_, 4u);
    invIconsPerRow_ = 1.0f / static_cast<float>(iconsPerRow_);

    uint32_t atlasSize = iconsPerRow_ * ICON_SIZE;
    std::vector<uint8_t> pixels(atlasSize * atlasSize * 4, 0);

    for (uint16_t itemId = 1; itemId < totalIcons_; itemId++) {
        const auto& props = itemReg.get(itemId);

        uint32_t iconX = (itemId % iconsPerRow_) * ICON_SIZE;
        uint32_t iconY = (itemId / iconsPerRow_) * ICON_SIZE;

        std::vector<uint8_t> iconPixels(ICON_SIZE * ICON_SIZE * 4, 0);

        if (props.type == ItemType::Block && props.blockId > 0 && props.blockId < blockReg.blockCount()) {
            // 3D isometric block icon
            const auto& blockProps = blockReg.get(props.blockId);
            renderBlockIcon(iconPixels.data(), blockAtlas, blockAtlasPixels, blockAtlasWidth,
                           blockProps.textures.top, blockProps.textures.south, blockProps.textures.east);
        } else {
            // Non-block item: render a simple colored square for now
            // TODO: load 2D item textures from assets
            for (int y = 2; y < ICON_SIZE - 2; y++) {
                for (int x = 2; x < ICON_SIZE - 2; x++) {
                    int idx = (y * ICON_SIZE + x) * 4;
                    iconPixels[idx + 0] = 180;
                    iconPixels[idx + 1] = 140;
                    iconPixels[idx + 2] = 100;
                    iconPixels[idx + 3] = 255;
                }
            }
        }

        // Blit icon into atlas
        for (int y = 0; y < ICON_SIZE; y++) {
            uint32_t srcOff = y * ICON_SIZE * 4;
            uint32_t dstOff = ((iconY + y) * atlasSize + iconX) * 4;
            std::memcpy(&pixels[dstOff], &iconPixels[srcOff], ICON_SIZE * 4);
        }
    }

    image_ = engine.uploadTexture(pixels.data(), atlasSize, atlasSize, 4);

    std::cout << "ItemIconAtlas: generated " << (totalIcons_ - 1) << " icons ("
              << iconsPerRow_ << "x" << iconsPerRow_ << " grid, "
              << atlasSize << "x" << atlasSize << " px)\n";

    return true;
}

void ItemIconAtlas::destroy(VulkanEngine& engine) {
    engine.destroyTexture(image_);
}

glm::vec4 ItemIconAtlas::getIconUV(uint16_t itemId) const {
    if (itemId == 0 || itemId >= totalIcons_) return {0, 0, 0, 0};
    float col = static_cast<float>(itemId % iconsPerRow_);
    float row = static_cast<float>(itemId / iconsPerRow_);
    return {col * invIconsPerRow_, row * invIconsPerRow_,
            (col + 1) * invIconsPerRow_, (row + 1) * invIconsPerRow_};
}
