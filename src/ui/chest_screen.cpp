#include "chest_screen.h"
#include "core/debug.h"

void ChestScreen::open() {
    // No crafting grid for chest — just call base to set open_ = true
    ContainerScreen::open();
    VLOG(DebugCat::UI, "Chest screen opened");
}

void ChestScreen::close(Inventory& inventory) {
    // Chest contents stay in ChestManager (persistent), only return cursor item
    if (!cursorItem_.isEmpty()) {
        uint16_t left = inventory.addItem(cursorItem_);
        if (left > 0) {
            VLOG(DebugCat::UI, "chest close: %u items lost (inventory full)", left);
        }
        cursorItem_.clear();
    }
    chestInv_ = nullptr;
    open_ = false;
    VLOG(DebugCat::UI, "Chest screen closed");
}

ContainerScreen::PanelLayout ChestScreen::computeLayout(float screenW, float screenH) const {
    PanelLayout L{};
    L.scale = getGuiScale(screenH);
    float s = static_cast<float>(L.scale);

    float slot = SLOT_SIZE * s;
    float gap  = SLOT_GAP * s;
    float pad  = PANEL_PAD * s;
    float secGap = SECTION_GAP * s;

    float gridW = 9 * slot + 8 * gap;
    L.panelW = gridW + pad * 2;

    // Layout from top to bottom:
    //   pad + chest label area (optional, skip for now)
    //   3 rows of chest slots
    //   secGap (separator)
    //   3 rows of main inventory
    //   secGap (separator)
    //   1 row of hotbar
    //   pad
    float chestAreaH = 3 * slot + 2 * gap;
    float mainAreaH  = 3 * slot + 2 * gap;
    L.panelH = pad + chestAreaH + secGap + mainAreaH + secGap + slot + pad;

    L.panelX = (screenW - L.panelW) * 0.5f;
    L.panelY = (screenH - L.panelH) * 0.5f;

    // Chest container slots at top
    L.containerX = L.panelX + pad;
    L.containerY = L.panelY + pad;

    // Main inventory below chest
    L.mainY = L.containerY + chestAreaH + secGap;

    // Hotbar at bottom
    L.hotbarY = L.mainY + mainAreaH + secGap;

    // No crafting grid
    L.craftX = 0;
    L.craftY = 0;
    L.outputX = 0;
    L.outputY = 0;

    return L;
}
