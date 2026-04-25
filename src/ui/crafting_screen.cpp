#include "crafting_screen.h"

ContainerScreen::PanelLayout CraftingScreen::computeLayout(float screenW, float screenH) const {
    PanelLayout L{};
    L.scale = getGuiScale(screenH);
    float s = static_cast<float>(L.scale);

    float slot = SLOT_SIZE * s;
    float gap  = SLOT_GAP * s;
    float pad  = PANEL_PAD * s;
    float secGap = SECTION_GAP * s;

    float gridW = 9 * slot + 8 * gap;
    L.panelW = gridW + pad * 2;

    float craftAreaH = 3 * slot + 2 * gap;  // 3×3 grid
    L.panelH = pad + slot + secGap + 3 * slot + 2 * gap + secGap + craftAreaH + pad;

    L.panelX = (screenW - L.panelW) * 0.5f;
    L.panelY = (screenH - L.panelH) * 0.5f;

    L.hotbarY = L.panelY + L.panelH - pad - slot;
    L.mainY   = L.hotbarY - secGap - 3 * slot - 2 * gap;
    L.craftY  = L.panelY + pad;

    float craftGridW = 3 * slot + 2 * gap;
    float outputZone = secGap + slot;
    float craftTotalW = craftGridW + outputZone;
    L.craftX = L.panelX + L.panelW - pad - craftTotalW;
    L.outputX = L.craftX + craftGridW + secGap;
    L.outputY = L.craftY + (craftAreaH - slot) * 0.5f;

    return L;
}
