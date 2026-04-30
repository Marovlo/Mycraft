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

void HUD::draw(float screenW, float screenH, const Inventory& inventory,
               float breakProgress, int gameTicks,
               int hp, int maxHp, int hunger, int maxHunger, bool isDead,
               bool isEating, int air, int maxAir, int hurtTicks,
               bool targetingMob, int xpLevel, float xpProgress) {
    int   scale = getGuiScale(screenH);
    float s     = static_cast<float>(scale);
    int   selected = inventory.getSelectedSlot();
    const auto& itemReg = ItemRegistry::instance();

    // ============================================================
    // MC 原版 HUD 布局（基于原版精灵图像素尺寸 × guiScale）
    // hotbar.png = 182x22 像素
    // hotbar_selection.png = 24x23 像素
    // 每个格子 = 20x20 像素（在 hotbar 内部）
    // ============================================================

    // 快捷栏尺寸
    float hotbarW = 182.0f * s;
    float hotbarH = 22.0f * s;
    float hotbarX = (screenW - hotbarW) * 0.5f;
    float hotbarY = screenH - hotbarH;

    // 1) 快捷栏背景（使用原版 hotbar.png）
    ui_->drawGuiSprite("hud/hotbar", hotbarX, hotbarY, hotbarW, hotbarH);

    // 2) 选中框（使用原版 hotbar_selection.png）
    // MC 原版：选中框 24x23，比格子大 2px 边距
    // 第一个格子左边缘在 hotbar 内偏移 3px，每格 20px
    float selW = 24.0f * s;
    float selH = 23.0f * s;
    float slotStartX = hotbarX + 3.0f * s;  // hotbar 内第一个格子的 X 偏移
    float selX = slotStartX + selected * 20.0f * s - 2.0f * s;  // 选中框比格子大 2px
    float selY = hotbarY - 1.0f * s;  // 选中框顶部比 hotbar 高 1px
    ui_->drawGuiSprite("hud/hotbar_selection", selX, selY, selW, selH);

    // 3) 物品图标（在快捷栏格子内渲染）
    float slotSize = 16.0f * s;  // 物品图标 16x16
    float iconPad  = 3.0f * s;   // 格子内边距（20px 格子 - 16px 图标 = 4px，居中 = 2px）
    for (int i = 0; i < Inventory::HOTBAR_SIZE; i++) {
        const ItemStack& stack = inventory.getSlot(i);
        if (stack.isEmpty()) continue;

        const auto& props = itemReg.get(stack.id);
        float ix = slotStartX + i * 20.0f * s + 2.0f * s;  // 居中偏移
        float iy = hotbarY + 3.0f * s;  // 垂直居中
        float iSz = slotSize;

        if (props.type == ItemType::Block && props.blockId > 0
            && BlockRegistry::instance().get(props.blockId).renderType != BlockRenderType::Cross) {
            blockModel_->enqueueBlockIcon(*ui_, props.blockId, *atlas_, ix, iy, iSz);
        } else if (!props.iconTileName.empty()) {
            uint16_t tile = atlas_->getTileIndex(props.iconTileName);
            glm::vec4 uv = atlas_->getTileUV(tile);
            ui_->drawTexturedRect(ix, iy, iSz, iSz, uv.x, uv.y, uv.z, uv.w);
        }

        // 数量标签
        if (stack.count > 1) {
            float glyphH = 8.0f * s * 0.35f;
            float rightX = slotStartX + i * 20.0f * s + 18.0f * s;
            float labelY = hotbarY + 20.0f * s - glyphH - 1.0f * s;
            ui_->drawNumber(static_cast<int>(stack.count), rightX, labelY, glyphH);
        }

        // 耐久条
        if (props.durability > 0 && stack.durability > 0) {
            float ratio = 1.0f - static_cast<float>(stack.durability) / props.durability;
            ratio = std::clamp(ratio, 0.0f, 1.0f);
            float barH = 2.0f * s;
            float barPad = 2.0f * s;
            float barFullW = 16.0f * s - barPad * 2;
            float bx = ix + barPad * 0.5f;
            float by = iy + slotSize - barH - 1.0f * s;
            ui_->drawRect(bx, by, barFullW, barH, glm::vec4(0.0f, 0.0f, 0.0f, 0.8f));
            glm::vec4 col;
            if (ratio > 0.5f) {
                float t = (ratio - 0.5f) * 2.0f;
                col = glm::vec4(1.0f - t, 1.0f, 0.0f, 1.0f);
            } else {
                float t = ratio * 2.0f;
                col = glm::vec4(1.0f, t, 0.0f, 1.0f);
            }
            ui_->drawRect(bx, by, barFullW * ratio, barH, col);
        }
    }

    // 3.5) 挖掘进度条 — 快捷栏上方
    if (breakProgress > 0.0f) {
        float p = std::clamp(breakProgress, 0.0f, 1.0f);
        float barH = 3.0f * s;
        float barW = hotbarW;
        float bx = hotbarX;
        float by = hotbarY - barH - 2.0f * s;
        ui_->drawRect(bx, by, barW, barH, glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));
        glm::vec4 fill(1.0f, 1.0f - p * 0.7f, 1.0f - p, 1.0f);
        ui_->drawRect(bx, by, barW * p, barH, fill);
    }

    // 4) 准星（使用原版 crosshair.png，15x15 像素）
    {
        float crossSize = 15.0f * s;
        float cx = (screenW - crossSize) * 0.5f;
        float cy = (screenH - crossSize) * 0.5f;
        ui_->drawGuiSprite("hud/crosshair", cx, cy, crossSize, crossSize);
    }

    // 4b) 准星指向生物时显示攻击指示器
    if (targetingMob && !isDead) {
        float cx = screenW * 0.5f;
        float cy = screenH * 0.5f;
        float iconSz = 6.0f * s;
        float offsetY = 6.0f * s;
        glm::vec4 swordColor(1.0f, 0.85f, 0.6f, 0.9f);
        float bladeW = 1.0f * s;
        float bladeH = iconSz;
        ui_->drawRect(cx + 1.0f * s, cy + offsetY, bladeW, bladeH, swordColor);
        float handleW = 3.0f * s;
        float handleH = 1.0f * s;
        ui_->drawRect(cx - 0.5f * s, cy + offsetY + bladeH, handleW, handleH,
                      glm::vec4(0.6f, 0.4f, 0.2f, 0.9f));
        ui_->drawRect(cx + 1.0f * s, cy + offsetY - 1.0f * s, bladeW, 1.0f * s,
                      glm::vec4(0.9f, 0.9f, 0.9f, 0.9f));
    }

    // ============================================================
    // 5) 状态栏 — 快捷栏上方
    // MC 原版布局：
    //   左侧：心形容器 + 心形（10 个，每个 9x9 像素，间距 8px）
    //   右侧：饥饿值（10 个，从右到左）
    //   心形上方：护甲值（如果有）
    //   饥饿值上方：气泡（如果在水下）
    // ============================================================

    float iconSize = 9.0f * s;  // 原版心形/饥饿值图标 9x9 像素
    float iconStep = 8.0f * s;  // MC 原版：每个图标间距 8px（有 1px 重叠）
    float statusY = hotbarY - 10.0f * s;  // 状态栏 Y 位置

    // 5a) 心形 — 左侧
    {
        int hearts = (maxHp + 1) / 2;
        float heartStartX = hotbarX;

        for (int i = 0; i < hearts; ++i) {
            float cx = heartStartX + i * iconStep;

            // 先画心形容器（背景）
            ui_->drawGuiSprite("hud/heart/container", cx, statusY, iconSize, iconSize);

            // 再画心形填充
            int hpHere = hp - i * 2;
            if (hpHere >= 2) {
                ui_->drawGuiSprite("hud/heart/full", cx, statusY, iconSize, iconSize);
            } else if (hpHere == 1) {
                ui_->drawGuiSprite("hud/heart/half", cx, statusY, iconSize, iconSize);
            }
            // hpHere <= 0: 只显示容器（空心）
        }
    }

    // 5b) 饥饿值 — 右侧（从右到左）
    {
        int drumsticks = (maxHunger + 1) / 2;
        float hungerEndX = hotbarX + hotbarW;  // 右对齐

        for (int i = 0; i < drumsticks; ++i) {
            // 从右到左排列
            float cx = hungerEndX - (i + 1) * iconStep - (iconSize - iconStep);

            // 先画空饥饿值容器
            ui_->drawGuiSprite("hud/food_empty", cx, statusY, iconSize, iconSize);

            // 再画填充
            int reverseI = drumsticks - 1 - i;
            int hungerHere = hunger - reverseI * 2;
            if (hungerHere >= 2) {
                ui_->drawGuiSprite("hud/food_full", cx, statusY, iconSize, iconSize);
            } else if (hungerHere == 1) {
                ui_->drawGuiSprite("hud/food_half", cx, statusY, iconSize, iconSize);
            }
        }

        // 5c) 气泡 — 饥饿值上方（仅在水下时显示）
        if (air < maxAir) {
            int bubbles = 10;
            int airFilled = (air * 10 + maxAir - 1) / maxAir;
            airFilled = std::clamp(airFilled, 0, 10);
            float bubbleY = statusY - iconSize - 2.0f * s;

            for (int i = 0; i < bubbles; ++i) {
                float bx = hungerEndX - (i + 1) * iconStep - (iconSize - iconStep);
                if (bubbles - 1 - i < airFilled) {
                    ui_->drawGuiSprite("hud/air", bx, bubbleY, iconSize, iconSize);
                } else {
                    ui_->drawGuiSprite("hud/air_empty", bx, bubbleY, iconSize, iconSize);
                }
            }
        }
    }

    // 5d) 低血量红色边框
    if (hp < maxHp && hp > 0 && !isDead) {
        float hpRatio = static_cast<float>(hp) / maxHp;
        float intensity = (1.0f - hpRatio) * 0.3f;
        glm::vec4 redTint(0.6f, 0.0f, 0.0f, intensity);
        float border = screenW * 0.08f;
        ui_->drawRect(0, 0, screenW, border, redTint);
        ui_->drawRect(0, screenH - border, screenW, border, redTint);
        ui_->drawRect(0, 0, border, screenH, redTint);
        ui_->drawRect(screenW - border, 0, border, screenH, redTint);
    }

    // 5e) 受伤闪红
    if (hurtTicks > 0 && !isDead) {
        float flash = static_cast<float>(hurtTicks) / 10.0f * 0.25f;
        ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.8f, 0.0f, 0.0f, flash));
    }

    // ============================================================
    // 6) 经验条 — 使用原版 experience_bar 精灵图（182x5 像素）
    // ============================================================
    if (!isDead) {
        float xpBarW = 182.0f * s;
        float xpBarH = 5.0f * s;
        float xpBarX = hotbarX;
        float xpBarY = hotbarY - xpBarH;

        // 经验条背景
        ui_->drawGuiSprite("hud/experience_bar_background", xpBarX, xpBarY, xpBarW, xpBarH);

        // 经验条填充（裁剪到当前进度）
        if (xpProgress > 0.0f) {
            float fillW = xpBarW * std::clamp(xpProgress, 0.0f, 1.0f);
            float srcW = 182.0f * std::clamp(xpProgress, 0.0f, 1.0f);
            ui_->drawGuiSpriteRegion("hud/experience_bar_progress",
                                     xpBarX, xpBarY, fillW, xpBarH,
                                     0.0f, 0.0f, srcW, 5.0f);
        }

        // 等级数字
        if (xpLevel > 0) {
            float numH = 6.0f * s;
            float numY = xpBarY - numH - 1.0f * s;
            // 黑色阴影
            ui_->drawNumber(xpLevel, screenW * 0.5f + 1.0f * s, numY + 1.0f * s, numH,
                            glm::vec4(0.0f, 0.0f, 0.0f, 0.8f));
            // 绿色数字
            ui_->drawNumber(xpLevel, screenW * 0.5f, numY, numH,
                            glm::vec4(0.5f, 1.0f, 0.2f, 1.0f));
        }
    }

    // 7) 物品名称显示
    {
        const ItemStack& held = inventory.getSlot(inventory.getSelectedSlot());
        if (!held.isEmpty()) {
            const auto& nameProps = itemReg.get(held.id);
            if (!nameProps.displayName.empty()) {
                float nameH = 7.0f * s;
                float nameY = hotbarY - 20.0f * s - nameH;
                ui_->drawText(nameProps.displayName, screenW * 0.5f, nameY, nameH,
                              glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
            }
        }
    }

    // 8) 死亡覆盖层
    if (isDead) {
        ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.5f, 0.0f, 0.0f, 0.55f));
        float titleH = 16.0f * s;
        ui_->drawText("YOU DIED", screenW * 0.5f, screenH * 0.35f, titleH,
                      glm::vec4(1.0f, 0.3f, 0.3f, 1.0f));
        float hintH = 8.0f * s;
        ui_->drawText("PRESS R TO RESPAWN", screenW * 0.5f, screenH * 0.48f, hintH,
                      glm::vec4(1.0f, 0.7f, 0.7f, 0.8f));
    }
}
