#include "ai_goals.h"
#include "core/block.h"
#include <algorithm>
#include <queue>
#include <unordered_set>

// ========== 寻路辅助函数（从 mob_entity.cpp 迁移） ==========

// 普通生物可行走判断：脚下有实心块，身体空间无实心块
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

// 蜘蛛爬墙判断：贴着任意实心块的空气格都可以站立（包括墙面、天花板）
static bool isClimbable(const World& world, int x, int y, int z, float mobHeight) {
    auto& reg = BlockRegistry::instance();
    // 身体空间必须是空气
    int headBlocks = static_cast<int>(std::ceil(mobHeight));
    for (int dy = 0; dy < headBlocks; dy++) {
        BlockId b = world.getBlock(x, y + dy, z);
        if (reg.isSolid(b) || reg.isLiquid(b)) return false;
    }
    // 贴着任意方向有实心块即可（下、上、前、后、左、右）
    static const int adjX[] = {0, 0, 1, -1, 0, 0};
    static const int adjY[] = {-1, headBlocks, 0, 0, 0, 0};
    static const int adjZ[] = {0, 0, 0, 0, 1, -1};
    for (int i = 0; i < 6; i++) {
        if (reg.isSolid(world.getBlock(x + adjX[i], y + adjY[i], z + adjZ[i]))) return true;
    }
    return false;
}

static bool isSafeDrop(int fromY, int toY) {
    return (fromY - toY) <= 3;
}

