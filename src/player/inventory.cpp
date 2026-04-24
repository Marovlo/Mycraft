#include "inventory.h"

uint16_t Inventory::addItem(ItemStack stack) {
    if (stack.isEmpty()) return 0;

    const auto& props = ItemRegistry::instance().get(stack.id);
    uint16_t maxStack = props.maxStackSize;

    // Phase 1: try to merge into existing stacks of the same type
    for (int i = 0; i < TOTAL_SLOTS && stack.count > 0; i++) {
        if (slots_[i].id == stack.id && slots_[i].count < maxStack) {
            uint16_t space = maxStack - slots_[i].count;
            uint16_t toAdd = std::min(space, stack.count);
            slots_[i].count += toAdd;
            stack.count -= toAdd;
        }
    }

    // Phase 2: place into empty slots
    for (int i = 0; i < TOTAL_SLOTS && stack.count > 0; i++) {
        if (slots_[i].isEmpty()) {
            slots_[i] = stack;
            if (slots_[i].count > maxStack) {
                stack.count = slots_[i].count - maxStack;
                slots_[i].count = maxStack;
            } else {
                stack.count = 0;
            }
        }
    }

    return stack.count;  // leftover
}

bool Inventory::consumeHeldItem(uint16_t count) {
    auto& held = slots_[selectedSlot_];
    if (held.isEmpty() || held.count < count) return false;
    held.count -= count;
    if (held.count == 0) held.clear();
    return true;
}

void Inventory::clear() {
    for (auto& slot : slots_) slot.clear();
}
