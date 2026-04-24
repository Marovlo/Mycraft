#pragma once

#include "renderer/ui_renderer.h"
#include "renderer/item_icon.h"
#include "player/inventory.h"

// Draws the in-game HUD: hotbar, crosshair, and later health/hunger bars.
class HUD {
public:
    void init(UIRenderer* ui, ItemIconAtlas* iconAtlas, VulkanEngine* engine);

    // Draw HUD elements. Call once per frame before UIRenderer::flush().
    void draw(float screenW, float screenH, const Inventory& inventory);

private:
    UIRenderer* ui_ = nullptr;
    ItemIconAtlas* iconAtlas_ = nullptr;
    VulkanEngine* engine_ = nullptr;

    // Whether icon atlas is bound as the active texture this frame
    bool iconsBound_ = false;

    void drawHotbar(float screenW, float screenH, const Inventory& inventory);
    void drawCrosshair(float screenW, float screenH);
};
