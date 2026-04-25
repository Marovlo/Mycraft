#pragma once

#include <glm/glm.hpp>
#include <cstdint>

class World;
class Player;
class Inventory;
class EntityManager;

// Runtime type tag — avoids virtual dispatch in hot loops. Extend as new
// entity kinds arrive (mobs, projectiles, XP orbs, ...).
enum class EntityKind : uint8_t {
    Item,
    Mob,
    Arrow,
};

// Base class for everything that lives in the world besides the player and
// blocks. AABB is centered on `position` with `halfExtents` spanning in each
// direction — this is the MC convention for dropped items, mobs and arrows.
class Entity {
public:
    glm::vec3 position    {0.0f};   // center of AABB
    glm::vec3 velocity    {0.0f};
    glm::vec3 halfExtents {0.125f}; // default: tiny (0.25 cube) — subclasses override
    bool onGround = false;
    bool alive    = true;
    int  tickCount = 0;

    // --- Render interpolation ---
    // Snapshot of position/yaw at the START of the current tick. EntityManager
    // captures these before calling tick(); the renderer then mixes between
    // prev* and the current values using the frame's partialTick factor so
    // entities move smoothly at the display refresh rate instead of teleporting
    // once every 50 ms (MC does the exact same thing).
    glm::vec3 prevPosition {0.0f};
    float     prevVisualYaw = 0.0f;

    virtual ~Entity() = default;
    virtual void tick(World& world, EntityManager& mgr,
                      Player& player, Inventory& inventory) = 0;
    virtual EntityKind kind() const = 0;

    // Called by EntityManager right before tick() to capture interpolation state.
    // Subclasses override to also snapshot their own visual-only fields (yaw etc.).
    virtual void captureInterpState() {
        prevPosition = position;
    }

protected:
    // Per-axis swept collision against solid world blocks. Updates position,
    // clears the corresponding velocity component on hit, and sets onGround
    // when blocked while falling. Safe for any centered AABB entity.
    void integrateMotion(const World& world, float dt);
};
