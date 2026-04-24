#pragma once

#include "core/item.h"
#include <array>

// Player inventory: 36 slots (9 hotbar + 27 main) + 4 armor + 1 offhand
class Inventory {
public:
    static constexpr int HOTBAR_SIZE = 9;
    static constexpr int MAIN_SIZE = 27;
    static constexpr int TOTAL_SLOTS = HOTBAR_SIZE + MAIN_SIZE;  // 36
    static constexpr int ARMOR_SLOTS = 4;

    // Hotbar = slots [0..8], Main = slots [9..35]
    ItemStack& getSlot(int index) { return slots_[index]; }
    const ItemStack& getSlot(int index) const { return slots_[index]; }

    // Hotbar selection
    int getSelectedSlot() const { return selectedSlot_; }
    void setSelectedSlot(int slot) { selectedSlot_ = slot % HOTBAR_SIZE; }

    // Get the item in hand (hotbar selected slot)
    ItemStack& getHeldItem() { return slots_[selectedSlot_]; }
    const ItemStack& getHeldItem() const { return slots_[selectedSlot_]; }

    // Try to add an item stack to inventory (first merge into existing, then empty slots)
    // Returns leftover count that couldn't be added.
    uint16_t addItem(ItemStack stack);

    // Remove one item from the held slot (for placing blocks, consuming food, etc.)
    // Returns true if an item was consumed.
    bool consumeHeldItem(uint16_t count = 1);

    // Clear all slots
    void clear();

private:
    std::array<ItemStack, TOTAL_SLOTS> slots_{};
    int selectedSlot_ = 0;
};
