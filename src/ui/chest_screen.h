#pragma once

#include "container_screen.h"
#include "world/chest_manager.h"

// ChestScreen: 27-slot chest container GUI (3 rows × 9 columns).
// The chest inventory is stored externally in ChestManager; this screen
// holds a pointer to the active chest's inventory array.
class ChestScreen : public ContainerScreen {
public:
    // Set the chest inventory to display/edit. Must be called before open().
    void setChestInventory(ChestManager::ChestInventory* inv) { chestInv_ = inv; }

    void open() override;
    void close(Inventory& inventory) override;

protected:
    std::pair<int,int> getCraftGridDims() const override { return {0, 0}; }
    PanelLayout computeLayout(float screenW, float screenH) const override;

    int getContainerSlotCount() const override { return ChestManager::CHEST_SLOTS; }
    std::pair<int,int> getContainerGridDims() const override { return {9, 3}; }

    ItemStack* getContainerSlot(int index) override {
        if (!chestInv_ || index < 0 || index >= ChestManager::CHEST_SLOTS) return nullptr;
        return &(*chestInv_)[index];
    }
    const ItemStack* getContainerSlot(int index) const override {
        if (!chestInv_ || index < 0 || index >= ChestManager::CHEST_SLOTS) return nullptr;
        return &(*chestInv_)[index];
    }

private:
    ChestManager::ChestInventory* chestInv_ = nullptr;
};
