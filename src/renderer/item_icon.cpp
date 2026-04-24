#include "item_icon.h"
#include "core/item.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <iostream>

glm::vec4 ItemIconAtlas::sampleAtlas(const std::vector<uint8_t>& pixels, uint32_t atlasWidth,
                                      uint16_t tileIndex, const TextureAtlas& atlas,
                                      float u, float v) const {
    uint32_t tilesPerRow = atlas.getTilesPerRow();
    uint32_t tileSize = atlas.getTileSize();
    uint32_t tileX = (tileIndex % tilesPerRow) * tileSize;
    uint32_t tileY = (tileIndex / tilesPerRow) * tileSize;

    int px = std::clamp(static_cast<int>(u * tileSize), 0, static_cast<int>(tileSize) - 1);
    int py = std::clamp(static_cast<int>(v * tileSize), 0, static_cast<int>(tileSize) - 1);

    uint32_t idx = ((tileY + py) * atlasWidth + (tileX + px)) * 4;
    if (idx + 3 >= pixels.size()) return {1, 0, 1, 1};

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
    const int S = ICON_SIZE;
    std::memset(iconPixels, 0, S * S * 4);

    // Simple isometric block: divide icon into 3 regions
    // Top diamond in upper portion, left face lower-left, right face lower-right
    // All coordinates in [0, S) pixel space

    float cx = S * 0.5f;       // center x
    float topY = S * 0.15f;    // top of diamond
    float midY = S * 0.45f;    // middle (widest point of diamond, top of side faces)
    float botY = S * 0.95f;    // bottom of side faces
    float halfW = S * 0.45f;   // half-width at widest point

    for (int py = 0; py < S; py++) {
        for (int px = 0; px < S; px++) {
            float x = px + 0.5f;
            float y = py + 0.5f;

            glm::vec4 color(0, 0, 0, 0);
            float u = 0, v = 0;

            // Top face (diamond): from (cx, topY) widening to midY
            if (y >= topY && y <= midY) {
                float t = (y - topY) / (midY - topY);  // 0 at top, 1 at mid
                float leftEdge = cx - halfW * t;
                float rightEdge = cx + halfW * t;

                if (x >= leftEdge && x <= rightEdge) {
                    // Check which half (above or below center line of diamond)
                    // Map to UV: u = horizontal position, v = vertical
                    u = (x - leftEdge) / (rightEdge - leftEdge);
                    v = t;
                    color = sampleAtlas(atlasPixels, atlasWidth, topTile, blockAtlas, u, v);
                    // Top face brightness = 1.0
                }
            }

            // Below midY: check if in the diamond's lower half (still top face)
            if (color.a == 0 && y > midY && y <= midY + (midY - topY)) {
                float t = (y - midY) / (midY - topY);  // 0 at mid, 1 at bottom of diamond
                float leftEdge = cx - halfW * (1.0f - t);
                float rightEdge = cx + halfW * (1.0f - t);

                if (x >= leftEdge && x <= rightEdge) {
                    u = (x - leftEdge) / (rightEdge - leftEdge);
                    v = 0.5f + t * 0.5f;
                    color = sampleAtlas(atlasPixels, atlasWidth, topTile, blockAtlas, u, v);
                }
            }

            // Left face: below diamond left half, from midY to botY
            float diamondBotY = midY + (midY - topY);
            if (color.a == 0 && y > midY && y <= botY && x < cx) {
                // Left edge slopes inward from (cx-halfW, midY) to (cx-halfW, botY)
                // Right edge is center line
                float faceH = botY - midY;
                float t = (y - midY) / faceH;  // 0 at top, 1 at bottom
                float leftEdge = cx - halfW;
                float rightEdge = cx;

                // Top of left face narrows from the diamond
                if (y <= diamondBotY) {
                    float dt = (y - midY) / (diamondBotY - midY);
                    rightEdge = cx - halfW * (1.0f - dt) * 0.0f + cx * 1.0f;
                    // Actually keep it simple: just check x < cx
                }

                if (x >= leftEdge && x < rightEdge) {
                    u = (x - leftEdge) / (rightEdge - leftEdge);
                    v = t;
                    color = sampleAtlas(atlasPixels, atlasWidth, frontTile, blockAtlas, u, v);
                    color *= 0.7f;  // darken front face
                    color.a = 1.0f;
                }
            }

            // Right face: below diamond right half, from midY to botY
            if (color.a == 0 && y > midY && y <= botY && x >= cx) {
                float leftEdge = cx;
                float rightEdge = cx + halfW;
                float faceH = botY - midY;
                float t = (y - midY) / faceH;

                if (x >= leftEdge && x <= rightEdge) {
                    u = (x - leftEdge) / (rightEdge - leftEdge);
                    v = t;
                    color = sampleAtlas(atlasPixels, atlasWidth, sideTile, blockAtlas, u, v);
                    color *= 0.85f;  // medium brightness
                    color.a = 1.0f;
                }
            }

            if (color.a > 0.01f) {
                int idx = (py * S + px) * 4;
                iconPixels[idx + 0] = static_cast<uint8_t>(std::clamp(color.r * 255.0f, 0.0f, 255.0f));
                iconPixels[idx + 1] = static_cast<uint8_t>(std::clamp(color.g * 255.0f, 0.0f, 255.0f));
                iconPixels[idx + 2] = static_cast<uint8_t>(std::clamp(color.b * 255.0f, 0.0f, 255.0f));
                iconPixels[idx + 3] = 255;
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
            const auto& blockProps = blockReg.get(props.blockId);
            // Use top, south (front), east (right side) faces
            renderBlockIcon(iconPixels.data(), blockAtlas, blockAtlasPixels, blockAtlasWidth,
                           blockProps.textures.top, blockProps.textures.south, blockProps.textures.east);
        } else {
            // Non-block item placeholder
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
