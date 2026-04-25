#include "container_screen.h"
#include "core/item.h"
#include "core/block.h"
#include "core/debug.h"
#include <algorithm>
#include <cmath>

// ============================================================
// Init / Open / Close
// ============================================================

void ContainerScreen::init(UIRenderer* ui, BlockModelRenderer* blockModel,
                           TextureAtlas* atlas, VulkanEngine* engine) {
    ui_ = ui;
    blockModel_ = blockModel;
    atlas_ = atlas;
    engine_ = engine;
}

void ContainerScreen::open() {
    auto [gw, gh] = getCraftGridDims();
    craftGrid_.resize(gw * gh);
    for (auto& s : craftGrid_) s.clear();
    craftOutput_.clear();
    open_ = true;
    VLOG(DebugCat::UI, "screen opened (craft grid %dx%d)", gw, gh);
}

void ContainerScreen::close(Inventory& inventory) {
    returnCraftItems(inventory);
    if (!cursorItem_.isEmpty()) {
        uint16_t left = inventory.addItem(cursorItem_);
        if (left > 0) {
            VLOG(DebugCat::UI, "screen close: %u items lost", left);
        }
        cursorItem_.clear();
    }
    craftOutput_.clear();
    open_ = false;
    VLOG(DebugCat::UI, "screen closed");
}

void ContainerScreen::returnCraftItems(Inventory& inventory) {
    for (auto& slot : craftGrid_) {
        if (!slot.isEmpty()) {
            uint16_t left = inventory.addItem(slot);
            if (left > 0) {
                VLOG(DebugCat::UI, "craft grid return: %u items lost", left);
            }
            slot.clear();
        }
    }
    craftOutput_.clear();
}

// ============================================================
// GUI Scale
// ============================================================

int ContainerScreen::getGuiScale(float screenH) {
    int s = static_cast<int>(screenH / 240.0f);
    return std::clamp(s, 2, 4);
}

// ============================================================
// Slot geometry
// ============================================================

ContainerScreen::SlotRect ContainerScreen::getSlotRect(
        const PanelLayout& L, SlotType type, int index) const {
    float s = static_cast<float>(L.scale);
    float slot = SLOT_SIZE * s;
    float gap  = SLOT_GAP * s;
    float innerX = L.panelX + PANEL_PAD * s;

    SlotRect r{};
    r.w = slot;
    r.h = slot;

    switch (type) {
        case SlotType::Hotbar:
            r.x = innerX + index * (slot + gap);
            r.y = L.hotbarY;
            break;
        case SlotType::Main: {
            int col = index % 9;
            int row = index / 9;
            r.x = innerX + col * (slot + gap);
            r.y = L.mainY + row * (slot + gap);
            break;
        }
        case SlotType::CraftInput:
            return getCraftSlotRect(L, index);
        case SlotType::CraftOutput:
            r.x = L.outputX;
            r.y = L.outputY;
            break;
        case SlotType::Container:
            return getContainerSlotRect(L, index);
        default:
            break;
    }
    return r;
}

ContainerScreen::SlotRect ContainerScreen::getContainerSlotRect(
        const PanelLayout& L, int index) const {
    float s = static_cast<float>(L.scale);
    float slot = SLOT_SIZE * s;
    float gap  = SLOT_GAP * s;
    auto [cols, rows] = getContainerGridDims();
    int col = index % cols;
    int row = index / cols;
    return {L.containerX + col * (slot + gap), L.containerY + row * (slot + gap), slot, slot};
}

ContainerScreen::SlotRect ContainerScreen::getCraftSlotRect(
        const PanelLayout& L, int index) const {
    float s = static_cast<float>(L.scale);
    float slot = SLOT_SIZE * s;
    float gap  = SLOT_GAP * s;
    auto [gw, gh] = getCraftGridDims();
    int col = index % gw;
    int row = index / gw;
    return {L.craftX + col * (slot + gap), L.craftY + row * (slot + gap), slot, slot};
}

