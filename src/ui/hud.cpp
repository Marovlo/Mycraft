#include "hud.h"
#include "core/item.h"
#include "core/block.h"
#include <algorithm>
#include <cmath>

void HUD::init(UIRenderer* ui, BlockModelRenderer* blockModel,
               TextureAtlas* atlas, VulkanEngine* engine) {
    ui_ = ui;
    blockModel_ = blockModel;
    atlas_ = atlas;
    engine_ = engine;
}

int HUD::getGuiScale(float screenH) {
    int s = static_cast<int>(screenH / 240.0f);
    return std::clamp(s, 2, 4);
}

float HUD::getHotbarStartX(float screenW, int scale) const {
    float slot = SLOT_SIZE * scale;
    float gap  = GAP       * scale;
    float total = Inventory::HOTBAR_SIZE * slot + (Inventory::HOTBAR_SIZE - 1) * gap;
    return (screenW - total) * 0.5f;
}

float HUD::getHotbarStartY(float screenH, int scale) const {
    return screenH - SLOT_SIZE * scale - BOTTOM_MARGIN * scale;
}

void HUD::draw(float screenW, float screenH, const Inventory& inventory,
               float breakProgress, int gameTicks,
               int hp, int maxHp, int hunger, int maxHunger, bool isDead,
               bool isEating, int air, int maxAir, int hurtTicks) {
    int   scale     = getGuiScale(screenH);
    float slotPx    = SLOT_SIZE * scale;
    float gapPx     = GAP       * scale;
    float borderPx  = BORDER    * scale;
    float iconPadPx = ICON_PAD  * scale;
    float startX    = getHotbarStartX(screenW, scale);
    float startY    = getHotbarStartY(screenH, scale);
    int   selected  = inventory.getSelectedSlot();

    const auto& itemReg = ItemRegistry::instance();

    // 1) Slot backgrounds (semi-transparent dark squares)
    for (int i = 0; i < Inventory::HOTBAR_SIZE; i++) {
        float x = startX + i * (slotPx + gapPx);
        float y = startY;
        ui_->drawRect(x, y, slotPx, slotPx, glm::vec4(0.10f, 0.10f, 0.10f, 0.78f));
    }

    // 2) Selection border around the chosen slot (4 thin white edges)
    {
        float x = startX + selected * (slotPx + gapPx);
        float y = startY;
        glm::vec4 sel(1.0f, 1.0f, 1.0f, 1.0f);
        ui_->drawRect(x - borderPx,  y - borderPx,  slotPx + borderPx * 2, borderPx, sel);
        ui_->drawRect(x - borderPx,  y + slotPx,    slotPx + borderPx * 2, borderPx, sel);
        ui_->drawRect(x - borderPx,  y,             borderPx, slotPx, sel);
        ui_->drawRect(x + slotPx,    y,             borderPx, slotPx, sel);
    }

    // 3) 3D-projected block icons (or 2D textured quads for non-block items in the future)
    for (int i = 0; i < Inventory::HOTBAR_SIZE; i++) {
        const ItemStack& stack = inventory.getSlot(i);
        if (stack.isEmpty()) continue;

        const auto& props = itemReg.get(stack.id);
        float x   = startX + i * (slotPx + gapPx);
        float y   = startY;
        float ix  = x + iconPadPx;
        float iy  = y + iconPadPx;
        float iSz = slotPx - iconPadPx * 2;

        if (props.type == ItemType::Block && props.blockId > 0
            && BlockRegistry::instance().get(props.blockId).renderType != BlockRenderType::Cross) {
            blockModel_->enqueueBlockIcon(*ui_, props.blockId, *atlas_, ix, iy, iSz);
        } else if (!props.iconTileName.empty()) {
            // 2D sprite icon for tools / materials.
            uint16_t tile = atlas_->getTileIndex(props.iconTileName);
            glm::vec4 uv = atlas_->getTileUV(tile);
            ui_->drawTexturedRect(ix, iy, iSz, iSz, uv.x, uv.y, uv.z, uv.w);
        }

        // Count label (only draw when > 1 — matches MC convention).
        if (stack.count > 1) {
            float glyphH = slotPx * 0.35f;
            float rightX = x + slotPx - borderPx;
            float labelY = y + slotPx - glyphH - borderPx;
            ui_->drawNumber(static_cast<int>(stack.count), rightX, labelY, glyphH);
        }

        // Durability bar (only when the item HAS durability AND has taken damage).
        // MC draws it at the slot bottom: thin horizontal bar, green→red.
        if (props.durability > 0 && stack.durability > 0) {
            float ratio = 1.0f - static_cast<float>(stack.durability) / props.durability;
            ratio = std::clamp(ratio, 0.0f, 1.0f);
            float barH = 2.0f * scale;
            float barPad = 2.0f * scale;   // inset from slot edges
            float barFullW = slotPx - barPad * 2;
            float bx = x + barPad;
            float by = y + slotPx - barH - barPad;
            // Background (dark)
            ui_->drawRect(bx, by, barFullW, barH, glm::vec4(0.0f, 0.0f, 0.0f, 0.8f));
            // Fill: green (1.0) → yellow (0.5) → red (0.0)
            glm::vec4 col;
            if (ratio > 0.5f) {
                float t = (ratio - 0.5f) * 2.0f; // 0..1
                col = glm::vec4(1.0f - t, 1.0f, 0.0f, 1.0f); // yellow→green
            } else {
                float t = ratio * 2.0f; // 0..1
                col = glm::vec4(1.0f, t, 0.0f, 1.0f); // red→yellow
            }
            ui_->drawRect(bx, by, barFullW * ratio, barH, col);
        }
    }

    // 3.5) Mining progress bar — shown just above the hotbar while a block
    //      is being broken. Width = hotbar width, color blends white→red.
    if (breakProgress > 0.0f) {
        float p = std::clamp(breakProgress, 0.0f, 1.0f);
        float barH = 3.0f * scale;
        float barW = Inventory::HOTBAR_SIZE * slotPx + (Inventory::HOTBAR_SIZE - 1) * gapPx;
        float bx = startX;
        float by = startY - barH - 2.0f * scale;
        // Track (dim background)
        ui_->drawRect(bx, by, barW, barH, glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));
        // Fill — interpolate from white to red as p grows.
        glm::vec4 fill(1.0f, 1.0f - p * 0.7f, 1.0f - p, 1.0f);
        ui_->drawRect(bx, by, barW * p, barH, fill);
    }

    // 4) First-person viewmodel — held item in the lower-right corner.
    //    Block items: 3D-projected cube (larger than hotbar icon).
    //    Tool items:  2D sprite.
    //    Empty hand:  nothing.
    {
        const ItemStack& held = inventory.getSlot(inventory.getSelectedSlot());
        if (!held.isEmpty()) {
            const auto& heldProps = itemReg.get(held.id);
            float vmSize = screenH * 0.25f;   // 25% of screen height
            float baseX = screenW - vmSize - 10.0f * scale;
            float baseY = screenH - vmSize - 10.0f * scale;

            // Swing animation when mining or eating.
            float swing = 0.0f;
            if (breakProgress > 0.0f || isEating) {
                swing = std::sin(static_cast<float>(gameTicks) * 0.6f) * vmSize * 0.08f;
            }
            float drawY = baseY + swing;

            if (heldProps.type == ItemType::Block && heldProps.blockId > 0
                && BlockRegistry::instance().get(heldProps.blockId).renderType != BlockRenderType::Cross) {
                blockModel_->enqueueBlockIcon(*ui_, heldProps.blockId, *atlas_,
                                              baseX, drawY, vmSize);
            } else if (!heldProps.iconTileName.empty()) {
                uint16_t tile = atlas_->getTileIndex(heldProps.iconTileName);
                glm::vec4 uv = atlas_->getTileUV(tile);
                ui_->drawTexturedRect(baseX, drawY, vmSize, vmSize,
                                      uv.x, uv.y, uv.z, uv.w);
            }
        }
    }

    // 5) Crosshair last so it sits on top in screen center.
    //    Scale crosshair too so it stays visible on Retina displays.
    ui_->drawCrosshair(screenW, screenH, 8.0f * scale, 1.0f * scale);

    // 6) Health hearts — left side above hotbar.
    float iconSize = 8.0f * scale;   // slightly larger icons
    float iconGap  = 0.5f * scale;
    float statusRowY = startY - iconSize - 4.0f * scale;
    {
        int hearts = (maxHp + 1) / 2;
        uint16_t tileFull  = atlas_->getTileIndex("hud_heart_full");
        uint16_t tileHalf  = atlas_->getTileIndex("hud_heart_half");
        uint16_t tileEmpty = atlas_->getTileIndex("hud_heart_empty");

        for (int i = 0; i < hearts; ++i) {
            float cx = startX + i * (iconSize + iconGap);
            int hpHere = hp - i * 2;
            uint16_t tile;
            if (hpHere >= 2)      tile = tileFull;
            else if (hpHere == 1) tile = tileHalf;
            else                  tile = tileEmpty;
            glm::vec4 uv = atlas_->getTileUV(tile);
            ui_->drawTexturedRect(cx, statusRowY, iconSize, iconSize,
                                  uv.x, uv.y, uv.z, uv.w);
        }
    }

    // Hunger drumsticks — right side, same row as hearts.
    {
        int drumsticks = (maxHunger + 1) / 2;
        float totalHungerW = drumsticks * (iconSize + iconGap) - iconGap;
        float hungerStartX = startX + slotPx * Inventory::HOTBAR_SIZE
                            + gapPx * (Inventory::HOTBAR_SIZE - 1) - totalHungerW;

        uint16_t drumFull  = atlas_->getTileIndex("hud_drumstick_full");
        uint16_t drumHalf  = atlas_->getTileIndex("hud_drumstick_half");
        uint16_t drumEmpty = atlas_->getTileIndex("hud_drumstick_empty");

        for (int i = 0; i < drumsticks; ++i) {
            float cx = hungerStartX + i * (iconSize + iconGap);
            int reverseI = drumsticks - 1 - i;
            int hungerHere = hunger - reverseI * 2;
            uint16_t tile;
            if (hungerHere >= 2)      tile = drumFull;
            else if (hungerHere == 1) tile = drumHalf;
            else                      tile = drumEmpty;
            glm::vec4 uv = atlas_->getTileUV(tile);
            ui_->drawTexturedRect(cx, statusRowY, iconSize, iconSize,
                                  uv.x, uv.y, uv.z, uv.w);
        }

        // Air bubbles — above drumsticks, same X alignment, only when air < maxAir.
        if (air < maxAir) {
            int bubbles = 10;  // always 10, matching drumstick count
            int airFilled = (air * 10 + maxAir - 1) / maxAir;  // scale to 0-10
            airFilled = std::clamp(airFilled, 0, 10);
            float bubbleY = statusRowY - iconSize - 2.0f * scale;

            uint16_t bubbleFull  = atlas_->getTileIndex("hud_bubble_full");
            uint16_t bubbleEmpty = atlas_->getTileIndex("hud_bubble_empty");

            for (int i = 0; i < bubbles; ++i) {
                float bx = hungerStartX + i * (iconSize + iconGap);
                uint16_t tile = (i < airFilled) ? bubbleFull : bubbleEmpty;
                glm::vec4 uv = atlas_->getTileUV(tile);
                ui_->drawTexturedRect(bx, bubbleY, iconSize, iconSize,
                                      uv.x, uv.y, uv.z, uv.w);
            }
        }
    }

    // 6c) Low health vignette — subtle red border that intensifies as HP drops.
    if (hp < maxHp && hp > 0 && !isDead) {
        float hpRatio = static_cast<float>(hp) / maxHp;
        float intensity = (1.0f - hpRatio) * 0.3f;  // max 30% opacity at 1 HP
        glm::vec4 redTint(0.6f, 0.0f, 0.0f, intensity);
        float border = screenW * 0.08f;
        // Top
        ui_->drawRect(0, 0, screenW, border, redTint);
        // Bottom
        ui_->drawRect(0, screenH - border, screenW, border, redTint);
        // Left
        ui_->drawRect(0, 0, border, screenH, redTint);
        // Right
        ui_->drawRect(screenW - border, 0, border, screenH, redTint);
    }

    // 6d) Damage flash — brief red overlay when recently hurt.
    if (hurtTicks > 0 && !isDead) {
        float flash = static_cast<float>(hurtTicks) / 10.0f * 0.25f;
        ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.8f, 0.0f, 0.0f, flash));
    }

    // 7) Item name display — show held item name above hotbar (MC behavior).
    {
        const ItemStack& held = inventory.getSlot(inventory.getSelectedSlot());
        if (!held.isEmpty()) {
            const auto& nameProps = itemReg.get(held.id);
            if (!nameProps.displayName.empty()) {
                float nameH = 7.0f * scale;
                float nameY = startY - nameH - 12.0f * scale;
                ui_->drawText(nameProps.displayName, screenW * 0.5f, nameY, nameH,
                              glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
            }
        }
    }

    // 8) Death overlay
    if (isDead) {
        ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.5f, 0.0f, 0.0f, 0.55f));
        float titleH = 16.0f * scale;
        ui_->drawText("YOU DIED", screenW * 0.5f, screenH * 0.35f, titleH,
                      glm::vec4(1.0f, 0.3f, 0.3f, 1.0f));
        float hintH = 8.0f * scale;
        ui_->drawText("PRESS R TO RESPAWN", screenW * 0.5f, screenH * 0.48f, hintH,
                      glm::vec4(1.0f, 0.7f, 0.7f, 0.8f));
    }
}
