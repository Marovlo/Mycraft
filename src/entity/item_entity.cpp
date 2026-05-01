#include "item_entity.h"
#include "entity_manager.h"
#include "world/world.h"
#include "player/player.h"
#include "player/inventory.h"
#include "core/common.h"
#include "core/tick_clock.h"
#include "core/debug.h"
#include "audio/sound_engine.h"
#include <glm/glm.hpp>
#include <cmath>

// MC item physics adapted for our dt-based integrator (integrateMotion uses
// position += velocity * dt, where dt = 1/20s = 0.05s).
// MC's original model: velocity is in blocks/tick and added directly to position.
// To match: our velocity needs to be 20× MC's so that vel * 0.05 = MC_vel * 1.
// Gravity: MC applies 0.04 b/tick² → our accel = 0.04 * 20 = 0.8 b/s per tick.
// But since we apply per tick: Δv = gravity (once per tick), effectively:
//   MC: v -= 0.04; pos += v;    (units: blocks/tick)
//   Us: v -= G;    pos += v*dt; (units: blocks/s)
// To match MC displacement per tick: v_us = v_mc / dt = v_mc * 20
// So G_us = 0.04 / dt = 0.04 * 20 = 0.8 (blocks/s per tick)
static constexpr float ITEM_GRAVITY   = 0.8f;    // Δv.y per tick (blocks/s units)
static constexpr float ITEM_DRAG_AIR  = 0.98f;   // applied to .xz each tick
static constexpr float ITEM_DRAG_GND  = 0.60f;   // extra ground friction on .xz
static constexpr float ITEM_DRAG_Y    = 0.98f;   // vertical drag
static constexpr float ITEM_TERMINAL_VEL = -60.0f; // blocks/s (~3 b/tick in MC units)

// Pickup tuning.
//
// MC-style: magnet radius ~3 blocks. Once inside, the entity's velocity is
// *set* (not accumulated) toward the player each tick, with speed inversely
// proportional to distance — closer = faster, giving the characteristic
// "slow start then snap" feel. This also means ground friction can't fight
// the magnet since we override velocity outright.
//
// Pickup itself fires when the entity AABB overlaps the player AABB (inflated
// by a small margin).
static constexpr float PICKUP_MAGNET_DIST  = 3.0f;
static constexpr float PICKUP_MAGNET_MIN_SPEED = 5.0f;  // blocks/s at outer edge
static constexpr float PICKUP_MAGNET_MAX_SPEED = 16.0f; // blocks/s right next to player
static constexpr float PICKUP_AABB_INFLATE = 0.25f;

void ItemEntity::tick(World& world, EntityManager& mgr,
                      Player& player, Inventory& inventory) {
    ++tickCount;

    // 1) Countdown timers.
    if (pickupDelayTicks > 0) --pickupDelayTicks;
    --lifetimeTicks;
    if (lifetimeTicks <= 0 || stack.isEmpty()) {
        alive = false;
        return;
    }

    // 2) Physics: gravity + drag + AABB integration.
    velocity.y -= ITEM_GRAVITY;
    if (velocity.y < ITEM_TERMINAL_VEL) velocity.y = ITEM_TERMINAL_VEL;

    // Integrate a full tick (1/20 s) — our integrator clamps dt internally.
    integrateMotion(world, static_cast<float>(TickClock::TICK_DURATION));

    // Apply drag after integration (velocity is already zero on collided axis).
    velocity.x *= ITEM_DRAG_AIR;
    velocity.z *= ITEM_DRAG_AIR;
    velocity.y *= ITEM_DRAG_Y;
    if (onGround) {
        velocity.x *= ITEM_DRAG_GND;
        velocity.z *= ITEM_DRAG_GND;
    }

    // 3) Visual spin — one full rotation every ~4 s.
    visualYaw += 0.0785f; // 2π / 80 ticks
    if (visualYaw > 6.2831853f) visualYaw -= 6.2831853f;

    // 4) Pickup check.
    if (pickupDelayTicks > 0) return;

    // Centre-distance test drives *magnet attraction* only. The actual pickup
    // uses AABB overlap so the item has time to visibly travel toward the
    // player instead of teleporting the moment it enters magnet range.
    glm::vec3 playerCentre = player.position + glm::vec3(0.0f, PLAYER_HEIGHT * 0.5f, 0.0f);
    glm::vec3 diff = playerCentre - position;
    float distSq = glm::dot(diff, diff);

    if (distSq < PICKUP_MAGNET_DIST * PICKUP_MAGNET_DIST) {
        float dist = std::sqrt(std::max(distSq, 0.01f));
        glm::vec3 dir = diff / dist;

        // MC-style: SET velocity toward player (not accumulate), speed is
        // inversely proportional to distance so close items snap quickly.
        float t = 1.0f - std::min(dist / PICKUP_MAGNET_DIST, 1.0f); // 0=far, 1=close
        float speed = PICKUP_MAGNET_MIN_SPEED + t * (PICKUP_MAGNET_MAX_SPEED - PICKUP_MAGNET_MIN_SPEED);
        velocity = dir * speed;

        VLOG(DebugCat::Entity,
             "magnet tick=%d dist=%.2f speed=%.3f pos=(%.2f,%.2f,%.2f)",
             tickCount, dist, speed, position.x, position.y, position.z);
    }

    // AABB-based pickup test. Player AABB (feet..feet+height, ±halfWidth) vs
    // item AABB inflated by PICKUP_AABB_INFLATE on all sides.
    float halfW = PLAYER_WIDTH * 0.5f;
    glm::vec3 pMin(player.position.x - halfW, player.position.y,                   player.position.z - halfW);
    glm::vec3 pMax(player.position.x + halfW, player.position.y + PLAYER_HEIGHT,   player.position.z + halfW);
    glm::vec3 iMin = position - halfExtents - glm::vec3(PICKUP_AABB_INFLATE);
    glm::vec3 iMax = position + halfExtents + glm::vec3(PICKUP_AABB_INFLATE);
    bool overlap = pMax.x > iMin.x && pMin.x < iMax.x &&
                   pMax.y > iMin.y && pMin.y < iMax.y &&
                   pMax.z > iMin.z && pMin.z < iMax.z;
    if (overlap) {
        uint16_t before = stack.count;
        uint16_t leftover = inventory.addItem(stack);
        stack.count = leftover;
        VLOG(DebugCat::Entity,
             "pickup id=%u taken=%u leftover=%u", stack.id, before - leftover, leftover);
        if (stack.count == 0) {
            // MC 原版：拾取物品播放 pop 音效
            getSoundEngine().play(SoundEventId::ItemPickup, position, 0.3f);
            mgr.notifyItemPickup();  // 通知 Game 层标记物品栏脏，同步给服务器
            alive = false;
        } else if (leftover < before) {
            // 部分拾取也播放音效
            getSoundEngine().play(SoundEventId::ItemPickup, position, 0.3f);
            mgr.notifyItemPickup();  // 部分拾取也需要同步
        }
    }
}