ContainerScreen::SlotHit ContainerScreen::hitTest(
        const PanelLayout& L, double mx, double my) const {
    auto check = [&](SlotType type, int count) -> SlotHit {
        for (int i = 0; i < count; ++i) {
            SlotRect r = getSlotRect(L, type, i);
            if (mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h)
                return {type, i};
        }
        return {};
    };

    SlotHit h;
    h = check(SlotType::Hotbar, 9);
    if (h.type != SlotType::None) return h;
    h = check(SlotType::Main, 27);
    if (h.type != SlotType::None) return h;

    int gridSize = static_cast<int>(craftGrid_.size());
    if (gridSize > 0) {
        h = check(SlotType::CraftInput, gridSize);
        if (h.type != SlotType::None) return h;
        h = check(SlotType::CraftOutput, 1);
        if (h.type != SlotType::None) return h;
    }

    int containerCount = getContainerSlotCount();
    if (containerCount > 0) {
        h = check(SlotType::Container, containerCount);
        if (h.type != SlotType::None) return h;
    }
    return {};
}

// ============================================================
// Crafting
// ============================================================

void ContainerScreen::updateCraftOutput() {
    auto [gw, gh] = getCraftGridDims();
    if (gw == 0 || gh == 0) { craftOutput_.clear(); return; }

    std::vector<ItemId> grid(gw * gh);
    for (int i = 0; i < gw * gh; ++i)
        grid[i] = craftGrid_[i].isEmpty() ? Item::None : craftGrid_[i].id;

    const Recipe* r = RecipeRegistry::instance().findMatch(grid.data(), gw, gh);
    craftOutput_ = r ? r->output : ItemStack{};
}

// ============================================================
// Input
// ============================================================

void ContainerScreen::handleInput(Inventory& inventory, float screenW, float screenH,
                                  double mx, double my,
                                  bool leftClick, bool rightClick, bool rightHeld,
                                  bool shiftHeld, bool sortPressed) {
    mouseX_ = mx;
    mouseY_ = my;

    // R key: sort inventory (and container if present)
    if (sortPressed) {
        sortInventory(inventory);
        return;
    }

    // Right-click drag: while holding right button with cursor item, place 1 into
    // each new slot the mouse passes over (MC behavior).
    if (rightHeld && !rightClick && !cursorItem_.isEmpty()) {
        PanelLayout L = computeLayout(screenW, screenH);
        SlotHit hit = hitTest(L, mx, my);
        if (hit.type != SlotType::None && hit.type != SlotType::CraftOutput &&
            !(hit.type == lastRightDragSlot_.type && hit.index == lastRightDragSlot_.index)) {
            // New slot — try to place 1 item
            ItemStack* target = nullptr;
            switch (hit.type) {
                case SlotType::Hotbar:     target = &inventory.getSlot(hit.index); break;
                case SlotType::Main:       target = &inventory.getSlot(hit.index + 9); break;
                case SlotType::CraftInput: target = &craftGrid_[hit.index]; break;
                case SlotType::Container:  target = getContainerSlot(hit.index); break;
                default: break;
            }
            if (target) {
                // 输出槽不允许放入物品
                if (hit.type == SlotType::Container && isContainerSlotOutputOnly(hit.index)) {
                    lastRightDragSlot_ = hit;
                    return;
                }
                bool placed = false;
                if (target->isEmpty()) {
                    *target = cursorItem_;
                    target->count = 1;
                    placed = true;
                } else if (target->id == cursorItem_.id) {
                    const auto& props = ItemRegistry::instance().get(target->id);
                    if (target->count < props.maxStackSize) {
                        target->count += 1;
                        placed = true;
                    }
                }
                if (placed) {
                    cursorItem_.count -= 1;
                    if (cursorItem_.count == 0) cursorItem_.clear();
                    if (hit.type == SlotType::CraftInput) updateCraftOutput();
                }
            }
            lastRightDragSlot_ = hit;
        }
        return;
    }

    // Reset drag tracking when right button is released
    if (!rightHeld && !rightClick) {
        lastRightDragSlot_ = {};
    }

    if (!leftClick && !rightClick) return;

    // On right click press, start tracking for drag
    if (rightClick) {
        lastRightDragSlot_ = {};
    }

    PanelLayout L = computeLayout(screenW, screenH);
    SlotHit hit = hitTest(L, mx, my);
    if (hit.type == SlotType::None) return;

    handleSlotClick(inventory, hit, rightClick, shiftHeld);

    // Record this slot as the starting point for right-click drag
    if (rightClick) {
        lastRightDragSlot_ = hit;
    }
}

