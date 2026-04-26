#include "entity_manager.h"
#include "world/world.h"
#include "player/player.h"
#include "player/inventory.h"
#include "core/debug.h"
#include "mob_entity.h"
#include "core/common.h"
#include "core/block.h"
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

// 检查生物在给定位置是否与实心方块重叠
static bool mobCollidesWithBlocks(const World& world, const glm::vec3& pos, float halfW, float halfH) {
    int minX = static_cast<int>(std::floor(pos.x - halfW));
    int maxX = static_cast<int>(std::floor(pos.x + halfW));
    int minY = static_cast<int>(std::floor(pos.y - halfH));
    int maxY = static_cast<int>(std::floor(pos.y + halfH));
    int minZ = static_cast<int>(std::floor(pos.z - halfW));
    int maxZ = static_cast<int>(std::floor(pos.z + halfW));
    for (int y = minY; y <= maxY; ++y)
        for (int z = minZ; z <= maxZ; ++z)
            for (int x = minX; x <= maxX; ++x)
                if (BlockRegistry::instance().isSolid(world.getBlock(x, y, z)))
                    return true;
    return false;
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

    // ========== 实体碰撞推挤（MC原版行为） ==========
    // MC 使用圆柱体碰撞：XZ平面上的圆形 + Y轴高度检查
    // 当两个实体的圆柱体重叠时，沿XZ平面推开

    // 1. 生物之间的推挤
    for (size_t i = 0; i < entities_.size(); i++) {
        if (!entities_[i]->alive || entities_[i]->kind() != EntityKind::Mob) continue;
        auto& mobA = static_cast<MobEntity&>(*entities_[i]);
        if (mobA.isDying) continue;

        for (size_t j = i + 1; j < entities_.size(); j++) {
            if (!entities_[j]->alive || entities_[j]->kind() != EntityKind::Mob) continue;
            auto& mobB = static_cast<MobEntity&>(*entities_[j]);
            if (mobB.isDying) continue;

            // 圆柱体碰撞检测（XZ平面圆形 + Y轴高度重叠）
            float dx = mobA.position.x - mobB.position.x;
            float dz = mobA.position.z - mobB.position.z;
            float distXZ = std::sqrt(dx * dx + dz * dz);

            float radiusA = mobA.mobWidth * 0.5f;
            float radiusB = mobB.mobWidth * 0.5f;
            float minDist = radiusA + radiusB;

            if (distXZ >= minDist) continue;

            // Y轴高度重叠检查（position是AABB中心）
            float halfHA = mobA.mobHeight * 0.5f;
            float halfHB = mobB.mobHeight * 0.5f;
            float yMinA = mobA.position.y - halfHA;
            float yMaxA = mobA.position.y + halfHA;
            float yMinB = mobB.position.y - halfHB;
            float yMaxB = mobB.position.y + halfHB;
            if (yMaxA <= yMinB || yMaxB <= yMinA) continue;

            // 计算推挤力
            float overlap = minDist - distXZ;
            float pushStrength = overlap * 0.5f;  // 每个实体推一半

            if (distXZ > 0.001f) {
                float nx = dx / distXZ;
                float nz = dz / distXZ;
                // 尝试推开，但检查方块碰撞
                glm::vec3 newPosA = mobA.position;
                newPosA.x += nx * pushStrength;
                newPosA.z += nz * pushStrength;
                if (!mobCollidesWithBlocks(world, newPosA, radiusA, mobA.mobHeight * 0.5f)) {
                    mobA.position = newPosA;
                }
                glm::vec3 newPosB = mobB.position;
                newPosB.x -= nx * pushStrength;
                newPosB.z -= nz * pushStrength;
                if (!mobCollidesWithBlocks(world, newPosB, radiusB, mobB.mobHeight * 0.5f)) {
                    mobB.position = newPosB;
                }
            } else {
                // 完全重叠时随机方向推开
                float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
                glm::vec3 newPosA = mobA.position;
                newPosA.x += std::cos(angle) * pushStrength;
                newPosA.z += std::sin(angle) * pushStrength;
                if (!mobCollidesWithBlocks(world, newPosA, radiusA, mobA.mobHeight * 0.5f)) {
                    mobA.position = newPosA;
                }
                glm::vec3 newPosB = mobB.position;
                newPosB.x -= std::cos(angle) * pushStrength;
                newPosB.z -= std::sin(angle) * pushStrength;
                if (!mobCollidesWithBlocks(world, newPosB, radiusB, mobB.mobHeight * 0.5f)) {
                    mobB.position = newPosB;
                }
            }
        }
    }

    // 2. 生物与玩家之间的推挤
    if (!player.dead) {
        float playerRadius = PLAYER_WIDTH * 0.5f;
        float playerYMin = player.position.y;  // 玩家position是脚底
        float playerYMax = player.position.y + PLAYER_HEIGHT;

        for (auto& e : entities_) {
            if (!e->alive || e->kind() != EntityKind::Mob) continue;
            auto& mob = static_cast<MobEntity&>(*e);
            if (mob.isDying) continue;

            float dx = player.position.x - mob.position.x;
            float dz = player.position.z - mob.position.z;
            float distXZ = std::sqrt(dx * dx + dz * dz);

            float mobRadius = mob.mobWidth * 0.5f;
            float minDist = playerRadius + mobRadius;

            if (distXZ >= minDist) continue;

            // Y轴高度重叠检查（mob的position是AABB中心）
            float halfH = mob.mobHeight * 0.5f;
            float mobYMin = mob.position.y - halfH;
            float mobYMax = mob.position.y + halfH;
            if (playerYMax <= mobYMin || mobYMax <= playerYMin) continue;

            // 推挤：双方各推一半
            float overlap = minDist - distXZ;
            float pushStrength = overlap * 0.5f;

            if (distXZ > 0.001f) {
                float nx = dx / distXZ;
                float nz = dz / distXZ;
                // 推开玩家（玩家不检查方块碰撞，由Physics处理）
                player.position.x += nx * pushStrength;
                player.position.z += nz * pushStrength;
                // 推开生物，检查方块碰撞
                glm::vec3 newPos = mob.position;
                newPos.x -= nx * pushStrength;
                newPos.z -= nz * pushStrength;
                if (!mobCollidesWithBlocks(world, newPos, mobRadius, mob.mobHeight * 0.5f)) {
                    mob.position = newPos;
                }
            } else {
                float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
                player.position.x += std::cos(angle) * pushStrength;
                player.position.z += std::sin(angle) * pushStrength;
                glm::vec3 newPos = mob.position;
                newPos.x -= std::cos(angle) * pushStrength;
                newPos.z -= std::sin(angle) * pushStrength;
                if (!mobCollidesWithBlocks(world, newPos, mobRadius, mob.mobHeight * 0.5f)) {
                    mob.position = newPos;
                }
            }
        }
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
