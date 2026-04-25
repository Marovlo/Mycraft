#include "furnace_screen.h"
#include "crafting/smelting_recipe.h"
#include "core/debug.h"

void FurnaceScreen::open() {
    ContainerScreen::open();
    VLOG(DebugCat::UI, "Furnace screen opened");
}

void FurnaceScreen::close(Inventory& inventory) {
    // 熔炉内容保留在FurnaceManager中（持久化），只归还光标物品
    if (!cursorItem_.isEmpty()) {
        uint16_t left = inventory.addItem(cursorItem_);
        if (left > 0) {
            VLOG(DebugCat::UI, "furnace close: %u items lost (inventory full)", left);
        }
        cursorItem_.clear();
    }
    furnaceData_ = nullptr;
    open_ = false;
    VLOG(DebugCat::UI, "Furnace screen closed");
}

ItemStack* FurnaceScreen::getContainerSlot(int index) {
    if (!furnaceData_) return nullptr;
    switch (index) {
        case SLOT_INPUT:  return &furnaceData_->inputSlot;
        case SLOT_FUEL:   return &furnaceData_->fuelSlot;
        case SLOT_OUTPUT: return &furnaceData_->outputSlot;
        default: return nullptr;
    }
}

const ItemStack* FurnaceScreen::getContainerSlot(int index) const {
    if (!furnaceData_) return nullptr;
    switch (index) {
        case SLOT_INPUT:  return &furnaceData_->inputSlot;
        case SLOT_FUEL:   return &furnaceData_->fuelSlot;
        case SLOT_OUTPUT: return &furnaceData_->outputSlot;
        default: return nullptr;
    }
}

// ============================================================
// Layout
// ============================================================

ContainerScreen::PanelLayout FurnaceScreen::computeLayout(float screenW, float screenH) const {
    PanelLayout L{};
    L.scale = getGuiScale(screenH);
    float s = static_cast<float>(L.scale);

    float slot = SLOT_SIZE * s;
    float gap  = SLOT_GAP * s;
    float pad  = PANEL_PAD * s;
    float secGap = SECTION_GAP * s;

    float gridW = 9 * slot + 8 * gap;
    L.panelW = gridW + pad * 2;

    // 熔炉区域高度：输入槽 + 火焰行 + 燃料槽 = 3行
    float furnaceAreaH = 3 * slot + 2 * gap;
    float mainAreaH = 3 * slot + 2 * gap;

    L.panelH = pad + furnaceAreaH + secGap + mainAreaH + secGap + slot + pad;

    L.panelX = (screenW - L.panelW) * 0.5f;
    L.panelY = (screenH - L.panelH) * 0.5f;

    // 熔炉区域基准点（输入槽位置）
    float furnaceLeft = L.panelX + pad;
    L.containerX = furnaceLeft + slot + gap;  // 输入槽X
    L.containerY = L.panelY + pad;            // 输入槽Y

    // 主背包在熔炉区域下方
    L.mainY = L.containerY + furnaceAreaH + secGap;

    // 快捷栏在最底部
    L.hotbarY = L.mainY + mainAreaH + secGap;

    // 不使用合成格
    L.craftX = 0;
    L.craftY = 0;
    L.outputX = 0;
    L.outputY = 0;

    return L;
}

FurnaceScreen::FurnaceLayout FurnaceScreen::computeFurnaceLayout(const PanelLayout& L) const {
    float s = static_cast<float>(L.scale);
    float slot = SLOT_SIZE * s;
    float gap  = SLOT_GAP * s;

    FurnaceLayout fl{};
    fl.slot = slot;

    // 输入槽：左上
    fl.inputX = L.containerX;
    fl.inputY = L.containerY;

    // 燃料槽：输入槽正下方（隔一行火焰）
    fl.fuelX = fl.inputX;
    fl.fuelY = fl.inputY + 2 * (slot + gap);

    // 输出槽：右侧，垂直居中于输入和燃料之间
    fl.outputX = fl.inputX + 3 * (slot + gap);
    fl.outputY = fl.inputY + (slot + gap) * 0.5f;

    // 火焰图标：输入槽和燃料槽之间居中
    fl.flameSize = slot * 0.8f;
    fl.flameX = fl.inputX + (slot - fl.flameSize) * 0.5f;
    fl.flameY = fl.inputY + slot + gap + (slot - fl.flameSize) * 0.5f;

    // 箭头图标：输入槽右侧
    fl.arrowW = slot * 1.5f;
    fl.arrowH = slot * 0.7f;
    fl.arrowX = fl.inputX + slot + gap * 2;
    fl.arrowY = fl.inputY + (slot - fl.arrowH) * 0.5f + (slot + gap) * 0.5f;

    return fl;
}