void ContainerScreen::handleSlotClick(Inventory& inventory, SlotHit hit, bool rightClick,
                                      bool shiftHeld) {
    // Shift+左键: 快速转移物品
    if (shiftHeld && !rightClick) {
        handleShiftClick(inventory, hit);
        return;
    }

    ItemStack* target = nullptr;

    switch (hit.type) {
        case SlotType::Hotbar:
            target = &inventory.getSlot(hit.index);
            break;
        case SlotType::Main:
            target = &inventory.getSlot(hit.index + 9);
            break;
        case SlotType::CraftInput:
            target = &craftGrid_[hit.index];
            break;
        case SlotType::Container:
            target = getContainerSlot(hit.index);
            // 输出槽只能取出，不能放入
            if (isContainerSlotOutputOnly(hit.index)) {
                if (!target || target->isEmpty()) return;
                // 只允许取出操作（类似CraftOutput逻辑）
                if (cursorItem_.isEmpty()) {
                    cursorItem_ = *target;
                    target->clear();
                } else if (cursorItem_.id == target->id) {
                    const auto& props = ItemRegistry::instance().get(cursorItem_.id);
                    uint16_t space = props.maxStackSize - cursorItem_.count;
                    uint16_t toMove = std::min(space, target->count);
                    if (toMove > 0) {
                        cursorItem_.count += toMove;
                        target->count -= toMove;
                        if (target->count == 0) target->clear();
                    }
                }
                return;
            }
            break;
        case SlotType::CraftOutput:
            if (!craftOutput_.isEmpty()) {
                if (cursorItem_.isEmpty()) {
                    cursorItem_ = craftOutput_;
                } else if (cursorItem_.id == craftOutput_.id) {
                    const auto& props = ItemRegistry::instance().get(cursorItem_.id);
                    if (cursorItem_.count + craftOutput_.count <= props.maxStackSize)
                        cursorItem_.count += craftOutput_.count;
                    else return;
                } else return;
                for (auto& slot : craftGrid_) {
                    if (!slot.isEmpty()) {
                        slot.count -= 1;
                        if (slot.count == 0) slot.clear();
                    }
                }
                updateCraftOutput();
                VLOG(DebugCat::UI, "crafted from grid");
            }
            return;
        default: return;
    }

    if (!target) return;

    if (!rightClick) {
        if (cursorItem_.isEmpty() && target->isEmpty()) return;
        if (cursorItem_.isEmpty()) {
            cursorItem_ = *target; target->clear();
        } else if (target->isEmpty()) {
            *target = cursorItem_; cursorItem_.clear();
        } else if (cursorItem_.id == target->id) {
            const auto& props = ItemRegistry::instance().get(cursorItem_.id);
            uint16_t space = props.maxStackSize - target->count;
            uint16_t toMove = std::min(space, cursorItem_.count);
            target->count += toMove;
            cursorItem_.count -= toMove;
            if (cursorItem_.count == 0) cursorItem_.clear();
        } else {
            std::swap(*target, cursorItem_);
        }
    } else {
        if (cursorItem_.isEmpty()) {
            if (target->isEmpty()) return;
            uint16_t half = (target->count + 1) / 2;
            cursorItem_ = *target;
            cursorItem_.count = half;
            target->count -= half;
            if (target->count == 0) target->clear();
        } else {
            if (target->isEmpty()) {
                *target = cursorItem_;
                target->count = 1;
                cursorItem_.count -= 1;
                if (cursorItem_.count == 0) cursorItem_.clear();
            } else if (target->id == cursorItem_.id) {
                const auto& props = ItemRegistry::instance().get(target->id);
                if (target->count < props.maxStackSize) {
                    target->count += 1;
                    cursorItem_.count -= 1;
                    if (cursorItem_.count == 0) cursorItem_.clear();
                }
            } else {
                std::swap(*target, cursorItem_);
            }
        }
    }

    if (hit.type == SlotType::CraftInput) updateCraftOutput();
}

