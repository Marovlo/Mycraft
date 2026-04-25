#pragma once

#include "player.h"
#include "world/world.h"

// AABB collision detection and resolution for the player against the voxel world.
class Physics {
public:
    // Apply gravity, movement, and resolve collisions.
    // Call once per frame with delta time.
    static void update(Player& player, const World& world, float dt);

    // Returns true iff the player's AABB (feet-position + PLAYER_WIDTH × PLAYER_HEIGHT)
    // overlaps the 1×1×1 block at (bx, by, bz). Used to reject block placement
    // that would put a solid block inside the player (MC behavior).
    static bool playerIntersectsBlock(const Player& player, int bx, int by, int bz);

private:
    // Check if any solid block overlaps with the player's AABB at the given position
    static bool isCollidingAt(const World& world, const glm::vec3& feetPos);

    // Resolve collision along a single axis
    static void resolveAxis(Player& player, const World& world, int axis, float dt);
};
