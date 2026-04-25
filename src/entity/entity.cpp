#include "entity.h"
#include "world/world.h"
#include "core/block.h"
#include <cmath>
#include <algorithm>

// Tiny gap kept between AABB and block surface — same rationale as Physics::COLLISION_GAP.
static constexpr float ENTITY_COLLISION_GAP = 0.001f;

static bool isSolidBlock(const World& world, int x, int y, int z) {
    return BlockRegistry::instance().isSolid(world.getBlock(x, y, z));
}

// Returns true if any solid block overlaps the AABB defined by
//   [center - he, center + he]  on the axes other than `axis`,
// sampled at block row `blockSlab` on axis `axis`.
static bool slabHasSolid(const World& world, const glm::vec3& center,
                         const glm::vec3& he, int axis, int blockSlab) {
    glm::vec3 mn = center - he;
    glm::vec3 mx = center + he;
    int minX, maxX, minY, maxY, minZ, maxZ;
    if (axis == 0) {       // X slab
        minX = maxX = blockSlab;
        minY = (int)std::floor(mn.y); maxY = (int)std::floor(mx.y);
        minZ = (int)std::floor(mn.z); maxZ = (int)std::floor(mx.z);
    } else if (axis == 1) { // Y slab
        minX = (int)std::floor(mn.x); maxX = (int)std::floor(mx.x);
        minY = maxY = blockSlab;
        minZ = (int)std::floor(mn.z); maxZ = (int)std::floor(mx.z);
    } else {                // Z slab
        minX = (int)std::floor(mn.x); maxX = (int)std::floor(mx.x);
        minY = (int)std::floor(mn.y); maxY = (int)std::floor(mx.y);
        minZ = maxZ = blockSlab;
    }
    for (int y = minY; y <= maxY; ++y)
        for (int z = minZ; z <= maxZ; ++z)
            for (int x = minX; x <= maxX; ++x)
                if (isSolidBlock(world, x, y, z)) return true;
    return false;
}

void Entity::integrateMotion(const World& world, float dt) {
    dt = std::min(dt, 0.05f);
    onGround = false;

    // --- Y axis ---
    {
        float newY = position.y + velocity.y * dt;
        glm::vec3 probe = position; probe.y = newY;
        if (velocity.y < 0) {
            int slab = (int)std::floor(newY - halfExtents.y);
            if (slabHasSolid(world, probe, halfExtents, 1, slab)) {
                position.y = static_cast<float>(slab + 1) + halfExtents.y + ENTITY_COLLISION_GAP;
                velocity.y = 0.0f;
                onGround = true;
            } else {
                position.y = newY;
            }
        } else if (velocity.y > 0) {
            int slab = (int)std::floor(newY + halfExtents.y);
            if (slabHasSolid(world, probe, halfExtents, 1, slab)) {
                position.y = static_cast<float>(slab) - halfExtents.y - ENTITY_COLLISION_GAP;
                velocity.y = 0.0f;
            } else {
                position.y = newY;
            }
        }
    }

    // --- X axis ---
    {
        float newX = position.x + velocity.x * dt;
        glm::vec3 probe = position; probe.x = newX;
        float edge = (velocity.x > 0) ? (newX + halfExtents.x) : (newX - halfExtents.x);
        int slab = (int)std::floor(edge);
        if (velocity.x != 0.0f && slabHasSolid(world, probe, halfExtents, 0, slab)) {
            if (velocity.x > 0)
                position.x = static_cast<float>(slab) - halfExtents.x - ENTITY_COLLISION_GAP;
            else
                position.x = static_cast<float>(slab + 1) + halfExtents.x + ENTITY_COLLISION_GAP;
            velocity.x = 0.0f;
        } else {
            position.x = newX;
        }
    }

    // --- Z axis ---
    {
        float newZ = position.z + velocity.z * dt;
        glm::vec3 probe = position; probe.z = newZ;
        float edge = (velocity.z > 0) ? (newZ + halfExtents.z) : (newZ - halfExtents.z);
        int slab = (int)std::floor(edge);
        if (velocity.z != 0.0f && slabHasSolid(world, probe, halfExtents, 2, slab)) {
            if (velocity.z > 0)
                position.z = static_cast<float>(slab) - halfExtents.z - ENTITY_COLLISION_GAP;
            else
                position.z = static_cast<float>(slab + 1) + halfExtents.z + ENTITY_COLLISION_GAP;
            velocity.z = 0.0f;
        } else {
            position.z = newZ;
        }
    }
}