// ============================================================
// Rendering — shared
// ============================================================

void ContainerScreen::drawSlot(float x, float y, float size, int scale,
                               const ItemStack& stack, bool highlight) {
    float borderPx = BORDER * scale;
    float iconPadPx = ICON_PAD * scale;

    glm::vec4 bgColor = highlight
        ? glm::vec4(0.65f, 0.65f, 0.68f, 0.92f)
        : glm::vec4(0.45f, 0.45f, 0.47f, 0.90f);
    ui_->drawRect(x, y, size, size, bgColor);

    glm::vec4 borderLight(0.60f, 0.60f, 0.62f, 0.75f);
    glm::vec4 borderDark(0.30f, 0.30f, 0.32f, 0.75f);
    ui_->drawRect(x, y, size, borderPx, borderLight);
    ui_->drawRect(x, y, borderPx, size, borderLight);
    ui_->drawRect(x, y + size - borderPx, size, borderPx, borderDark);
    ui_->drawRect(x + size - borderPx, y, borderPx, size, borderDark);

    if (highlight) {
        glm::vec4 hl(1.0f, 1.0f, 1.0f, 0.7f);
        ui_->drawRect(x, y, size, borderPx, hl);
        ui_->drawRect(x, y + size - borderPx, size, borderPx, hl);
        ui_->drawRect(x, y, borderPx, size, hl);
        ui_->drawRect(x + size - borderPx, y, borderPx, size, hl);
    }

    if (stack.isEmpty()) return;

    const auto& props = ItemRegistry::instance().get(stack.id);
    float ix = x + iconPadPx, iy = y + iconPadPx;
    float iSz = size - iconPadPx * 2;

    // Cross-type blocks (flowers, mushrooms) use 2D icon, not 3D cube.
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

    if (props.durability > 0 && stack.durability > 0) {
        float ratio = 1.0f - static_cast<float>(stack.durability) / props.durability;
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        float barH = 2.0f * scale, barPad = 2.0f * scale;
        float barW = size - barPad * 2;
        float bx = x + barPad, by = y + size - barH - barPad;
        ui_->drawRect(bx, by, barW, barH, glm::vec4(0, 0, 0, 0.8f));
        glm::vec4 col = ratio > 0.5f
            ? glm::vec4(1.0f - (ratio-0.5f)*2, 1, 0, 1)
            : glm::vec4(1, ratio*2, 0, 1);
        ui_->drawRect(bx, by, barW * ratio, barH, col);
    }
}

