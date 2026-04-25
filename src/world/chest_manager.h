#pragma once

#include "core/item.h"
#include "core/serialization.h"

#include <array>
#include <unordered_map>
#include <vector>

// ========== ChestManager ==========
// Manages per-block-position chest inventories. Each chest stores 27 ItemStacks
// (3 rows × 9 columns), matching Minecraft's single chest layout.
//
// Keyed by packed world coordinates (int x, y, z → uint64_t).
// Thread-safety: single-threaded (same as all game logic).

class ChestManager {
public:
    static constexpr int CHEST_SLOTS = 27;
    using ChestInventory = std::array<ItemStack, CHEST_SLOTS>;

    // Get or create a chest inventory at the given world position.
    ChestInventory& getOrCreate(int x, int y, int z);

    // Get a chest inventory (returns nullptr if none exists at this position).
    ChestInventory* get(int x, int y, int z);
    const ChestInventory* get(int x, int y, int z) const;

    // Remove a chest inventory (called when chest block is broken).
    // Returns the contents before removal (for dropping items).
    ChestInventory remove(int x, int y, int z);

    // Check if a chest exists at the given position.
    bool has(int x, int y, int z) const;

    // Serialization
    void serialize(BinaryWriter& w) const;
    void deserialize(BinaryReader& r);

    // Clear all data (e.g., when loading a new world)
    void clear() { chests_.clear(); }

    size_t count() const { return chests_.size(); }

private:
    // Pack 3 ints into a single 64-bit key for fast hashing.
    // x, y, z are world coordinates. y is 0-255, x/z can be large.
    static uint64_t packKey(int x, int y, int z) {
        // Use bit manipulation: store as three 21-bit signed values
        // This supports coordinates up to ±1M which is more than enough.
        uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(x));
        uint64_t uy = static_cast<uint64_t>(static_cast<uint32_t>(y));
        uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(z));
        return (ux) | (uy << 32) | (uz << 48);
    }

    // We need a proper hash for uint64_t keys
    struct KeyHash {
        size_t operator()(uint64_t k) const {
            // Fibonacci hashing
            return static_cast<size_t>(k * 11400714819323198485ULL);
        }
    };

    std::unordered_map<uint64_t, ChestInventory, KeyHash> chests_;
};
