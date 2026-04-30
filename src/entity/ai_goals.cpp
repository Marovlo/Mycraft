#include "ai_goals.h"
#include "core/block.h"
#include <algorithm>
#include <queue>
#include <unordered_set>

// ========== 寻路辅助函数（从 mob_entity.cpp 迁移） ==========

static bool isWalkable(const World& world, int x, int y, int z, float mobHeight) {
    auto& reg = BlockRegistry::instance();
    BlockId below = world.getBlock(x, y - 1, z);
    if (!reg.isSolid(below)) return false;
    int headBlocks = static_cast<int>(std::ceil(mobHeight));
    for (int dy = 0; dy < headBlocks; dy++) {
        BlockId b = world.getBlock(x, y + dy, z);
        if (reg.isSolid(b) || reg.isLiquid(b)) return false;
    }
    return true;
}

static bool isSafeDrop(int fromY, int toY) {
    return (fromY - toY) <= 3;
}

static int findWalkableY(const World& world, int x, int z, int baseY,
                          float mobHeight, bool avoidCliffs) {
    if (isWalkable(world, x, baseY, z, mobHeight)) return baseY;
    if (isWalkable(world, x, baseY + 1, z, mobHeight)) return baseY + 1;
    for (int dy = 1; dy <= 4; dy++) {
        int tryY = baseY - dy;
        if (tryY < 1) break;
        if (isWalkable(world, x, tryY, z, mobHeight)) {
            if (avoidCliffs && !isSafeDrop(baseY, tryY)) return -1;
            return tryY;
        }
    }
    return -1;
}

static bool isWanderTargetSafe(const World& world, const glm::vec3& from,
                                const glm::vec3& target, float mobHeight) {
    int tx = static_cast<int>(std::floor(target.x));
    int tz = static_cast<int>(std::floor(target.z));
    int fromY = static_cast<int>(std::floor(from.y - mobHeight * 0.5f));
    int ty = findWalkableY(world, tx, tz, fromY, mobHeight, true);
    return ty >= 0;
}

// A* 寻路
struct PathNode {
    int x, y, z;
    float g, h;
    int parentIdx;
    float f() const { return g + h; }
};