void ContainerScreen::drawOutputSlot(const PanelLayout& L, bool highlight) {
    SlotRect r = getSlotRect(L, SlotType::CraftOutput, 0);
    float borderPx = BORDER * L.scale;
    float s = static_cast<float>(L.scale);

    glm::vec4 outputBg = highlight
        ? glm::vec4(0.65f, 0.62f, 0.50f, 0.92f)
        : glm::vec4(0.50f, 0.48f, 0.40f, 0.90f);
    ui_->drawRect(r.x, r.y, r.w, r.h, outputBg);

    glm::vec4 gold(0.85f, 0.75f, 0.30f, 0.90f);
    ui_->drawRect(r.x, r.y, r.w, borderPx, gold);
    ui_->drawRect(r.x, r.y + r.h - borderPx, r.w, borderPx, gold);
    ui_->drawRect(r.x, r.y, borderPx, r.h, gold);
    ui_->drawRect(r.x + r.w - borderPx, r.y, borderPx, r.h, gold);

    if (!craftOutput_.isEmpty()) {
        const auto& props = ItemRegistry::instance().get(craftOutput_.id);
        float iconPadPx = ICON_PAD * s;
        float ix = r.x + iconPadPx, iy = r.y + iconPadPx;
        float iSz = r.w - iconPadPx * 2;
        bool use3DCube = props.type == ItemType::Block && props.blockId > 0
            && BlockRegistry::instance().get(props.blockId).renderType != BlockRenderType::Cross;
        if (use3DCube)
            blockModel_->enqueueBlockIcon(*ui_, props.blockId, *atlas_, ix, iy, iSz);
        else if (!props.iconTileName.empty()) {
            uint16_t tile = atlas_->getTileIndex(props.iconTileName);
            glm::vec4 uv = atlas_->getTileUV(tile);
            ui_->drawTexturedRect(ix, iy, iSz, iSz, uv.x, uv.y, uv.z, uv.w);
        }
        if (craftOutput_.count > 1) {
            float glyphH = r.w * 0.35f;
            ui_->drawNumber(static_cast<int>(craftOutput_.count),
                            r.x + r.w - borderPx, r.y + r.h - glyphH - borderPx, glyphH);
        }
    }
}

void ContainerScreen::drawContainerSlots(const PanelLayout& L, const SlotHit& hover) {
    int containerCount = getContainerSlotCount();
    for (int i = 0; i < containerCount; ++i) {
        SlotRect r = getContainerSlotRect(L, i);
        const ItemStack* cs = getContainerSlot(i);
        drawSlot(r.x, r.y, r.w, L.scale, cs ? *cs : ItemStack{},
                 hover.type == SlotType::Container && hover.index == i);
    }
}

void ContainerScreen::drawArrow(const PanelLayout& L) {
    auto [gw, gh] = getCraftGridDims();
    float s = static_cast<float>(L.scale);
    float slot = SLOT_SIZE * s;
    float gap = SLOT_GAP * s;

    float craftGridEnd = L.craftX + gw * slot + (gw - 1) * gap;
    float craftAreaH = gh * slot + (gh - 1) * gap;
    float arrowMidX = (craftGridEnd + L.outputX) * 0.5f;
    float arrowMidY = L.craftY + craftAreaH * 0.5f;

    float shaftW = (L.outputX - craftGridEnd) * 0.35f;
    float shaftH = 1.0f * s;
    glm::vec4 arrowCol(0.65f, 0.65f, 0.65f, 0.75f);
    ui_->drawRect(arrowMidX - shaftW * 0.5f, arrowMidY - shaftH * 0.5f,
                  shaftW, shaftH, arrowCol);
    float headX = arrowMidX + shaftW * 0.5f;
    float headW = 1.0f * s;
    ui_->drawRect(headX, arrowMidY - 2.5f * s, headW, 5.0f * s, arrowCol);
    ui_->drawRect(headX + headW, arrowMidY - 1.5f * s, headW, 3.0f * s, arrowCol);
    ui_->drawRect(headX + headW * 2, arrowMidY - 0.5f * s, headW, 1.0f * s, arrowCol);
}

