#pragma once

#include "core/block.h"
#include "core/item.h"
#include "world/world.h"
#include "player/player.h"
#include "player/inventory.h"

#include <functional>

class EntityManager;

// Drives MC-style "hold left-mouse to mine a block" loop.
// Ticked at the fixed game rate (20 Hz). Frame-level input is reduced to a
// single boolean (leftMouseHeld) so the same logic stays deterministic.
//
// Lifecycle:
//   tick() each game tick with current input state + world + inventory.
//   When progress reaches 1.0 the block is removed, drops are spawned as
//   ItemEntities in the world (via EntityManager), and the held tool's
//   durability is decremented.
class BlockInteraction {
public:
    // Optional callback invoked just BEFORE a block is removed from the world.
    // Parameters: (blockId, x, y, z, entityMgr). Used by Game to drop chest
    // contents, etc. Set to nullptr to disable.
    using BlockBrokenCallback = std::function<void(BlockId, int, int, int, EntityManager&)>;
    BlockBrokenCallback onBlockBroken;

    // Called from Game::gameTick(). Reach is the maximum interaction distance
    // (matches Player::MAX_REACH). `entityMgr` is where drops are spawned.
    void tick(World& world, Player& player, Inventory& inventory,
              EntityManager& entityMgr,
              bool leftMouseHeld, float reach);

    // Reset all state (e.g. when player switches hotbar slot or releases mouse).
    void reset();

    // Inspection for HUD: returns true and fills out fields when actively mining.
    bool getActiveBreak(int& bx, int& by, int& bz, float& outProgress) const {
        if (!state_.active) return false;
        bx = state_.blockX; by = state_.blockY; bz = state_.blockZ;
        outProgress = state_.progress;
        return true;
    }

private:
    struct BreakState {
        bool      active   = false;
        int       blockX   = 0, blockY = 0, blockZ = 0;
        BlockId   blockId  = 0;
        float     progress = 0.0f;
        ItemId    toolItemId = Item::None; // tool used when started, for cancel-on-swap
    };
    BreakState state_;

    // Compute progress added per tick using the MC formula. Returns 0 for
    // unbreakable / fully-blocked-by-bad-tool combinations.
    static float computeBreakDeltaPerTick(const BlockProperties& block,
                                          const ItemStack& heldTool);

    // Decide whether the current tool satisfies the block's tool/level gating
    // for *drops*. Mining without correct tool is allowed (MC behavior) but
    // yields nothing.
    static bool canHarvestDrops(const BlockProperties& block,
                                const ItemStack& heldTool);

    // Emit drops into the world as ItemEntities. Removal of the block itself
    // is done by the caller so this stays side-effect-free w.r.t. the world.
    static void spawnDrops(EntityManager& entityMgr,
                           const BlockProperties& block,
                           int bx, int by, int bz);
};
