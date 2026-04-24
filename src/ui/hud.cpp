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
    ui_->drawCrosshair(screenW, screenH, 40.0f, 4.0f);
}

void HUD::drawHotbar(float screenW, float screenH, const Inventory& inventory) {
    const float slotSize = 64.0f;
    const float gap = 4.0f;
    const float border = 3.0f;
    const float totalWidth = Inventory::HOTBAR_SIZE * slotSize + (Inventory::HOTBAR_SIZE - 1) * gap;
    const float startX = (screenW - totalWidth) * 0.5f;
    const float startY = screenH - slotSize - 12.0f;

    int selected = inventory.getSelectedSlot();

    for (int i = 0; i < Inventory::HOTBAR_SIZE; i++) {
        float x = startX + i * (slotSize + gap);
        float y = startY;

        // Selection highlight: thin white border BEHIND the slot
        if (i == selected) {
            glm::vec4 selColor(1.0f, 1.0f, 1.0f, 0.9f);
            // Top edge
            ui_->drawRect(x - border, y - border, slotSize + border * 2, border, selColor);
            // Bottom edge
            ui_->drawRect(x - border, y + slotSize, slotSize + border * 2, border, selColor);
            // Left edge
            ui_->drawRect(x - border, y, border, slotSize, selColor);
            // Right edge
            ui_->drawRect(x + slotSize, y, border, slotSize, selColor);
        }

        // Slot background (drawn ON TOP of border)
        glm::vec4 bgColor(0.15f, 0.15f, 0.15f, 0.75f);
        ui_->drawRect(x, y, slotSize, slotSize, bgColor);

        // Item icon
        const auto& stack = inventory.getSlot(i);
        if (!stack.isEmpty()) {
            glm::vec4 uv = iconAtlas_->getIconUV(stack.id);
            if (uv.z > uv.x) {
                float iconPad = 4.0f;
                ui_->drawTexturedRect(x + iconPad, y + iconPad,
                                     slotSize - iconPad * 2, slotSize - iconPad * 2,
                                     uv.x, uv.y, uv.z, uv.w);
            }
        }
    }
}
