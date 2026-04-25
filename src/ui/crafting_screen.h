#pragma once

#include "container_screen.h"

// Crafting table screen (right-click crafting table): 36 slots + 3×3 grid.
class CraftingScreen : public ContainerScreen {
protected:
    std::pair<int,int> getCraftGridDims() const override { return {3, 3}; }
    PanelLayout computeLayout(float screenW, float screenH) const override;
};
