#include "mob_spawner.h"
#include "entity_manager.h"
#include "world/world.h"
#include "world/day_night_cycle.h"
#include "player/player.h"
#include "core/block.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

// MC 原版生成参数
static constexpr int SPAWN_INTERVAL_TICKS = 400;  // 每 400 tick 尝试一次
static constexpr float MIN_SPAWN_DIST = 24.0f;
static constexpr float MAX_SPAWN_DIST = 128.0f;
static constexpr int MAX_HOSTILE_MOBS = 70;
static constexpr int MAX_PASSIVE_MOBS = 10;

void MobSpawner::tick(World& world, Player& player, EntityManager& mgr,
                      const DayNightCycle& dayNight) {
    // 消失规则
    despawnFarMobs(mgr, player.position);

    // 定时生成
    spawnTimer_++;
    if (spawnTimer_ < SPAWN_INTERVAL_TICKS) return;
    spawnTimer_ = 0;

    // 敌对生物生成（夜晚或暗处）
    int hostileCount = countMobs(mgr, true);
    if (hostileCount < MAX_HOSTILE_MOBS) {
        // 随机选择玩家周围的位置
        for (int attempt = 0; attempt < 4; attempt++) {
            float angle = (std::rand() % 360) * 3.14159f / 180.0f;
            float dist = MIN_SPAWN_DIST + (std::rand() % static_cast<int>(MAX_SPAWN_DIST - MIN_SPAWN_DIST));
            int sx = static_cast<int>(player.position.x + std::cos(angle) * dist);
            int sz = static_cast<int>(player.position.z + std::sin(angle) * dist);
            int sy = getSurfaceY(world, sx, sz);
            if (sy <= 0) continue;

            // 检查亮度 — 敌对生物需要暗处 (skyLight * factor <= 7)
            // 简化：夜晚或地下
            bool isDark = dayNight.isNight() || sy < 50;
            if (!isDark) continue;

            // 随机选择敌对生物类型
            MobType types[] = {MobType::Zombie, MobType::Skeleton, MobType::Spider, MobType::Creeper};
            MobType type = types[std::rand() % 4];

            int groupSize = 1 + (std::rand() % 3);  // 1-3只
            trySpawnGroup(world, mgr, type, sx, sy, sz, groupSize);
        }
    }

    // 被动生物不自然生成（MC原版行为：只在世界生成时放置）
}

void MobSpawner::spawnInitialMobs(World& world, EntityManager& mgr, int chunkX, int chunkZ) {
    // 每个区块有概率生成 0-2 只被动生物
    int count = std::rand() % 3;  // 0-2
    if (count == 0) return;

    MobType passiveTypes[] = {MobType::Pig, MobType::Cow, MobType::Sheep, MobType::Chicken};

    for (int i = 0; i < count; i++) {
        int lx = std::rand() % 16;
        int lz = std::rand() % 16;
        int wx = chunkX * 16 + lx;
        int wz = chunkZ * 16 + lz;
        int wy = getSurfaceY(world, wx, wz);
        if (wy <= 0) continue;

        // 检查是否是草地
        BlockId surface = world.getBlock(wx, wy - 1, wz);
        if (surface != Block::Grass && surface != Block::Dirt) continue;

        MobType type = passiveTypes[std::rand() % 4];
        if (canSpawnAt(world, type, wx, wy, wz)) {
            auto mob = std::make_unique<MobEntity>(type);
            mob->position = glm::vec3(wx + 0.5f, wy + mob->mobHeight * 0.5f, wz + 0.5f);
            mob->prevPosition = mob->position;
            mob->bodyYaw = (std::rand() % 360) * 3.14159f / 180.0f;
            mob->prevBodyYaw = mob->bodyYaw;
            mgr.addEntity(std::move(mob));
        }
    }
}

int MobSpawner::countMobs(const EntityManager& mgr, bool hostile) const {
    int count = 0;
    for (const auto& e : mgr.entities()) {
        if (!e || !e->alive || e->kind() != EntityKind::Mob) continue;
        const auto& mob = static_cast<const MobEntity&>(*e);
        const auto& props = MobRegistry::instance().get(mob.mobType);
        if (props.isHostile == hostile) count++;
    }
    return count;
}

bool MobSpawner::trySpawnGroup(World& world, EntityManager& mgr,
                                MobType type, int x, int y, int z, int count) {
    int spawned = 0;
    for (int i = 0; i < count; i++) {
        int sx = x + (std::rand() % 5) - 2;
        int sz = z + (std::rand() % 5) - 2;
        int sy = getSurfaceY(world, sx, sz);
        if (sy <= 0) continue;

        if (canSpawnAt(world, type, sx, sy, sz)) {
            auto mob = std::make_unique<MobEntity>(type);
            mob->position = glm::vec3(sx + 0.5f, sy + mob->mobHeight * 0.5f, sz + 0.5f);
            mob->prevPosition = mob->position;
            mob->bodyYaw = (std::rand() % 360) * 3.14159f / 180.0f;
            mob->prevBodyYaw = mob->bodyYaw;
            mgr.addEntity(std::move(mob));
            spawned++;
        }
    }
    return spawned > 0;
}

bool MobSpawner::canSpawnAt(const World& world, MobType type, int x, int y, int z) const {
    const auto& props = MobRegistry::instance().get(type);

    // 脚下必须是实心方块
    BlockId below = world.getBlock(x, y - 1, z);
    if (!BlockRegistry::instance().isSolid(below)) return false;

    // 身体空间必须是空气
    int headBlocks = static_cast<int>(std::ceil(props.height));
    for (int dy = 0; dy < headBlocks; dy++) {
        BlockId b = world.getBlock(x, y + dy, z);
        if (b != Block::Air) return false;
    }

    // 不能在水中生成（除了溺尸，暂不实现）
    if (BlockRegistry::instance().isLiquid(world.getBlock(x, y, z))) return false;

    return true;
}

int MobSpawner::getSurfaceY(const World& world, int x, int z) const {
    for (int y = 200; y > 0; y--) {
        BlockId b = world.getBlock(x, y, z);
        if (BlockRegistry::instance().isSolid(b)) {
            return y + 1;
        }
    }
    return 0;
}

void MobSpawner::despawnFarMobs(EntityManager& mgr, const glm::vec3& playerPos) {
    for (const auto& e : mgr.entities()) {
        if (!e || !e->alive || e->kind() != EntityKind::Mob) continue;
        auto& mob = static_cast<MobEntity&>(*e);

        float dist = glm::length(mob.position - playerPos);

        // 超过 128 格立即消失
        if (dist > 128.0f) {
            // 只消失敌对生物，被动生物保留
            const auto& props = MobRegistry::instance().get(mob.mobType);
            if (props.isHostile) {
                mob.alive = false;
            }
            continue;
        }

        // 32-128 格：每 tick 1/800 概率消失（仅敌对）
        if (dist > 32.0f) {
            const auto& props = MobRegistry::instance().get(mob.mobType);
            if (props.isHostile && (std::rand() % 800) == 0) {
                mob.alive = false;
            }
        }
    }
}