void ContainerScreen::drawCursorItem(float slotSize, float scale) {
    if (cursorItem_.isEmpty()) return;
    float cx = static_cast<float>(mouseX_) - slotSize * 0.5f;
    float cy = static_cast<float>(mouseY_) - slotSize * 0.5f;
    const auto& props = ItemRegistry::instance().get(cursorItem_.id);
    float iconPadPx = ICON_PAD * scale;
    float ix = cx + iconPadPx, iy = cy + iconPadPx;
    float iSz = slotSize - iconPadPx * 2;
    glm::vec4 tint(1, 1, 1, 0.85f);
    bool use3DCube = props.type == ItemType::Block && props.blockId > 0
        && BlockRegistry::instance().get(props.blockId).renderType != BlockRenderType::Cross;
    if (use3DCube)
        blockModel_->enqueueBlockIcon(*ui_, props.blockId, *atlas_, ix, iy, iSz);
    else if (!props.iconTileName.empty()) {
        uint16_t tile = atlas_->getTileIndex(props.iconTileName);
        glm::vec4 uv = atlas_->getTileUV(tile);
        ui_->drawTexturedRect(ix, iy, iSz, iSz, uv.x, uv.y, uv.z, uv.w, tint);
    }
    if (cursorItem_.count > 1) {
        float glyphH = slotSize * 0.35f;
        ui_->drawNumber(static_cast<int>(cursorItem_.count),
                        cx + slotSize - BORDER * scale,
                        cy + slotSize - glyphH - BORDER * scale, glyphH);
    }
}

// ============================================================
// Main draw — shared frame (overlay + panel + inventory + craft + cursor)
// ============================================================

void ContainerScreen::draw(float screenW, float screenH, const Inventory& inventory) {
    if (!open_) return;

    PanelLayout L = computeLayout(screenW, screenH);
    float s = static_cast<float>(L.scale);
    float slot = SLOT_SIZE * s;

    // Dim overlay
    ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0, 0, 0, 0.40f));

    // Panel background
    ui_->drawRect(L.panelX, L.panelY, L.panelW, L.panelH,
                  glm::vec4(0.22f, 0.22f, 0.24f, 0.92f));

    // Panel border
    float bdr = BORDER * s;
    glm::vec4 bL(0.60f, 0.60f, 0.62f, 0.85f);
    glm::vec4 bD(0.35f, 0.35f, 0.37f, 0.85f);
    ui_->drawRect(L.panelX, L.panelY, L.panelW, bdr, bL);
    ui_->drawRect(L.panelX, L.panelY, bdr, L.panelH, bL);
    ui_->drawRect(L.panelX, L.panelY + L.panelH - bdr, L.panelW, bdr, bD);
    ui_->drawRect(L.panelX + L.panelW - bdr, L.panelY, bdr, L.panelH, bD);

    SlotHit hover = hitTest(L, mouseX_, mouseY_);

    // Hotbar
    for (int i = 0; i < 9; ++i) {
        SlotRect r = getSlotRect(L, SlotType::Hotbar, i);
        drawSlot(r.x, r.y, r.w, L.scale, inventory.getSlot(i),
                 hover.type == SlotType::Hotbar && hover.index == i);
    }

    // Main inventory
    for (int i = 0; i < 27; ++i) {
        SlotRect r = getSlotRect(L, SlotType::Main, i);
        drawSlot(r.x, r.y, r.w, L.scale, inventory.getSlot(i + 9),
                 hover.type == SlotType::Main && hover.index == i);
    }

    // Crafting grid
    int gridSize = static_cast<int>(craftGrid_.size());
    for (int i = 0; i < gridSize; ++i) {
        SlotRect r = getSlotRect(L, SlotType::CraftInput, i);
        drawSlot(r.x, r.y, r.w, L.scale, craftGrid_[i],
                 hover.type == SlotType::CraftInput && hover.index == i);
    }

    // Output slot + arrow (if crafting grid exists)
    if (gridSize > 0) {
        drawOutputSlot(L, hover.type == SlotType::CraftOutput && hover.index == 0);
        drawArrow(L);
    }

    // Container slots (chest, furnace, etc.) — delegated to virtual method
    drawContainerSlots(L, hover);

    // Separator between hotbar and main
    {
        float sepY = L.hotbarY - SECTION_GAP * s * 0.5f;
        float innerX = L.panelX + PANEL_PAD * s;
        float gridW = 9 * slot + 8 * SLOT_GAP * s;
        ui_->drawRect(innerX, sepY, gridW, 1.0f * s,
                      glm::vec4(0.35f, 0.35f, 0.35f, 0.5f));
    }

    // Cursor item (last = on top)
    drawCursorItem(slot, s);
}

