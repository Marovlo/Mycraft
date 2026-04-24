#pragma once

#include "renderer/ui_renderer.h"
#include "renderer/block_model.h"
#include "renderer/texture_atlas.h"
#include "player/inventory.h"

class HUD {
public:
    void init(UIRenderer* ui, BlockModelRenderer* blockModel,
              TextureAtlas* atlas, VulkanEngine* engine);

    // Draw HUD backgrounds (crosshair, hotbar slots). Call before render3D.
    void drawBackgrounds(float screenW, float screenH, const Inventory& inventory);

    // Render 3D block icons in hotbar slots. Call during 3D render pass.
    void render3DIcons(VkCommandBuffer cmd, float screenW, float screenH,
                       const Inventory& inventory,
                       uint32_t fullW, uint32_t fullH);

private:
    UIRenderer* ui_ = nullptr;
    BlockModelRenderer* blockModel_ = nullptr;
    TextureAtlas* atlas_ = nullptr;
    VulkanEngine* engine_ = nullptr;

    // Hotbar layout constants
    static constexpr float SLOT_SIZE = 64.0f;
    static constexpr float GAP = 4.0f;
    static constexpr float BORDER = 1.5f;
    static constexpr float BOTTOM_MARGIN = 12.0f;
    static constexpr float ICON_PAD = 6.0f;

    float getHotbarStartX(float screenW) const;
    float getHotbarStartY(float screenH) const;
};
