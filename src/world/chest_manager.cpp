#include "chest_manager.h"
#include "core/debug.h"

ChestManager::ChestInventory& ChestManager::getOrCreate(int x, int y, int z) {
    uint64_t key = packKey(x, y, z);
    auto it = chests_.find(key);
    if (it != chests_.end()) return it->second;

    // Insert a new empty chest
    auto& inv = chests_[key];
    for (auto& slot : inv) slot.clear();
    VLOG(DebugCat::Save, "Created chest inventory at (%d,%d,%d)", x, y, z);
    return inv;
}

ChestManager::ChestInventory* ChestManager::get(int x, int y, int z) {
    uint64_t key = packKey(x, y, z);
    auto it = chests_.find(key);
    return (it != chests_.end()) ? &it->second : nullptr;
}

const ChestManager::ChestInventory* ChestManager::get(int x, int y, int z) const {
    uint64_t key = packKey(x, y, z);
    auto it = chests_.find(key);
    return (it != chests_.end()) ? &it->second : nullptr;
}

ChestManager::ChestInventory ChestManager::remove(int x, int y, int z) {
    uint64_t key = packKey(x, y, z);
    auto it = chests_.find(key);
    if (it == chests_.end()) {
        ChestInventory empty;
        for (auto& s : empty) s.clear();
        return empty;
    }
    ChestInventory contents = it->second;
    chests_.erase(it);
    VLOG(DebugCat::Save, "Removed chest inventory at (%d,%d,%d)", x, y, z);
    return contents;
}

bool ChestManager::has(int x, int y, int z) const {
    return chests_.count(packKey(x, y, z)) > 0;
}

void ChestManager::serialize(BinaryWriter& w) const {
    w.writeU32(static_cast<uint32_t>(chests_.size()));

    for (const auto& [key, inv] : chests_) {
        // Unpack key back to coordinates
        int32_t x = static_cast<int32_t>(static_cast<uint32_t>(key & 0xFFFFFFFF));
        int32_t y = static_cast<int32_t>(static_cast<uint16_t>((key >> 32) & 0xFFFF));
        int32_t z = static_cast<int32_t>(static_cast<int16_t>((key >> 48) & 0xFFFF));

        w.writeI32(x);
        w.writeI32(y);
        w.writeI32(z);

        for (int i = 0; i < CHEST_SLOTS; ++i) {
            w.writeU16(inv[i].id);
            w.writeU16(inv[i].count);
            w.writeU16(inv[i].durability);
        }
    }
}

void ChestManager::deserialize(BinaryReader& r) {
    chests_.clear();

    uint32_t count = r.readU32();
    for (uint32_t c = 0; c < count && r.isValid(); ++c) {
        int32_t x = r.readI32();
        int32_t y = r.readI32();
        int32_t z = r.readI32();

        uint64_t key = packKey(x, y, z);
        ChestInventory& inv = chests_[key];

        for (int i = 0; i < CHEST_SLOTS; ++i) {
            inv[i].id         = r.readU16();
            inv[i].count      = r.readU16();
            inv[i].durability = r.readU16();
        }
    }

    VLOG(DebugCat::Save, "Loaded %u chest inventories", count);
}