ContainerScreen::SlotRect FurnaceScreen::getContainerSlotRect(
        const PanelLayout& L, int index) const {
    FurnaceLayout fl = computeFurnaceLayout(L);
    SlotRect r{};
    r.w = fl.slot;
    r.h = fl.slot;
    switch (index) {
        case SLOT_INPUT:
            r.x = fl.inputX;
            r.y = fl.inputY;
            break;
        case SLOT_FUEL:
            r.x = fl.fuelX;
            r.y = fl.fuelY;
            break;
        case SLOT_OUTPUT:
            r.x = fl.outputX;
            r.y = fl.outputY;
            break;
        default:
            break;
    }
    return r;
}

// ============================================================
// Drawing
// ============================================================

void FurnaceScreen::drawFlameProgress(float x, float y, float size, float progress) {
    if (!ui_ || !atlas_) return;

    // 灰色背景火焰
    uint16_t bgTile = atlas_->getTileIndex("furnace_flame_bg");
    glm::vec4 bgUV = atlas_->getTileUV(bgTile);
    ui_->drawTexturedRect(x, y, size, size, bgUV.x, bgUV.y, bgUV.z, bgUV.w);

    // 亮色火焰（从下往上裁剪，MC原版行为）
    if (progress > 0.0f) {
        uint16_t fgTile = atlas_->getTileIndex("furnace_flame");
        glm::vec4 fgUV = atlas_->getTileUV(fgTile);

        float clipH = size * progress;
        float clipY = y + size - clipH;

        float uvTop = fgUV.y;
        float uvBot = fgUV.w;
        float uvClipTop = uvBot - (uvBot - uvTop) * progress;

        ui_->drawTexturedRect(x, clipY, size, clipH,
                              fgUV.x, uvClipTop, fgUV.z, uvBot);
    }
}

void FurnaceScreen::drawArrowProgress(float x, float y, float w, float h, float progress) {
    if (!ui_ || !atlas_) return;

    // 灰色背景箭头
    uint16_t bgTile = atlas_->getTileIndex("furnace_arrow_bg");
    glm::vec4 bgUV = atlas_->getTileUV(bgTile);
    ui_->drawTexturedRect(x, y, w, h, bgUV.x, bgUV.y, bgUV.z, bgUV.w);

    // 白色箭头（从左往右裁剪，MC原版行为）
    if (progress > 0.0f) {
        uint16_t fgTile = atlas_->getTileIndex("furnace_arrow");
        glm::vec4 fgUV = atlas_->getTileUV(fgTile);

        float clipW = w * progress;
        float uvLeft = fgUV.x;
        float uvRight = fgUV.z;
        float uvClipRight = uvLeft + (uvRight - uvLeft) * progress;

        ui_->drawTexturedRect(x, y, clipW, h,
                              uvLeft, fgUV.y, uvClipRight, fgUV.w);
    }
}

void FurnaceScreen::drawOutputSlotAt(float x, float y, float size, int scale,
                                     const ItemStack& stack, bool highlight) {
    float borderPx = BORDER * scale;
    float s = static_cast<float>(scale);

    // 金色背景
    glm::vec4 outputBg = highlight
        ? glm::vec4(0.65f, 0.62f, 0.50f, 0.92f)
        : glm::vec4(0.50f, 0.48f, 0.40f, 0.90f);
    ui_->drawRect(x, y, size, size, outputBg);

    // 金色边框
    glm::vec4 gold(0.85f, 0.75f, 0.30f, 0.90f);
    ui_->drawRect(x, y, size, borderPx, gold);
    ui_->drawRect(x, y + size - borderPx, size, borderPx, gold);
    ui_->drawRect(x, y, borderPx, size, gold);
    ui_->drawRect(x + size - borderPx, y, borderPx, size, gold);

    if (!stack.isEmpty()) {
        const auto& props = ItemRegistry::instance().get(stack.id);
        float iconPadPx = ICON_PAD * s;
        float ix = x + iconPadPx, iy = y + iconPadPx;
        float iSz = size - iconPadPx * 2;
        bool use3DCube = props.type == ItemType::Block && props.blockId > 0
            && BlockRegistry::instance().get(props.blockId).renderType != BlockRenderType::Cross;
        if (use3DCube)
            blockModel_->enqueueBlockIcon(*ui_, props.blockId, *atlas_, ix, iy, iSz);
        else if (!props.iconTileName.empty()) {
            uint16_t tile = atlas_->getTileIndex(props.iconTileName);
            glm::vec4 uv = atlas_->getTileUV(tile);
            ui_->drawTexturedRect(ix, iy, iSz, iSz, uv.x, uv.y, uv.z, uv.w);
        }
        if (stack.count > 1) {
            float glyphH = size * 0.35f;
            ui_->drawNumber(static_cast<int>(stack.count),
                            x + size - borderPx, y + size - glyphH - borderPx, glyphH);
        }
    }
}

