#include "inventory_screen.h"

ContainerScreen::PanelLayout InventoryScreen::computeLayout(float screenW, float screenH) const {
    PanelLayout L{};
    L.scale = getGuiScale(screenH);
    float s = static_cast<float>(L.scale);

    float slot = SLOT_SIZE * s;
    float gap  = SLOT_GAP * s;
    float pad  = PANEL_PAD * s;
    float secGap = SECTION_GAP * s;

    float gridW = 9 * slot + 8 * gap;
    L.panelW = gridW + pad * 2;

    float craftAreaH = 2 * slot + gap;  // 2×2 grid
    L.panelH = pad + slot + secGap + 3 * slot + 2 * gap + secGap + craftAreaH + pad;

    L.panelX = (screenW - L.panelW) * 0.5f;
    L.panelY = (screenH - L.panelH) * 0.5f;

    L.hotbarY = L.panelY + L.panelH - pad - slot;
    L.mainY   = L.hotbarY - secGap - 3 * slot - 2 * gap;
    L.craftY  = L.panelY + pad;

    float craftTotalW = 2 * slot + gap + secGap + slot;
    L.craftX = L.panelX + L.panelW - pad - craftTotalW;
    L.outputX = L.craftX + 2 * slot + gap + secGap;
    L.outputY = L.craftY + (2 * slot + gap - slot) * 0.5f;

    return L;
}
