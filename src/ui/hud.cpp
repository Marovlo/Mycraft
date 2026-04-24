#include "hud.h"
#include "core/item.h"

void HUD::init(UIRenderer* ui, BlockModelRenderer* blockModel,
               TextureAtlas* atlas, VulkanEngine* engine) {
    ui_ = ui;
    blockModel_ = blockModel;
    atlas_ = atlas;
    engine_ = engine;
}

float HUD::getHotbarStartX(float screenW) const {
    float totalWidth = Inventory::HOTBAR_SIZE * SLOT_SIZE + (Inventory::HOTBAR_SIZE - 1) * GAP;
    return (screenW - totalWidth) * 0.5f;
}

float HUD::getHotbarStartY(float screenH) const {
    return screenH - SLOT_SIZE - BOTTOM_MARGIN;
}

void HUD::drawBackgrounds(float screenW, float screenH, const Inventory& inventory) {
    // Crosshair
    ui_->drawCrosshair(screenW, screenH, 40.0f, 4.0f);

    float startX = getHotbarStartX(screenW);
    float startY = getHotbarStartY(screenH);
    int selected = inventory.getSelectedSlot();

    for (int i = 0; i < Inventory::HOTBAR_SIZE; i++) {
        float x = startX + i * (SLOT_SIZE + GAP);
        float y = startY;

        // Selection border (thin white lines)
        if (i == selected) {
            glm::vec4 sel(1.0f, 1.0f, 1.0f, 1.0f);
            ui_->drawRect(x - BORDER, y - BORDER, SLOT_SIZE + BORDER * 2, BORDER, sel);
            ui_->drawRect(x - BORDER, y + SLOT_SIZE, SLOT_SIZE + BORDER * 2, BORDER, sel);
            ui_->drawRect(x - BORDER, y, BORDER, SLOT_SIZE, sel);
            ui_->drawRect(x + SLOT_SIZE, y, BORDER, SLOT_SIZE, sel);
        }

        // Slot background
        ui_->drawRect(x, y, SLOT_SIZE, SLOT_SIZE, glm::vec4(0.15f, 0.15f, 0.15f, 0.75f));
    }
}

void HUD::render3DIcons(VkCommandBuffer cmd, float screenW, float screenH,
                         const Inventory& inventory,
                         uint32_t fullW, uint32_t fullH) {
    // Temporarily disabled — will implement with proper second UBO
    // The single-UBO approach causes white screen because GPU reads
    // the last written UBO value for ALL draw calls in the command buffer
}
