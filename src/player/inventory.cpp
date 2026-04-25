#include "inventory.h"
#include "core/serialization.h"

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

uint16_t Inventory::countItem(ItemId id) const {
    uint16_t total = 0;
    for (const auto& s : slots_) {
        if (s.id == id) total += s.count;
    }
    return total;
}

uint16_t Inventory::consumeItem(ItemId id, uint16_t count) {
    uint16_t removed = 0;
    for (auto& s : slots_) {
        if (s.id != id) continue;
        uint16_t take = std::min(s.count, static_cast<uint16_t>(count - removed));
        s.count -= take;
        if (s.count == 0) s.clear();
        removed += take;
        if (removed >= count) break;
    }
    return removed;
}

void Inventory::serialize(BinaryWriter& w) const {
    w.writeU8(static_cast<uint8_t>(selectedSlot_));
    for (int i = 0; i < TOTAL_SLOTS; ++i) {
        const auto& s = slots_[i];
        w.writeU16(s.id);
        w.writeU16(s.count);
        w.writeU16(s.durability);
    }
}

void Inventory::deserialize(BinaryReader& r) {
    selectedSlot_ = static_cast<int>(r.readU8()) % HOTBAR_SIZE;
    for (int i = 0; i < TOTAL_SLOTS; ++i) {
        auto& s = slots_[i];
        s.id         = r.readU16();
        s.count      = r.readU16();
        s.durability = r.readU16();
    }
}
