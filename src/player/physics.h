#pragma once

#include "player.h"
#include "world/world.h"

// AABB collision detection and resolution for the player against the voxel world.
class Physics {
public:
    // Apply gravity, movement, and resolve collisions.
    // Call once per frame with delta time.
    static void update(Player& player, const World& world, float dt);

private:
    // Check if any solid block overlaps with the player's AABB at the given position
    static bool isCollidingAt(const World& world, const glm::vec3& feetPos);

    // Resolve collision along a single axis
    static void resolveAxis(Player& player, const World& world, int axis, float dt);
};