static std::vector<glm::ivec2> findPath(const World& world, glm::ivec3 start, glm::ivec3 goal,
                                          float mobHeight, bool avoidCliffs = false,
                                          int maxSteps = 16) {
    struct NodeCompare {
        bool operator()(const std::pair<float, int>& a, const std::pair<float, int>& b) {
            return a.first > b.first;
        }
    };

    std::vector<PathNode> nodes;
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, NodeCompare> open;
    std::unordered_set<int64_t> closed;

    auto packKey = [](int x, int z) -> int64_t {
        return (static_cast<int64_t>(x) << 32) | (static_cast<int64_t>(z) & 0xFFFFFFFF);
    };

    float h0 = std::abs(static_cast<float>(goal.x - start.x)) +
                std::abs(static_cast<float>(goal.z - start.z));
    nodes.push_back({start.x, start.y, start.z, 0.0f, h0, -1});
    open.push({h0, 0});

    static const int dx[] = {1, -1, 0, 0, 1, -1, 1, -1};
    static const int dz[] = {0, 0, 1, -1, 1, 1, -1, -1};

    while (!open.empty() && nodes.size() < 512) {
        auto [fVal, idx] = open.top();
        open.pop();

        const auto& cur = nodes[idx];
        int64_t key = packKey(cur.x, cur.z);
        if (closed.count(key)) continue;
        closed.insert(key);

        if (cur.x == goal.x && cur.z == goal.z) {
            std::vector<glm::ivec2> path;
            int i = idx;
            while (i >= 0) {
                path.push_back({nodes[i].x, nodes[i].z});
                i = nodes[i].parentIdx;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (cur.g >= maxSteps) continue;

        for (int d = 0; d < 8; d++) {
            int nx = cur.x + dx[d];
            int nz = cur.z + dz[d];
            if (closed.count(packKey(nx, nz))) continue;

            int ny = findWalkableY(world, nx, nz, cur.y, mobHeight, avoidCliffs);
            if (ny < 0) continue;

            if (d >= 4) {
                int compX = dx[d];
                int compZ = dz[d];
                auto& reg = BlockRegistry::instance();
                bool xBlocked = reg.isSolid(world.getBlock(cur.x + compX, cur.y, cur.z));
                bool zBlocked = reg.isSolid(world.getBlock(cur.x, cur.y, cur.z + compZ));
                if (xBlocked && zBlocked) continue;
            }

            float stepCost = (ny > cur.y) ? 0.5f : 0.0f;
            float cost = ((d < 4) ? 1.0f : 1.414f) + stepCost;
            float ng = cur.g + cost;
            float nh = std::abs(static_cast<float>(goal.x - nx)) +
                       std::abs(static_cast<float>(goal.z - nz));
            int newIdx = static_cast<int>(nodes.size());
            nodes.push_back({nx, ny, nz, ng, nh, idx});
            open.push({ng + nh, newIdx});
        }
    }

    return {};
}

// ========== WanderGoal ==========

void WanderGoal::tick(MobEntity& mob, World& world, Player&, EntityManager&) {
    mob.stateTimer--;

    if (mob.aiState == AIState::Idle) {
        if (mob.stateTimer <= 0) {
            mob.aiState = AIState::Wander;
            mob.stateTimer = 40 + (std::rand() % 120);
            for (int attempt = 0; attempt < 5; attempt++) {
                float angle = (std::rand() % 360) * 3.14159f / 180.0f;
                float dist = 3.0f + (std::rand() % 5);
                glm::vec3 candidate = mob.position + glm::vec3(std::cos(angle) * dist, 0, std::sin(angle) * dist);
                if (isWanderTargetSafe(world, mob.position, candidate, mob.mobHeight)) {
                    mob.wanderTarget = candidate;
                    mob.hasWanderTarget = true;
                    break;
                }
            }
            if (!mob.hasWanderTarget) {
                mob.aiState = AIState::Idle;
                mob.stateTimer = 40 + (std::rand() % 80);
            }
        }
    } else if (mob.aiState == AIState::Wander) {
        if (mob.hasWanderTarget) {
            mob.moveToward(mob.wanderTarget, mob.moveSpeed, &world);
            float dx = mob.wanderTarget.x - mob.position.x;
            float dz = mob.wanderTarget.z - mob.position.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist < 1.0f || mob.stateTimer <= 0) {
                mob.aiState = AIState::Idle;
                mob.stateTimer = 40 + (std::rand() % 80);
                mob.hasWanderTarget = false;
            }
        } else {
            mob.aiState = AIState::Idle;
            mob.stateTimer = 40 + (std::rand() % 80);
        }
    }
}

// ========== FleeGoal ==========

void FleeGoal::tick(MobEntity& mob, World& world, Player& player, EntityManager&) {
    mob.stateTimer--;
    if (mob.stateTimer <= 0) {
        mob.aiState = AIState::Idle;
        mob.stateTimer = 40 + (std::rand() % 60);
        mob.panicTicks = 0;
        return;
    }

    glm::vec3 away = mob.position - player.position;
    if (glm::length(away) > 0.01f) {
        away = glm::normalize(away);
        if (mob.stateTimer % 15 == 0) {
            float randAngle = ((std::rand() % 120) - 60) * 3.14159f / 180.0f;
            float cosA = std::cos(randAngle), sinA = std::sin(randAngle);
            float nx = away.x * cosA - away.z * sinA;
            float nz = away.x * sinA + away.z * cosA;
            away.x = nx;
            away.z = nz;
        }
    } else {
        float angle = (std::rand() % 360) * 3.14159f / 180.0f;
        away = glm::vec3(std::cos(angle), 0, std::sin(angle));
    }
    float fleeSpeed = (mob.panicTicks > 0) ? mob.moveSpeed * 2.5f : mob.moveSpeed * 1.5f;
    mob.moveToward(mob.position + away * 5.0f, fleeSpeed, &world);
}

// ========== ChaseGoal ==========

bool ChaseGoal::canUse(const MobEntity& mob, const World& world, const Player& player) const {
    if (player.dead) return false;
    float dist = glm::length(mob.position - player.position);
    const auto& props = MobRegistry::instance().get(mob.mobType);
    if (dist > props.detectRange) return false;
    return mob.canSeePlayer(world, player);
}

void ChaseGoal::tick(MobEntity& mob, World& world, Player& player, EntityManager&) {
    const auto& props = MobRegistry::instance().get(mob.mobType);
    float distToPlayer = glm::length(mob.position - player.position);

    if (player.dead || distToPlayer > props.detectRange * 1.5f) {
        mob.aiState = AIState::Idle;
        mob.stateTimer = 40;
        mob.path.clear();
        return;
    }

    mob.aiState = AIState::Chase;

    // 更新寻路
    pathUpdateTimer--;
    if (pathUpdateTimer <= 0 || mob.path.empty()) {
        pathUpdateTimer = 20;
        glm::ivec3 start(static_cast<int>(std::floor(mob.position.x)),
                         static_cast<int>(std::floor(mob.position.y - mob.halfExtents.y)),
                         static_cast<int>(std::floor(mob.position.z)));
        glm::ivec3 goal(static_cast<int>(std::floor(player.position.x)),
                        static_cast<int>(std::floor(player.position.y)),
                        static_cast<int>(std::floor(player.position.z)));
        mob.path = findPath(world, start, goal, mob.mobHeight, false, 24);
        mob.pathIndex = 1;
    }

    // 沿路径移动
    if (!mob.path.empty() && mob.pathIndex < static_cast<int>(mob.path.size())) {
        glm::vec3 target(mob.path[mob.pathIndex].x + 0.5f, mob.position.y, mob.path[mob.pathIndex].y + 0.5f);
        mob.moveToward(target, mob.moveSpeed, &world);
        float d = glm::length(glm::vec2(target.x - mob.position.x, target.z - mob.position.z));
        if (d < 0.5f) mob.pathIndex++;
    } else {
        mob.moveToward(player.position, mob.moveSpeed, &world);
    }
}

// ========== MeleeAttackGoal ==========

void MeleeAttackGoal::tick(MobEntity& mob, World&, Player& player, EntityManager&) {
    float dist = glm::length(mob.position - player.position);
    if (dist <= mob.attackRange) {
        glm::vec3 kb = glm::normalize(player.position - mob.position);
        player.takeDamage(static_cast<int>(mob.attackDamage));
        player.velocity += kb * 5.0f;
        player.velocity.y += 4.0f;
    }
    const auto& props = MobRegistry::instance().get(mob.mobType);
    mob.attackCooldown = props.attackCooldownTicks;
}

// ========== RangedAttackGoal ==========

void RangedAttackGoal::tick(MobEntity& mob, World&, Player& player, EntityManager& mgr) {
    // MC 原版骷髅射箭行为：
    // 1. 进入攻击范围后开始蓄力（举起弓，手臂抬起动画）
    // 2. 蓄力 20 tick（1 秒）
    // 3. 蓄力完成后射出箭矢
    // 4. 进入攻击冷却

    static constexpr int SKELETON_CHARGE_TICKS = 20;  // MC 原版蓄力时间

    // 开始蓄力
    if (!mob.isChargingBow) {
        mob.isChargingBow = true;
        mob.bowChargeTicks = 0;
    }

    // 蓄力中：递增计数器
    mob.bowChargeTicks++;

    // 蓄力期间面向玩家
    glm::vec3 toPlayer = player.position - mob.position;
    float targetYaw = std::atan2(toPlayer.x, toPlayer.z);
    mob.bodyYaw = targetYaw;
    mob.headYaw = targetYaw;

    // 蓄力完成：射出箭矢
    if (mob.bowChargeTicks >= SKELETON_CHARGE_TICKS) {
        glm::vec3 eyePos = mob.position + glm::vec3(0, mob.mobHeight * 0.4f, 0);
        glm::vec3 targetPos = player.position + glm::vec3(0, 1.0f, 0);
        glm::vec3 dir = targetPos - eyePos;
        float horizDist = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        float arrowSpeed = 1.6f;
        float flightTime = horizDist / arrowSpeed;
        dir.y += 0.5f * ArrowEntity::GRAVITY * flightTime;
        dir = glm::normalize(dir);

        mgr.spawnArrow(eyePos, dir, arrowSpeed, static_cast<int>(mob.attackDamage), false);

        // 重置蓄力状态
        mob.isChargingBow = false;
        mob.bowChargeTicks = 0;

        const auto& props = MobRegistry::instance().get(mob.mobType);
        mob.attackCooldown = props.attackCooldownTicks;
    }
}

// ========== CreeperExplodeGoal ==========

void CreeperExplodeGoal::tick(MobEntity& mob, World& world, Player& player, EntityManager&) {
    float distToPlayer = glm::length(mob.position - player.position);

    if (!mob.ignited) {
        mob.aiState = AIState::Exploding;
        mob.fuseTimer = 30;
        mob.ignited = true;
        return;
    }

    mob.fuseTimer--;

    // 如果玩家跑远了就取消
    if (distToPlayer > mob.attackRange * 2.0f) {
        mob.aiState = AIState::Chase;
        mob.fuseTimer = 0;
        mob.ignited = false;
        mob.stateTimer = 200;
        return;
    }

    if (mob.fuseTimer <= 0) {
        // 爆炸！
        getSoundEngine().play(SoundEventId::Explode, mob.position, 1.0f);

        if (distToPlayer < 6.0f) {
            float dmgFactor = 1.0f - (distToPlayer / 6.0f);
            int dmg = static_cast<int>(49.0f * dmgFactor);
            if (dmg > 0) {
                glm::vec3 kb = glm::normalize(player.position - mob.position);
                player.takeDamage(dmg);
                player.velocity += kb * 8.0f;
                player.velocity.y += 4.0f;
            }
        }

        // 破坏周围方块
        int cx = static_cast<int>(std::floor(mob.position.x));
        int cy = static_cast<int>(std::floor(mob.position.y));
        int cz = static_cast<int>(std::floor(mob.position.z));
        for (int dy = -3; dy <= 3; dy++) {
            for (int dz = -3; dz <= 3; dz++) {
                for (int dx = -3; dx <= 3; dx++) {
                    if (dx*dx + dy*dy + dz*dz > 9) continue;
                    int bx = cx + dx, by = cy + dy, bz = cz + dz;
                    BlockId b = world.getBlock(bx, by, bz);
                    if (b != Block::Air && b != Block::Bedrock && b != Block::Water) {
                        world.setBlock(bx, by, bz, Block::Air);
                    }
                }
            }
        }
        mob.alive = false;
    }
}