void FurnaceScreen::drawContainerSlots(const PanelLayout& L, const SlotHit& hover) {
    if (!furnaceData_) return;

    FurnaceLayout fl = computeFurnaceLayout(L);

    // 火焰进度
    drawFlameProgress(fl.flameX, fl.flameY, fl.flameSize,
                      furnaceData_->fuelProgressRatio());

    // 箭头进度
    drawArrowProgress(fl.arrowX, fl.arrowY, fl.arrowW, fl.arrowH,
                      furnaceData_->smeltProgressRatio());

    // 输入槽
    drawSlot(fl.inputX, fl.inputY, fl.slot, L.scale,
             furnaceData_->inputSlot,
             hover.type == SlotType::Container && hover.index == SLOT_INPUT);

    // 燃料槽
    drawSlot(fl.fuelX, fl.fuelY, fl.slot, L.scale,
             furnaceData_->fuelSlot,
             hover.type == SlotType::Container && hover.index == SLOT_FUEL);

    // 输出槽（金色边框）
    drawOutputSlotAt(fl.outputX, fl.outputY, fl.slot, L.scale,
                     furnaceData_->outputSlot,
                     hover.type == SlotType::Container && hover.index == SLOT_OUTPUT);
}

void FurnaceScreen::draw(float screenW, float screenH, const Inventory& inventory) {
    // 调用基类draw（绘制面板、背包、快捷栏、Container槽位、光标物品）
    ContainerScreen::draw(screenW, screenH, inventory);
}

// ============================================================
// Shift+Click — 熔炉特有逻辑
// ============================================================

void FurnaceScreen::handleShiftClick(Inventory& inventory, SlotHit hit) {
    if (!furnaceData_) {
        ContainerScreen::handleShiftClick(inventory, hit);
        return;
    }

    ItemStack* source = nullptr;

    switch (hit.type) {
        case SlotType::Hotbar:
            source = &inventory.getSlot(hit.index);
            break;
        case SlotType::Main:
            source = &inventory.getSlot(hit.index + 9);
            break;
        case SlotType::Container:
            source = getContainerSlot(hit.index);
            break;
        default:
            return;
    }

    if (!source || source->isEmpty()) return;

    const auto& smeltReg = SmeltingRegistry::instance();

    if (hit.type == SlotType::Container) {
        // 熔炉槽位 → 背包（所有3个槽位都转移到背包）
        const auto& props = ItemRegistry::instance().get(source->id);
        // 先合并到已有堆叠
        for (int i = 0; i < Inventory::TOTAL_SLOTS && source->count > 0; ++i) {
            ItemStack& dst = inventory.getSlot(i);
            if (!dst.isEmpty() && dst.id == source->id && dst.count < props.maxStackSize) {
                uint16_t space = props.maxStackSize - dst.count;
                uint16_t toMove = std::min(space, source->count);
                dst.count += toMove;
                source->count -= toMove;
            }
        }
        // 再放入空格
        for (int i = 0; i < Inventory::TOTAL_SLOTS && source->count > 0; ++i) {
            ItemStack& dst = inventory.getSlot(i);
            if (dst.isEmpty()) {
                dst = *source;
                source->clear();
            }
        }
        if (source->count == 0) source->clear();
    } else {
        // 背包 → 熔炉：根据物品类型决定目标槽位
        // 1) 如果是燃料 → 优先放燃料槽
        // 2) 如果有冶炼配方 → 放输入槽
        // 3) 如果既是燃料又有配方 → 优先输入槽（MC原版行为）

        bool hasRecipe = (smeltReg.findRecipe(source->id) != nullptr);
        bool isFuel = smeltReg.isFuel(source->id);

        ItemStack* targetSlot = nullptr;

        if (hasRecipe) {
            // 优先放输入槽
            targetSlot = &furnaceData_->inputSlot;
        } else if (isFuel) {
            // 只是燃料，放燃料槽
            targetSlot = &furnaceData_->fuelSlot;
        }

        if (targetSlot) {
            if (targetSlot->isEmpty()) {
                *targetSlot = *source;
                source->clear();
            } else if (targetSlot->id == source->id) {
                const auto& props = ItemRegistry::instance().get(source->id);
                uint16_t space = props.maxStackSize - targetSlot->count;
                uint16_t toMove = std::min(space, source->count);
                if (toMove > 0) {
                    targetSlot->count += toMove;
                    source->count -= toMove;
                    if (source->count == 0) source->clear();
                }
            }
            // 如果输入槽满了且物品也是燃料，尝试放燃料槽
            if (hasRecipe && isFuel && source->count > 0) {
                ItemStack* fuelTarget = &furnaceData_->fuelSlot;
                if (fuelTarget->isEmpty()) {
                    *fuelTarget = *source;
                    source->clear();
                } else if (fuelTarget->id == source->id) {
                    const auto& props = ItemRegistry::instance().get(source->id);
                    uint16_t space = props.maxStackSize - fuelTarget->count;
                    uint16_t toMove = std::min(space, source->count);
                    if (toMove > 0) {
                        fuelTarget->count += toMove;
                        source->count -= toMove;
                        if (source->count == 0) source->clear();
                    }
                }
            }
        }
    }
}
