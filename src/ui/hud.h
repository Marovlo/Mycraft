#pragma once

#include "renderer/ui_renderer.h"
#include "renderer/block_model.h"
#include "renderer/texture_atlas.h"
#include "player/inventory.h"

// HUD enqueues 2D quads + projected block icons into a single UIRenderer batch.
// Render order is: slot background → selection border → icon (per slot) → crosshair.
// All draws share the block-atlas texture, so they fit one descriptor / one draw call.
class HUD {
public:
    void init(UIRenderer* ui, BlockModelRenderer* blockModel,
              TextureAtlas* atlas, VulkanEngine* engine);

    // Queue every HUD element for this frame. UIRenderer::flush() will draw them later.
    // breakProgress: 0..1, < 0 means "no active mining" (no bar drawn).
    // gameTicks: total ticks since start, used for viewmodel swing animation.
    // hp / maxHp: player health for heart display.
    // isDead: show death overlay.
    void draw(float screenW, float screenH, const Inventory& inventory,
              float breakProgress, int gameTicks,
              int hp, int maxHp, int hunger, int maxHunger, bool isDead,
              bool isEating = false, int air = 300, int maxAir = 300,
              int hurtTicks = 0, bool targetingMob = false);

private:
    UIRenderer* ui_ = nullptr;
    BlockModelRenderer* blockModel_ = nullptr;
    TextureAtlas* atlas_ = nullptr;
    VulkanEngine* engine_ = nullptr;

    // Hotbar layout constants in *base* (unscaled) pixels.
    // Final pixel sizes are multiplied by getGuiScale(screenH) so the HUD
    // looks correct on Retina / high-DPI framebuffers.
    static constexpr float SLOT_SIZE     = 20.0f;
    static constexpr float GAP           = 2.0f;
    static constexpr float BORDER        = 1.0f;
    static constexpr float BOTTOM_MARGIN = 8.0f;
    static constexpr float ICON_PAD      = 2.0f;

    // Auto GUI scale, MC-style: 1× per ~240 px of screen height, clamped to [2,4].
    // This keeps the HUD visually consistent across 720p ↔ 4K framebuffers.
    static int getGuiScale(float screenH);

    float getHotbarStartX(float screenW, int scale) const;
    float getHotbarStartY(float screenH, int scale) const;
};