static int findWalkableY(const World& world, int x, int z, int baseY,
                          float mobHeight, bool avoidCliffs, bool canClimb = false) {
    if (canClimb) {
        // 蜘蛛：在 baseY 附近上下各搜索，找到可爬行的格子
        if (isClimbable(world, x, baseY, z, mobHeight)) return baseY;
        for (int dy = 1; dy <= 4; dy++) {
            if (isClimbable(world, x, baseY + dy, z, mobHeight)) return baseY + dy;
            if (baseY - dy >= 1 && isClimbable(world, x, baseY - dy, z, mobHeight)) return baseY - dy;
        }
        return -1;
    }
    if (isWalkable(world, x, baseY, z, mobHeight)) return baseY;
    if (isWalkable(world, x, baseY + 1, z, mobHeight)) return baseY + 1;
    // 向上再多找一格（跳台阶）
    if (isWalkable(world, x, baseY + 2, z, mobHeight)) return baseY + 2;
    // 向下最多找 8 格（MC 原版允许最多 8 格高度差的悬崖）
    for (int dy = 1; dy <= 8; dy++) {
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

// A* 寻路（原版 MC 风格：3D 节点，节点数上限，不用代价截断）
struct PathNode {
    int x, y, z;
    float g, h;
    int parentIdx;
    float f() const { return g + h; }
};

static std::vector<glm::ivec3> findPath(const World& world, glm::ivec3 start, glm::ivec3 goal,
                                         float mobHeight, bool avoidCliffs = false,
                                         int maxExpansions = 200, bool canClimb = false) {
    struct NodeCompare {
        bool operator()(const std::pair<float, int>& a, const std::pair<float, int>& b) {
            return a.first > b.first;
        }
    };

    std::vector<PathNode> nodes;
    nodes.reserve(256);
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, NodeCompare> open;

    // closed set：key 包含 Y，避免同 XZ 不同高度的节点互相覆盖
    auto packKey = [](int x, int y, int z) -> int64_t {
        return (static_cast<int64_t>(x & 0xFFFFF) << 40) |
               (static_cast<int64_t>(y & 0xFFF)   << 28) |
               (static_cast<int64_t>(z & 0xFFFFFFF));
    };
    std::unordered_set<int64_t> closed;

    // 启发函数：欧几里得距离（3D）
    auto heuristic = [](int ax, int ay, int az, int bx, int by, int bz) -> float {
        float dx = static_cast<float>(ax - bx);
        float dy = static_cast<float>(ay - by);
        float dz = static_cast<float>(az - bz);
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };

    float h0 = heuristic(start.x, start.y, start.z, goal.x, goal.y, goal.z);
    nodes.push_back({start.x, start.y, start.z, 0.0f, h0, -1});
    open.push({h0, 0});

    // 4 个正交方向 + 4 个对角方向
    static const int dx[] = {1, -1, 0, 0, 1, -1, 1, -1};
    static const int dz[] = {0, 0, 1, -1, 1, 1, -1, -1};

    int expansions = 0;
    while (!open.empty() && expansions < maxExpansions) {
        auto [fVal, idx] = open.top();
        open.pop();

        const PathNode& cur = nodes[idx];
        int64_t key = packKey(cur.x, cur.y, cur.z);
        if (closed.count(key)) continue;
        closed.insert(key);
        expansions++;

        // 到达目标（XZ 匹配即可，Y 由地形决定）
        if (cur.x == goal.x && cur.z == goal.z) {
            std::vector<glm::ivec3> path;
            int i = idx;
            while (i >= 0) {
                path.push_back({nodes[i].x, nodes[i].y, nodes[i].z});
                i = nodes[i].parentIdx;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (int d = 0; d < 8; d++) {
            int nx = cur.x + dx[d];
            int nz = cur.z + dz[d];

            // 对角移动：两个正交方向都不能被实心块堵住（防止穿墙角）
            if (d >= 4) {
                auto& reg = BlockRegistry::instance();
                bool xBlocked = reg.isSolid(world.getBlock(cur.x + dx[d], cur.y, cur.z));
                bool zBlocked = reg.isSolid(world.getBlock(cur.x, cur.y, cur.z + dz[d]));
                if (xBlocked || zBlocked) continue;  // 原版：任一方向被堵就不走对角
            }

            // 找到邻格的可行走 Y（支持爬坡/下坡/爬墙）
            int ny = findWalkableY(world, nx, nz, cur.y, mobHeight, avoidCliffs, canClimb);
            if (ny < 0) continue;

            int64_t nkey = packKey(nx, ny, nz);
            if (closed.count(nkey)) continue;

            // 移动代价：正交=1，对角=1.414，上坡额外+1（MC原版惩罚爬坡）
            float stepCost = (d < 4) ? 1.0f : 1.414f;
            if (ny > cur.y) stepCost += static_cast<float>(ny - cur.y);  // 爬坡惩罚

            float ng = cur.g + stepCost;
            float nh = heuristic(nx, ny, nz, goal.x, goal.y, goal.z);
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
    // 追踪记忆：已经在追踪中时，即使失去视线也继续追 chaseMemoryTicks
    if (chaseMemoryTicks > 0) {
        return dist <= props.detectRange * 1.5f;
    }
    // 初始探测：超出探测范围直接排除
    if (dist > props.detectRange) return false;
    // MC 原版：16 格内不需要视线（贴脸直接感知），超过 16 格才需要视线
    if (dist <= 16.0f) return true;
    return mob.canSeePlayer(world, player);
}

void ChaseGoal::tick(MobEntity& mob, World& world, Player& player, EntityManager&) {
    const auto& props = MobRegistry::instance().get(mob.mobType);
    float distToPlayer = glm::length(mob.position - player.position);

    // 更新追踪记忆计时器
    if (mob.canSeePlayer(world, player)) {
        chaseMemoryTicks = 60;  // 看到玩家后记忆 60 tick（3秒），MC原版行为
    } else if (chaseMemoryTicks > 0) {
        chaseMemoryTicks--;
    }

    if (player.dead || distToPlayer > props.detectRange * 1.5f) {
        mob.aiState = AIState::Idle;
        mob.stateTimer = 40;
        mob.path.clear();
        mob.pathIndex = 0;
        pathFailCooldown = 0;
        chaseMemoryTicks = 0;
        mob.isChasing = false;
        return;
    }

    mob.aiState = AIState::Chase;
    mob.isChasing = true;

    // 寻路失败冷却递减
    if (pathFailCooldown > 0) {
        pathFailCooldown--;
    }

    // ---- repath 触发条件（原版 MC 风格）----
    // 1. 路径走完 → 立即 repath
    // 2. 路径为空且无失败冷却 → 立即 repath
    // 3. 玩家移动超过 4 格（路径过时）→ repath
    // 4. 兜底定时（每 100 tick = 5 秒）→ 防止极端情况卡死
    // 注意：不做"每 N tick 强制 repath"，否则会打断正在执行的绕路路径
    glm::ivec3 curGoal(static_cast<int>(std::floor(player.position.x)),
                       static_cast<int>(std::floor(player.position.y)),
                       static_cast<int>(std::floor(player.position.z)));
    float goalDrift = glm::length(glm::vec3(curGoal - lastGoal));
    bool pathDone = mob.pathIndex >= static_cast<int>(mob.path.size());

    pathUpdateTimer--;
    bool needRepath = pathDone ||
                      (mob.path.empty() && pathFailCooldown <= 0) ||
                      (goalDrift >= 4.0f) ||
                      (pathUpdateTimer <= 0);

    if (needRepath) {
        pathUpdateTimer = 100;
        lastGoal = curGoal;

        // 计算起点：脚底格+1 = 站立格（isWalkable 语义）
        int startFeetY = static_cast<int>(std::floor(mob.position.y - mob.halfExtents.y));
        glm::ivec3 start(static_cast<int>(std::floor(mob.position.x)),
                         startFeetY + 1,
                         static_cast<int>(std::floor(mob.position.z)));

        // 计算终点：玩家脚底格+1
        int goalFeetY = static_cast<int>(std::floor(player.position.y));
        glm::ivec3 goal(static_cast<int>(std::floor(player.position.x)),
                        goalFeetY + 1,
                        static_cast<int>(std::floor(player.position.z)));

        // maxExpansions=200：原版 MC 的节点展开上限
        // canClimb：蜘蛛可以爬墙
        auto newPath = findPath(world, start, goal, mob.mobHeight, false, 200, mob.canClimb);

        printf("[ChaseGoal] repath start=(%d,%d,%d) goal=(%d,%d,%d) pathLen=%d\n",
               start.x, start.y, start.z, goal.x, goal.y, goal.z, (int)newPath.size());
        for (int pi = 0; pi < (int)newPath.size() && pi < 8; pi++) {
            printf("  path[%d]=(%d,%d,%d)\n", pi, newPath[pi].x, newPath[pi].y, newPath[pi].z);
        }

        if (!newPath.empty()) {
            mob.path = std::move(newPath);
            mob.pathIndex = 1;  // 跳过起点（当前位置）
            pathFailCooldown = 0;
        } else {
            mob.path.clear();
            mob.pathIndex = 0;
            pathFailCooldown = 20;  // 1 秒后再试
        }
    }

    // MC 原版追玩家速度约为漫步速度的 1.52 倍
    float chaseSpeed = mob.moveSpeed * 1.52f;

    // ---- 路径跟随（3D 目标点，含 Y 坐标）----
    if (!mob.path.empty() && mob.pathIndex < static_cast<int>(mob.path.size())) {
        const glm::ivec3& wp = mob.path[mob.pathIndex];
        // 目标点：方块中心 XZ，方块站立 Y（wp.y 已是站立格）
        glm::vec3 target(wp.x + 0.5f, static_cast<float>(wp.y), wp.z + 0.5f);
        mob.moveToward(target, chaseSpeed, &world);

        // 到达判定：XZ 距离 < 0.5 格即视为到达当前节点
        float dxz = glm::length(glm::vec2(target.x - mob.position.x, target.z - mob.position.z));
        if (dxz < 0.5f) {
            mob.pathIndex++;
        }
    } else {
        // 路径为空（找不到路或极近距离）：直线朝玩家移动
        mob.moveToward(player.position, chaseSpeed, &world);
    }
}

// ========== MeleeAttackGoal ==========

void MeleeAttackGoal::tick(MobEntity& mob, World&, Player& player, EntityManager&) {
    // 使用 XZ 平面距离判断攻击范围（与 canUse 保持一致）
    glm::vec2 diff(mob.position.x - player.position.x, mob.position.z - player.position.z);
    float distXZ = glm::length(diff);
    if (distXZ <= mob.attackRange) {
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
