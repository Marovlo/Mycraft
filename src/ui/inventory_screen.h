#pragma once

#include "container_screen.h"

// Player inventory screen (E key): 36 slots + 2×2 crafting grid.
class InventoryScreen : public ContainerScreen {
protected:
    std::pair<int,int> getCraftGridDims() const override { return {2, 2}; }
    PanelLayout computeLayout(float screenW, float screenH) const override;
};
