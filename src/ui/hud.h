#pragma once

#include "renderer/ui_renderer.h"
#include "renderer/block_model.h"
#include "renderer/texture_atlas.h"
#include "renderer/gui_atlas.h"
#include "player/inventory.h"

// HUD enqueues 2D quads + projected block icons into a single UIRenderer batch.
// 使用原版 MC GUI 精灵图渲染快捷栏、心形、饥饿值、经验条等。
class HUD {
public:
    void init(UIRenderer* ui, BlockModelRenderer* blockModel,
              TextureAtlas* atlas, VulkanEngine* engine);

    // Queue every HUD element for this frame. UIRenderer::flush() will draw them later.
    void draw(float screenW, float screenH, const Inventory& inventory,
              float breakProgress, int gameTicks,
              int hp, int maxHp, int hunger, int maxHunger, bool isDead,
              bool isEating = false, int air = 300, int maxAir = 300,
              int hurtTicks = 0, bool targetingMob = false,
              int xpLevel = 0, float xpProgress = 0.0f);

private:
    UIRenderer* ui_ = nullptr;
    BlockModelRenderer* blockModel_ = nullptr;
    TextureAtlas* atlas_ = nullptr;
    VulkanEngine* engine_ = nullptr;

    // MC 原版 HUD 布局常量（基于原版 GUI 精灵图像素尺寸）
    // hotbar.png = 182x22, hotbar_selection.png = 24x23
    // 心形/饥饿值/气泡 = 9x9, 经验条 = 182x5
    // 所有尺寸乘以 guiScale 后渲染

    // Auto GUI scale, MC-style: 1× per ~240 px of screen height, clamped to [2,4].
    static int getGuiScale(float screenH);
};
