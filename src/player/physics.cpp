#include "physics.h"
#include "core/block.h"
#include <cmath>
#include <algorithm>

static bool isSolidBlock(const World& world, int x, int y, int z) {
    return BlockRegistry::instance().isSolid(world.getBlock(x, y, z));
}

// Extra gap between player AABB and block surface to prevent camera clipping.
// Must be large enough that near plane + FOV side projection doesn't see inside walls.
// With halfW=0.3, near=0.05, FOV=70: side reach = 0.05 * tan(35°) ≈ 0.035
// So gap of 0.04 is enough.
static constexpr float COLLISION_GAP = 0.04f;

void Physics::update(Player& player, const World& world, float dt) {
    dt = std::min(dt, 0.05f);

    // Apply gravity
    player.velocity.y -= GRAVITY * dt;
    player.velocity.y = std::max(player.velocity.y, -78.4f);

    player.onGround = false;
    float halfW = PLAYER_WIDTH * 0.5f;

    // Y axis
    {
        float newY = player.position.y + player.velocity.y * dt;

        int minX = static_cast<int>(std::floor(player.position.x - halfW));
        int maxX = static_cast<int>(std::floor(player.position.x + halfW));
        int minZ = static_cast<int>(std::floor(player.position.z - halfW));
        int maxZ = static_cast<int>(std::floor(player.position.z + halfW));

        if (player.velocity.y < 0) {
            int footY = static_cast<int>(std::floor(newY));
            bool collided = false;
            for (int bx = minX; bx <= maxX && !collided; bx++)
                for (int bz = minZ; bz <= maxZ && !collided; bz++)
                    if (isSolidBlock(world, bx, footY, bz))
                        collided = true;

            if (collided) {
                player.position.y = static_cast<float>(footY + 1);
                player.velocity.y = 0;
                player.onGround = true;
            } else {
                player.position.y = newY;
            }
        } else if (player.velocity.y > 0) {
            int headY = static_cast<int>(std::floor(newY + PLAYER_HEIGHT));
            bool collided = false;
            for (int bx = minX; bx <= maxX && !collided; bx++)
                for (int bz = minZ; bz <= maxZ && !collided; bz++)
                    if (isSolidBlock(world, bx, headY, bz))
                        collided = true;

            if (collided) {
                player.velocity.y = 0;
            } else {
                player.position.y = newY;
            }
        }
    }

    // X axis
    {
        float newX = player.position.x + player.velocity.x * dt;

        int minY = static_cast<int>(std::floor(player.position.y));
        int maxY = static_cast<int>(std::floor(player.position.y + PLAYER_HEIGHT));
        int minZ = static_cast<int>(std::floor(player.position.z - halfW));
        int maxZ = static_cast<int>(std::floor(player.position.z + halfW));

        float edgeX = newX + (player.velocity.x > 0 ? halfW : -halfW);
        int checkX = static_cast<int>(std::floor(edgeX));
        bool collided = false;

        for (int by = minY; by <= maxY && !collided; by++)
            for (int bz = minZ; bz <= maxZ && !collided; bz++)
                if (isSolidBlock(world, checkX, by, bz))
                    collided = true;

        if (collided) {
            if (player.velocity.x > 0)
                player.position.x = static_cast<float>(checkX) - halfW - COLLISION_GAP;
            else
                player.position.x = static_cast<float>(checkX + 1) + halfW + COLLISION_GAP;
            player.velocity.x = 0;
        } else {
            player.position.x = newX;
        }
    }

    // Z axis
    {
        float newZ = player.position.z + player.velocity.z * dt;

        int minY = static_cast<int>(std::floor(player.position.y));
        int maxY = static_cast<int>(std::floor(player.position.y + PLAYER_HEIGHT));
        int minX = static_cast<int>(std::floor(player.position.x - halfW));
        int maxX = static_cast<int>(std::floor(player.position.x + halfW));

        float edgeZ = newZ + (player.velocity.z > 0 ? halfW : -halfW);
        int checkZ = static_cast<int>(std::floor(edgeZ));
        bool collided = false;

        for (int by = minY; by <= maxY && !collided; by++)
            for (int bx = minX; bx <= maxX && !collided; bx++)
                if (isSolidBlock(world, bx, by, checkZ))
                    collided = true;

        if (collided) {
            if (player.velocity.z > 0)
                player.position.z = static_cast<float>(checkZ) - halfW - COLLISION_GAP;
            else
                player.position.z = static_cast<float>(checkZ + 1) + halfW + COLLISION_GAP;
            player.velocity.z = 0;
        } else {
            player.position.z = newZ;
        }
    }
}
