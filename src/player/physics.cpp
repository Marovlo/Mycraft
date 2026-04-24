#include "physics.h"
#include "core/block.h"
#include <cmath>
#include <algorithm>

static bool isSolidBlock(const World& world, int x, int y, int z) {
    return BlockRegistry::instance().isSolid(world.getBlock(x, y, z));
}

void Physics::update(Player& player, const World& world, float dt) {
    dt = std::min(dt, 0.05f); // Cap to avoid tunneling

    float speed = player.sprinting ? SPRINT_SPEED : MOVE_SPEED;

    // Apply gravity
    player.velocity.y -= GRAVITY * dt;

    // Clamp terminal velocity
    player.velocity.y = std::max(player.velocity.y, -78.4f); // MC terminal velocity

    // Move and resolve each axis independently to prevent corner sticking
    // Y first (gravity), then X, then Z
    player.onGround = false;

    // Y axis
    {
        float newY = player.position.y + player.velocity.y * dt;
        float halfW = PLAYER_WIDTH * 0.5f;

        int minX = static_cast<int>(std::floor(player.position.x - halfW));
        int maxX = static_cast<int>(std::floor(player.position.x + halfW));
        int minZ = static_cast<int>(std::floor(player.position.z - halfW));
        int maxZ = static_cast<int>(std::floor(player.position.z + halfW));

        if (player.velocity.y < 0) {
            // Falling: check below feet
            int footY = static_cast<int>(std::floor(newY));
            bool collided = false;
            for (int bx = minX; bx <= maxX && !collided; bx++)
                for (int bz = minZ; bz <= maxZ && !collided; bz++)
                    if (isSolidBlock(world, bx, footY, bz))
                        collided = true;

            if (collided) {
                player.position.y = static_cast<float>(static_cast<int>(std::floor(newY)) + 1);
                player.velocity.y = 0;
                player.onGround = true;
            } else {
                player.position.y = newY;
            }
        } else if (player.velocity.y > 0) {
            // Rising: check above head
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
        float halfW = PLAYER_WIDTH * 0.5f;

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
                player.position.x = static_cast<float>(checkX) - halfW - 0.001f;
            else
                player.position.x = static_cast<float>(checkX + 1) + halfW + 0.001f;
            player.velocity.x = 0;
        } else {
            player.position.x = newX;
        }
    }

    // Z axis
    {
        float newZ = player.position.z + player.velocity.z * dt;
        float halfW = PLAYER_WIDTH * 0.5f;

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
                player.position.z = static_cast<float>(checkZ) - halfW - 0.001f;
            else
                player.position.z = static_cast<float>(checkZ + 1) + halfW + 0.001f;
            player.velocity.z = 0;
        } else {
            player.position.z = newZ;
        }
    }
}
