#include "entity_manager.h"
#include "world/world.h"
#include "player/player.h"
#include "player/inventory.h"
#include "core/debug.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// MC: entities more than 128 blocks from any player despawn next tick.
static constexpr float ENTITY_DESPAWN_DIST    = 128.0f;
static constexpr float ENTITY_DESPAWN_DIST_SQ = ENTITY_DESPAWN_DIST * ENTITY_DESPAWN_DIST;

// Randomish horizontal bounce for a new drop — deterministic, uses position
// hash so the same block always scatters the same way (nice for replays).
static glm::vec3 dropScatterVelocity(const glm::vec3& pos) {
    uint32_t h = static_cast<uint32_t>(std::lround(pos.x * 928371.0f))
               ^ static_cast<uint32_t>(std::lround(pos.y * 6971.0f))
               ^ static_cast<uint32_t>(std::lround(pos.z * 373.0f));
    float vx = ((h & 0xFF) / 255.0f - 0.5f) * 0.2f;
    float vz = (((h >> 8) & 0xFF) / 255.0f - 0.5f) * 0.2f;
    return glm::vec3(vx, 0.2f, vz);
}

void EntityManager::tick(World& world, Player& player, Inventory& inventory) {
    glm::vec3 playerCentre = player.position;

    for (auto& e : entities_) {
        if (!e->alive) continue;

        // Snapshot state BEFORE we advance — renderer will mix(prev, current, partialTick).
        e->captureInterpState();

        glm::vec3 d = e->position - playerCentre;
        if (glm::dot(d, d) > ENTITY_DESPAWN_DIST_SQ) {
            e->alive = false;
            continue;
        }
        e->tick(world, *this, player, inventory);
    }

    // Sweep dead entities out at end — keeps references stable during iteration.
    entities_.erase(
        std::remove_if(entities_.begin(), entities_.end(),
                       [](const std::unique_ptr<Entity>& e) { return !e || !e->alive; }),
        entities_.end());
}

void EntityManager::spawnItem(const glm::vec3& worldPos, const ItemStack& stack,
                              const glm::vec3& initialVel) {
    if (stack.isEmpty()) return;
    auto item = std::make_unique<ItemEntity>();
    item->position = worldPos;
    // Keep the interpolation snapshot coherent on the first frame — if left at
    // (0,0,0) the renderer would draw a streak from the world origin into the
    // spawn point for one tick.
    item->prevPosition = worldPos;
    // Combine the caller's velocity with a bit of scatter so piles of drops
    // spread out visually instead of stacking on top of each other.
    item->velocity = initialVel + dropScatterVelocity(worldPos);
    item->stack = stack;
    // Deterministic per-position bob phase.
    item->visualPhase = std::fmod(worldPos.x + worldPos.z * 1.37f, 6.2831853f);
    item->prevVisualYaw = item->visualYaw;
    VLOG(DebugCat::Entity, "spawn itemId=%u count=%u pos=(%.2f,%.2f,%.2f)",
         stack.id, stack.count, worldPos.x, worldPos.y, worldPos.z);
    entities_.push_back(std::move(item));
}
