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

bool Physics::playerIntersectsBlock(const Player& player, int bx, int by, int bz) {
    // Player AABB in world units — same convention as update().
    float halfW = PLAYER_WIDTH * 0.5f;
    float pxMin = player.position.x - halfW;
    float pxMax = player.position.x + halfW;
    float pyMin = player.position.y;
    float pyMax = player.position.y + PLAYER_HEIGHT;
    float pzMin = player.position.z - halfW;
    float pzMax = player.position.z + halfW;

    // Block AABB: integer cell [b, b+1].
    float blockMinX = static_cast<float>(bx);
    float blockMinY = static_cast<float>(by);
    float blockMinZ = static_cast<float>(bz);
    float blockMaxX = blockMinX + 1.0f;
    float blockMaxY = blockMinY + 1.0f;
    float blockMaxZ = blockMinZ + 1.0f;

    // Half-open intervals are fine for discrete block grids; use <= / >= so that
    // touching exactly (zero-thickness overlap) still counts as a rejection,
    // mirroring MC which blocks placement even when the player is *standing on*
    // the target cell.
    return !(pxMax <= blockMinX || pxMin >= blockMaxX ||
             pyMax <= blockMinY || pyMin >= blockMaxY ||
             pzMax <= blockMinZ || pzMin >= blockMaxZ);
}

void Physics::update(Player& player, const World& world, float dt) {
    dt = std::min(dt, 0.05f);

    // Check if feet are in water (for swimming physics)
    int footX = static_cast<int>(std::floor(player.position.x));
    int footY = static_cast<int>(std::floor(player.position.y));
    int footZ = static_cast<int>(std::floor(player.position.z));
    bool feetInWater = BlockRegistry::instance().isLiquid(world.getBlock(footX, footY, footZ));

    if (feetInWater) {
        // Water physics: reduced gravity, water drag
        player.velocity.y -= GRAVITY * dt * 0.15f;  // much less gravity in water
        player.velocity.y = std::max(player.velocity.y, -4.0f);  // slow sink

        // Water drag on all axes
        player.velocity.x *= 0.85f;
        player.velocity.z *= 0.85f;
        player.velocity.y *= 0.90f;

        // Swimming: space key makes player float up (handled by handleTickInput
        // setting velocity.y positive)
    } else {
        // Normal gravity
        player.velocity.y -= GRAVITY * dt;
        player.velocity.y = std::max(player.velocity.y, -78.4f);
    }

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
            // Sneak edge prevention: if sneaking and on ground, check if the new
            // position would leave us with no solid block underfoot. If so, clamp.
            if (player.sneaking && player.onGround) {
                int footY = static_cast<int>(std::floor(player.position.y)) - 1;
                int newMinX = static_cast<int>(std::floor(newX - halfW));
                int newMaxX = static_cast<int>(std::floor(newX + halfW));
                bool hasGround = false;
                for (int bx = newMinX; bx <= newMaxX && !hasGround; bx++)
                    for (int bz = minZ; bz <= maxZ && !hasGround; bz++)
                        if (isSolidBlock(world, bx, footY, bz))
                            hasGround = true;
                if (!hasGround) {
                    player.velocity.x = 0;
                } else {
                    player.position.x = newX;
                }
            } else {
                player.position.x = newX;
            }
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
            // Sneak edge prevention on Z axis
            if (player.sneaking && player.onGround) {
                int footY = static_cast<int>(std::floor(player.position.y)) - 1;
                int newMinZ = static_cast<int>(std::floor(newZ - halfW));
                int newMaxZ = static_cast<int>(std::floor(newZ + halfW));
                bool hasGround = false;
                for (int bx = minX; bx <= maxX && !hasGround; bx++)
                    for (int bz = newMinZ; bz <= newMaxZ && !hasGround; bz++)
                        if (isSolidBlock(world, bx, footY, bz))
                            hasGround = true;
                if (!hasGround) {
                    player.velocity.z = 0;
                } else {
                    player.position.z = newZ;
                }
            } else {
                player.position.z = newZ;
            }
        }
    }
}
