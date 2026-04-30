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
#include <random>

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
    // 优化：先收集活跃生物索引，避免在 O(n²) 循环中反复检查 kind/alive
    static thread_local std::mt19937 pushRng{42};

    // 1. 收集活跃生物索引（避免在 O(n²) 内层循环中重复判断）
    activeMobs_.clear();
    for (size_t i = 0; i < entities_.size(); i++) {
        if (!entities_[i]->alive || entities_[i]->kind() != EntityKind::Mob) continue;
        auto& mob = static_cast<MobEntity&>(*entities_[i]);
        if (mob.isDying) continue;
        activeMobs_.push_back(i);
    }

    // 2. 生物之间的推挤（只遍历活跃生物对）
    for (size_t ai = 0; ai < activeMobs_.size(); ai++) {
        auto& mobA = static_cast<MobEntity&>(*entities_[activeMobs_[ai]]);
        float radiusA = mobA.mobWidth * 0.5f;
        float halfHA = mobA.mobHeight * 0.5f;

        for (size_t bi = ai + 1; bi < activeMobs_.size(); bi++) {
            auto& mobB = static_cast<MobEntity&>(*entities_[activeMobs_[bi]]);

            // 快速距离平方预筛选（避免 sqrt）
            float dx = mobA.position.x - mobB.position.x;
            float dz = mobA.position.z - mobB.position.z;
            float distSqXZ = dx * dx + dz * dz;

            float radiusB = mobB.mobWidth * 0.5f;
            float minDist = radiusA + radiusB;
            float minDistSq = minDist * minDist;

            if (distSqXZ >= minDistSq) continue;

            // Y轴高度重叠检查
            float halfHB = mobB.mobHeight * 0.5f;
            float yMinA = mobA.position.y - halfHA;
            float yMaxA = mobA.position.y + halfHA;
            float yMinB = mobB.position.y - halfHB;
            float yMaxB = mobB.position.y + halfHB;
            if (yMaxA <= yMinB || yMaxB <= yMinA) continue;

            // 通过预筛选后才计算 sqrt
            float distXZ = std::sqrt(distSqXZ);
            float overlap = minDist - distXZ;
            float pushStrength = overlap * 0.5f;

            if (distXZ > 0.001f) {
                float invDist = 1.0f / distXZ;
                float nx = dx * invDist;
                float nz = dz * invDist;
                glm::vec3 newPosA = mobA.position;
                newPosA.x += nx * pushStrength;
                newPosA.z += nz * pushStrength;
                if (!mobCollidesWithBlocks(world, newPosA, radiusA, halfHA)) {
                    mobA.position = newPosA;
                }
                glm::vec3 newPosB = mobB.position;
                newPosB.x -= nx * pushStrength;
                newPosB.z -= nz * pushStrength;
                if (!mobCollidesWithBlocks(world, newPosB, radiusB, halfHB)) {
                    mobB.position = newPosB;
                }
            } else {
                // 完全重叠时随机方向推开（使用确定性 RNG 替代 std::rand）
                std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
                float angle = angleDist(pushRng);
                float cosA = std::cos(angle), sinA = std::sin(angle);
                glm::vec3 newPosA = mobA.position;
                newPosA.x += cosA * pushStrength;
                newPosA.z += sinA * pushStrength;
                if (!mobCollidesWithBlocks(world, newPosA, radiusA, halfHA)) {
                    mobA.position = newPosA;
                }
                glm::vec3 newPosB = mobB.position;
                newPosB.x -= cosA * pushStrength;
                newPosB.z -= sinA * pushStrength;
                if (!mobCollidesWithBlocks(world, newPosB, radiusB, halfHB)) {
                    mobB.position = newPosB;
                }
            }
        }
    }

    // 3. 生物与玩家之间的推挤（复用 activeMobs_ 列表）
    if (!player.dead) {
        float playerRadius = PLAYER_WIDTH * 0.5f;
        float playerYMin = player.position.y;
        float playerYMax = player.position.y + PLAYER_HEIGHT;

        for (size_t idx : activeMobs_) {
            auto& mob = static_cast<MobEntity&>(*entities_[idx]);

            float dx = player.position.x - mob.position.x;
            float dz = player.position.z - mob.position.z;
            float distSqXZ = dx * dx + dz * dz;

            float mobRadius = mob.mobWidth * 0.5f;
            float minDist = playerRadius + mobRadius;

            if (distSqXZ >= minDist * minDist) continue;

            // Y轴高度重叠检查
            float halfH = mob.mobHeight * 0.5f;
            float mobYMin = mob.position.y - halfH;
            float mobYMax = mob.position.y + halfH;
            if (playerYMax <= mobYMin || mobYMax <= playerYMin) continue;

            float distXZ = std::sqrt(distSqXZ);
            float overlap = minDist - distXZ;
            float pushStrength = overlap * 0.5f;

            if (distXZ > 0.001f) {
                float invDist = 1.0f / distXZ;
                float nx = dx * invDist;
                float nz = dz * invDist;
                player.position.x += nx * pushStrength;
                player.position.z += nz * pushStrength;
                glm::vec3 newPos = mob.position;
                newPos.x -= nx * pushStrength;
                newPos.z -= nz * pushStrength;
                if (!mobCollidesWithBlocks(world, newPos, mobRadius, halfH)) {
                    mob.position = newPos;
                }
            } else {
                std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
                float angle = angleDist(pushRng);
                float cosA = std::cos(angle), sinA = std::sin(angle);
                player.position.x += cosA * pushStrength;
                player.position.z += sinA * pushStrength;
                glm::vec3 newPos = mob.position;
                newPos.x -= cosA * pushStrength;
                newPos.z -= sinA * pushStrength;
                if (!mobCollidesWithBlocks(world, newPos, mobRadius, halfH)) {
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

void EntityManager::spawnArrow(const glm::vec3& from, const glm::vec3& dir,
                               float speed, int damage, bool fromPlayer) {
    auto arrow = std::make_unique<ArrowEntity>();
    arrow->fromPlayer = fromPlayer;
    arrow->launch(from, dir, speed, damage);
    VLOG(DebugCat::Entity, "spawn arrow pos=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f) spd=%.2f dmg=%d",
         from.x, from.y, from.z, dir.x, dir.y, dir.z, speed, damage);
    entities_.push_back(std::move(arrow));
}

void EntityManager::spawnXPOrbs(const glm::vec3& worldPos, int totalXP) {
    if (totalXP <= 0) return;

    auto splits = XPOrbEntity::splitXP(totalXP);
    for (int xpVal : splits) {
        auto orb = std::make_unique<XPOrbEntity>(xpVal);
        orb->position = worldPos + glm::vec3(0.0f, 0.3f, 0.0f);
        orb->prevPosition = orb->position;

        // 随机散射速度（类似物品掉落）
        float angle = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
        float hSpeed = 0.1f + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 0.15f;
        orb->velocity = glm::vec3(
            std::cos(angle) * hSpeed,
            0.3f + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 0.2f,
            std::sin(angle) * hSpeed
        );

        orb->visualPhase = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;
        VLOG(DebugCat::Entity, "spawn xp orb value=%d pos=(%.2f,%.2f,%.2f)",
             xpVal, worldPos.x, worldPos.y, worldPos.z);
        entities_.push_back(std::move(orb));
    }
}