// ============================================================
// Shift+Click — 快速转移物品
// ============================================================

void ContainerScreen::handleShiftClick(Inventory& inventory, SlotHit hit) {
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
        case SlotType::CraftInput:
            source = &craftGrid_[hit.index];
            break;
        case SlotType::CraftOutput:
            // Shift+点击合成输出：尝试将结果放入背包
            if (!craftOutput_.isEmpty()) {
                uint16_t left = inventory.addItem(craftOutput_);
                if (left == 0) {
                    // 成功放入，消耗合成材料
                    for (auto& slot : craftGrid_) {
                        if (!slot.isEmpty()) {
                            slot.count -= 1;
                            if (slot.count == 0) slot.clear();
                        }
                    }
                    updateCraftOutput();
                }
            }
            return;
        default:
            return;
    }

    if (!source || source->isEmpty()) return;

    int containerCount = getContainerSlotCount();

    // 判断来源区域，决定目标区域
    if (hit.type == SlotType::Container) {
        // 箱子 → 背包（优先合并到已有堆叠，然后放空格）
        const auto& props = ItemRegistry::instance().get(source->id);
        // 先尝试合并到背包已有同类物品
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
    } else if (hit.type == SlotType::Hotbar || hit.type == SlotType::Main ||
               hit.type == SlotType::CraftInput) {
        if (containerCount > 0) {
            // 有箱子打开时：背包/合成格 → 箱子
            const auto& props = ItemRegistry::instance().get(source->id);
            // 先合并到箱子已有同类
            for (int i = 0; i < containerCount && source->count > 0; ++i) {
                ItemStack* dst = getContainerSlot(i);
                if (dst && !dst->isEmpty() && dst->id == source->id &&
                    dst->count < props.maxStackSize) {
                    uint16_t space = props.maxStackSize - dst->count;
                    uint16_t toMove = std::min(space, source->count);
                    dst->count += toMove;
                    source->count -= toMove;
                }
            }
            // 再放入箱子空格
            for (int i = 0; i < containerCount && source->count > 0; ++i) {
                ItemStack* dst = getContainerSlot(i);
                if (dst && dst->isEmpty()) {
                    *dst = *source;
                    source->clear();
                }
            }
            if (source->count == 0) source->clear();
        } else {
            // 没有箱子（背包/工作台界面）：hotbar ↔ main 互转
            if (hit.type == SlotType::Hotbar) {
                // Hotbar → Main
                const auto& props = ItemRegistry::instance().get(source->id);
                for (int i = 9; i < Inventory::TOTAL_SLOTS && source->count > 0; ++i) {
                    ItemStack& dst = inventory.getSlot(i);
                    if (!dst.isEmpty() && dst.id == source->id &&
                        dst.count < props.maxStackSize) {
                        uint16_t space = props.maxStackSize - dst.count;
                        uint16_t toMove = std::min(space, source->count);
                        dst.count += toMove;
                        source->count -= toMove;
                    }
                }
                for (int i = 9; i < Inventory::TOTAL_SLOTS && source->count > 0; ++i) {
                    ItemStack& dst = inventory.getSlot(i);
                    if (dst.isEmpty()) {
                        dst = *source;
                        source->clear();
                    }
                }
                if (source->count == 0) source->clear();
            } else {
                // Main / CraftInput → Hotbar
                const auto& props = ItemRegistry::instance().get(source->id);
                for (int i = 0; i < Inventory::HOTBAR_SIZE && source->count > 0; ++i) {
                    ItemStack& dst = inventory.getSlot(i);
                    if (!dst.isEmpty() && dst.id == source->id &&
                        dst.count < props.maxStackSize) {
                        uint16_t space = props.maxStackSize - dst.count;
                        uint16_t toMove = std::min(space, source->count);
                        dst.count += toMove;
                        source->count -= toMove;
                    }
                }
                for (int i = 0; i < Inventory::HOTBAR_SIZE && source->count > 0; ++i) {
                    ItemStack& dst = inventory.getSlot(i);
                    if (dst.isEmpty()) {
                        dst = *source;
                        source->clear();
                    }
                }
                if (source->count == 0) source->clear();
            }
        }
    }

    if (hit.type == SlotType::CraftInput) updateCraftOutput();
}

