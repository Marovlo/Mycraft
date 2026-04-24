#include "hud.h"
#include "core/item.h"

void HUD::init(UIRenderer* ui, ItemIconAtlas* iconAtlas, VulkanEngine* engine) {
    ui_ = ui;
    iconAtlas_ = iconAtlas;
    engine_ = engine;
}

void HUD::draw(float screenW, float screenH, const Inventory& inventory) {
    drawCrosshair(screenW, screenH);
    drawHotbar(screenW, screenH, inventory);
}

void HUD::drawCrosshair(float screenW, float screenH) {
    ui_->drawCrosshair(screenW, screenH, 30.0f, 3.0f);
}

void HUD::drawHotbar(float screenW, float screenH, const Inventory& inventory) {
    // Hotbar dimensions
    const float slotSize = 48.0f;
    const float padding = 4.0f;
    const float borderWidth = 2.0f;
    const float totalWidth = Inventory::HOTBAR_SIZE * (slotSize + padding) - padding;
    const float startX = (screenW - totalWidth) * 0.5f;
    const float startY = screenH - slotSize - 16.0f;  // 16px from bottom

    int selected = inventory.getSelectedSlot();

    for (int i = 0; i < Inventory::HOTBAR_SIZE; i++) {
        float x = startX + i * (slotSize + padding);
        float y = startY;

        // Slot background
        glm::vec4 bgColor(0.1f, 0.1f, 0.1f, 0.7f);
        ui_->drawRect(x, y, slotSize, slotSize, bgColor);

        // Selection highlight
        if (i == selected) {
            glm::vec4 selColor(1.0f, 1.0f, 1.0f, 0.5f);
            ui_->drawRect(x - borderWidth, y - borderWidth,
                         slotSize + borderWidth * 2, slotSize + borderWidth * 2, selColor);
            // Redraw background on top of highlight border
            ui_->drawRect(x, y, slotSize, slotSize, bgColor);
        }

        // Item icon
        const auto& stack = inventory.getSlot(i);
        if (!stack.isEmpty()) {
            glm::vec4 uv = iconAtlas_->getIconUV(stack.id);
            if (uv.z > uv.x) {  // valid UV
                float iconPad = 4.0f;
                ui_->drawTexturedRect(x + iconPad, y + iconPad,
                                     slotSize - iconPad * 2, slotSize - iconPad * 2,
                                     uv.x, uv.y, uv.z, uv.w);
            }
        }
    }
}