// ============================================================
// Sort — R键整理背包
// ============================================================

int ContainerScreen::itemSortCategory(ItemId id) {
    if (id == Item::None) return 99;  // 空物品排最后
    const auto& props = ItemRegistry::instance().get(id);
    switch (props.type) {
        case ItemType::Block:    return 0;  // 方块优先
        case ItemType::Tool:     return 1;  // 工具
        case ItemType::Weapon:   return 2;  // 武器
        case ItemType::Food:     return 3;  // 食物
        case ItemType::Material: return 4;  // 杂物/材料
        default:                 return 5;
    }
}

void ContainerScreen::sortSlotRange(ItemStack* slots, int count) {
    // 第一步：合并同类物品（相同 id 的堆叠合并）
    for (int i = 0; i < count; ++i) {
        if (slots[i].isEmpty()) continue;
        const auto& props = ItemRegistry::instance().get(slots[i].id);
        for (int j = i + 1; j < count; ++j) {
            if (slots[j].isEmpty()) continue;
            if (slots[j].id == slots[i].id && props.maxStackSize > 1) {
                uint16_t space = props.maxStackSize - slots[i].count;
                uint16_t toMove = std::min(space, slots[j].count);
                if (toMove > 0) {
                    slots[i].count += toMove;
                    slots[j].count -= toMove;
                    if (slots[j].count == 0) slots[j].clear();
                }
                if (slots[i].count >= props.maxStackSize) break;
            }
        }
    }

    // 第二步：收集非空物品，保持原有出现顺序（stable）
    struct SortEntry {
        ItemStack stack;
        int originalIndex;  // 用于保持组内原有顺序
    };
    std::vector<SortEntry> items;
    items.reserve(count);
    for (int i = 0; i < count; ++i) {
        if (!slots[i].isEmpty()) {
            items.push_back({slots[i], i});
        }
    }

    // 第三步：稳定排序 — 按类别排序，组内保持原有顺序
    std::stable_sort(items.begin(), items.end(),
        [](const SortEntry& a, const SortEntry& b) {
            int catA = itemSortCategory(a.stack.id);
            int catB = itemSortCategory(b.stack.id);
            if (catA != catB) return catA < catB;
            // 同类别内，相同物品 id 聚在一起
            return a.stack.id < b.stack.id;
        });

    // 第四步：写回，紧凑填充到前面的格子
    for (int i = 0; i < count; ++i) {
        if (i < static_cast<int>(items.size())) {
            slots[i] = items[i].stack;
        } else {
            slots[i].clear();
        }
    }
}

void ContainerScreen::sortInventory(Inventory& inventory) {
    // 整理玩家背包（hotbar 0-8 + main 9-35，共36格一起排序）
    ItemStack tempSlots[Inventory::TOTAL_SLOTS];
    for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i) {
        tempSlots[i] = inventory.getSlot(i);
    }
    sortSlotRange(tempSlots, Inventory::TOTAL_SLOTS);
    for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i) {
        inventory.getSlot(i) = tempSlots[i];
    }

    // 如果有箱子打开，也整理箱子
    int containerCount = getContainerSlotCount();
    if (containerCount > 0) {
        std::vector<ItemStack> containerSlots(containerCount);
        for (int i = 0; i < containerCount; ++i) {
            const ItemStack* cs = getContainerSlot(i);
            containerSlots[i] = cs ? *cs : ItemStack{};
        }
        sortSlotRange(containerSlots.data(), containerCount);
        for (int i = 0; i < containerCount; ++i) {
            ItemStack* cs = getContainerSlot(i);
            if (cs) *cs = containerSlots[i];
        }
    }

    VLOG(DebugCat::UI, "inventory sorted");
}
