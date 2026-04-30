#include "mob_entity.h"
#include "entity_manager.h"
#include "arrow_entity.h"
#include "world/world.h"
#include "player/player.h"
#include "player/inventory.h"
#include "core/block.h"
#include "core/item.h"
#include "audio/sound_engine.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <cstdlib>
#include "world/day_night_cycle.h"

// 静态成员定义
const DayNightCycle* MobEntity::sDayNight = nullptr;

// ========== MobRegistry ==========

MobRegistry& MobRegistry::instance() {
    static MobRegistry reg;
    return reg;
}

void MobRegistry::registerDefaults() {
    mobs_[static_cast<int>(MobType::Pig)] = {
        "pig", MobType::Pig, 10, 0.25f, 0.0f, 0.0f, 0, 0.6f, 0.9f, 0.0f, false, false
    };
    mobs_[static_cast<int>(MobType::Cow)] = {
        "cow", MobType::Cow, 10, 0.2f, 0.0f, 0.0f, 0, 0.7f, 1.4f, 0.0f, false, false
    };
    mobs_[static_cast<int>(MobType::Sheep)] = {
        "sheep", MobType::Sheep, 8, 0.23f, 0.0f, 0.0f, 0, 0.6f, 1.3f, 0.0f, false, false
    };
    mobs_[static_cast<int>(MobType::Chicken)] = {
        "chicken", MobType::Chicken, 4, 0.25f, 0.0f, 0.0f, 0, 0.4f, 0.7f, 0.0f, false, false
    };
    mobs_[static_cast<int>(MobType::Zombie)] = {
        "zombie", MobType::Zombie, 20, 0.23f, 3.0f, 1.5f, 20, 0.6f, 1.95f, 40.0f, true, true
    };
    mobs_[static_cast<int>(MobType::Skeleton)] = {
        "skeleton", MobType::Skeleton, 20, 0.25f, 3.0f, 16.0f, 40, 0.6f, 1.99f, 16.0f, true, true
    };
    mobs_[static_cast<int>(MobType::Spider)] = {
        "spider", MobType::Spider, 16, 0.3f, 2.0f, 1.5f, 20, 1.4f, 0.9f, 16.0f, true, false
    };
    mobs_[static_cast<int>(MobType::Creeper)] = {
        "creeper", MobType::Creeper, 20, 0.25f, 0.0f, 3.0f, 0, 0.6f, 1.7f, 16.0f, true, false
    };
}

// ========== MobEntity 构造 ==========

MobEntity::MobEntity(MobType type) : mobType(type) {
    const auto& props = MobRegistry::instance().get(type);
    hp = props.maxHp;
    maxHp = props.maxHp;
    moveSpeed = props.moveSpeed;
    attackDamage = props.attackDamage;
    attackRange = props.attackRange;
    mobWidth = props.width;
    mobHeight = props.height;
    halfExtents = glm::vec3(mobWidth * 0.5f, mobHeight * 0.5f, mobWidth * 0.5f);
}

// ========== A* 寻路系统 ==========

struct PathNode {
    int x, y, z;   // 方块坐标（y = 脚底所在高度）
    float g, h;
    int parentIdx;
    float f() const { return g + h; }
};

// 检查指定位置是否可行走（脚下实心 + 身体空间无阻挡 + 无液体）
static bool isWalkable(const World& world, int x, int y, int z, float mobHeight) {
    auto& reg = BlockRegistry::instance();
    // 脚下必须是实心方块
    BlockId below = world.getBlock(x, y - 1, z);
    if (!reg.isSolid(below)) return false;
    // 身体占据的空间必须是空气/非实心且非液体
    int headBlocks = static_cast<int>(std::ceil(mobHeight));
    for (int dy = 0; dy < headBlocks; dy++) {
        BlockId b = world.getBlock(x, y + dy, z);
        if (reg.isSolid(b) || reg.isLiquid(b)) return false;
    }
    return true;
}

// 检查从 fromY 到 toY 的落差是否安全（被动生物不走下 3 格以上悬崖）
static bool isSafeDrop(int fromY, int toY) {
    return (fromY - toY) <= 3;
}

// 在指定 XZ 位置、以 baseY 为参考，搜索可行走的 Y 坐标
// 允许上1格台阶、下3格（敌对生物不限制下落）
static int findWalkableY(const World& world, int x, int z, int baseY,
                          float mobHeight, bool avoidCliffs) {
    // 优先检查同层
    if (isWalkable(world, x, baseY, z, mobHeight)) return baseY;
    // 上1格台阶
    if (isWalkable(world, x, baseY + 1, z, mobHeight)) return baseY + 1;
    // 向下搜索（最多下落4格找到地面）
    for (int dy = 1; dy <= 4; dy++) {
        int tryY = baseY - dy;
        if (tryY < 1) break;
        if (isWalkable(world, x, tryY, z, mobHeight)) {
            if (avoidCliffs && !isSafeDrop(baseY, tryY)) return -1;
            return tryY;
        }
    }
    return -1;  // 不可达
}

// A* 寻路：返回 XZ 路径坐标序列
// avoidWater: 陆地生物避开水域
// avoidCliffs: 被动生物避开高悬崖
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
            // 回溯路径
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

            // 以当前节点的 Y 为基准，搜索邻居可行走的 Y
            int ny = findWalkableY(world, nx, nz, cur.y, mobHeight, avoidCliffs);
            if (ny < 0) continue;

            // 对角线移动时检查两个相邻正交方向是否可通行（防止穿墙角）
            // dx/dz 索引 4-7 对应: (1,1), (-1,1), (1,-1), (-1,-1)
            // 需要检查两个正交分量方向是否被墙挡住
            if (d >= 4) {
                // 分解对角线为两个正交分量
                int compX = dx[d]; // 对角线的 X 分量
                int compZ = dz[d]; // 对角线的 Z 分量
                auto& reg = BlockRegistry::instance();
                // 检查 X 方向邻居和 Z 方向邻居是否都是实心（两面墙夹角）
                bool xBlocked = reg.isSolid(world.getBlock(cur.x + compX, cur.y, cur.z));
                bool zBlocked = reg.isSolid(world.getBlock(cur.x, cur.y, cur.z + compZ));
                if (xBlocked && zBlocked) {
                    continue;  // 对角线被两面墙挡住
                }
            }

            // 上台阶额外代价
            float stepCost = 0.0f;
            if (ny > cur.y) stepCost = 0.5f;  // 上台阶稍贵

            float cost = ((d < 4) ? 1.0f : 1.414f) + stepCost;
            float ng = cur.g + cost;
            float nh = std::abs(static_cast<float>(goal.x - nx)) +
                       std::abs(static_cast<float>(goal.z - nz));
            int newIdx = static_cast<int>(nodes.size());
            nodes.push_back({nx, ny, nz, ng, nh, idx});
            open.push({ng + nh, newIdx});
        }
    }

    return {};  // 没找到路径
}

// 验证漫步目标是否安全可达（被动生物用）
static bool isWanderTargetSafe(const World& world, const glm::vec3& from,
                                const glm::vec3& target, float mobHeight) {
    int tx = static_cast<int>(std::floor(target.x));
    int tz = static_cast<int>(std::floor(target.z));
    int fromY = static_cast<int>(std::floor(from.y - mobHeight * 0.5f));

    // 目标位置必须可行走且不是悬崖
    int ty = findWalkableY(world, tx, tz, fromY, mobHeight, true);
    return ty >= 0;
}

// ========== MobEntity::tick ==========

void MobEntity::tick(World& world, EntityManager& mgr,
                     Player& player, Inventory& inventory) {
    if (!alive) return;
    tickCount++;

    // 死亡动画
    if (isDying) {
        deathTicks++;
        if (deathTicks >= 20) {  // 1秒后消失
            // 生成掉落物
            spawnLoot(world, mgr);
            alive = false;
        }
        return;
    }

    // 无敌帧递减
    if (invulnerableTicks > 0) invulnerableTicks--;
    if (hurtTicks > 0) hurtTicks--;
    if (attackCooldown > 0) attackCooldown--;
    if (panicTicks > 0) panicTicks--;

    // MC 原版：生物随机环境叫声（每 80~200 tick 随机触发一次）
    if (ambientSoundTimer_ <= 0) {
        // 播放环境叫声
        SoundEventId ambientSound = SoundEventId::Count; // 无效值表示无音效
        switch (mobType) {
            case MobType::Cow:      ambientSound = SoundEventId::MobCowSay; break;
            case MobType::Pig:      ambientSound = SoundEventId::MobPigSay; break;
            case MobType::Sheep:    ambientSound = SoundEventId::MobSheepSay; break;
            case MobType::Chicken:  ambientSound = SoundEventId::MobChickenSay; break;
            case MobType::Zombie:   ambientSound = SoundEventId::MobZombieSay; break;
            case MobType::Skeleton: ambientSound = SoundEventId::MobSkeletonSay; break;
            case MobType::Spider:   ambientSound = SoundEventId::MobSpiderSay; break;
            default: break;
        }
        if (ambientSound != SoundEventId::Count) {
            getSoundEngine().play(ambientSound, position, 0.4f);
        }
        // 重置计时器：MC 原版约 80~200 tick 间隔
        ambientSoundTimer_ = 80 + (tickCount % 120);
    } else {
        ambientSoundTimer_--;
    }

    // 重力
    if (!onGround) {
        velocity.y -= 28.0f * 0.05f;  // GRAVITY * dt (1 tick = 0.05s)
    }

    // 摩擦力
    if (onGround) {
        velocity.x *= 0.6f;
        velocity.z *= 0.6f;
    } else {
        velocity.x *= 0.91f;
        velocity.z *= 0.91f;
    }

    // 鸡缓降
    if (mobType == MobType::Chicken && velocity.y < -2.0f) {
        velocity.y = -2.0f;
    }

    // 蜘蛛爬墙：碰到垂直方块面时向上攀爬
    if (mobType == MobType::Spider && !onGround) {
        // 检测蜘蛛水平方向是否紧贴方块（碰撞检测）
        float feetY = position.y - halfExtents.y;
        int bx = static_cast<int>(std::floor(position.x));
        int by = static_cast<int>(std::floor(feetY));
        int bz = static_cast<int>(std::floor(position.z));
        bool touchingWall = false;
        // 检查四个水平方向是否有实心方块
        float hw = halfExtents.x + 0.05f;  // 略微扩大检测范围
        int checkPositions[][2] = {
            {static_cast<int>(std::floor(position.x + hw)), bz},
            {static_cast<int>(std::floor(position.x - hw)), bz},
            {bx, static_cast<int>(std::floor(position.z + hw))},
            {bx, static_cast<int>(std::floor(position.z - hw))}
        };
        for (auto& cp : checkPositions) {
            // 检查身体高度范围内是否有实心方块
            for (int dy = 0; dy <= static_cast<int>(std::ceil(mobHeight)); dy++) {
                if (BlockRegistry::instance().isSolid(world.getBlock(cp[0], by + dy, cp[1]))) {
                    touchingWall = true;
                    break;
                }
            }
            if (touchingWall) break;
        }
        if (touchingWall) {
            // 攀爬：抵消重力，给予向上速度
            if (velocity.y < 0) velocity.y = 0;
            velocity.y = moveSpeed * 4.0f;  // 攀爬速度等于水平移速
        }
    }

    // 物理碰撞
    integrateMotion(world, 0.05f);

    // AI
    tickAI(world, player, mgr);

    // 燃烧
    tickBurning(world);

    // 行走动画：正常时慢摆，恐慌时快摆
    float speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    if (speed > 0.01f) {
        float animSpeed = (panicTicks > 0) ? speed * 3.5f : speed * 1.2f;
        walkCycle += animSpeed;
        if (walkCycle > 6.2831853f) walkCycle -= 6.2831853f;
    }

    // 虚空伤害
    if (position.y < -64.0f) {
        alive = false;
    }
}

// ========== AI ==========

void MobEntity::tickAI(World& world, Player& player, EntityManager& mgr) {
    const auto& props = MobRegistry::instance().get(mobType);
    if (props.isHostile) {
        tickHostileAI(world, player, mgr);
    } else {
        tickPassiveAI(world, player);
    }
}

void MobEntity::tickPassiveAI(World& world, Player& player) {
    stateTimer--;

    switch (aiState) {
    case AIState::Idle:
        if (stateTimer <= 0) {
            aiState = AIState::Wander;
            stateTimer = 40 + (std::rand() % 120);  // 2-8秒
            // 选择随机漫步目标，验证安全性
            for (int attempt = 0; attempt < 5; attempt++) {
                float angle = (std::rand() % 360) * 3.14159f / 180.0f;
                float dist = 3.0f + (std::rand() % 5);
                glm::vec3 candidate = position + glm::vec3(std::cos(angle) * dist, 0, std::sin(angle) * dist);
                if (isWanderTargetSafe(world, position, candidate, mobHeight)) {
                    wanderTarget = candidate;
                    hasWanderTarget = true;
                    break;
                }
            }
            if (!hasWanderTarget) {
                // 所有候选目标都不安全，回到Idle
                aiState = AIState::Idle;
                stateTimer = 40 + (std::rand() % 80);
            }
        }
        break;

    case AIState::Wander:
        if (hasWanderTarget) {
            moveToward(wanderTarget, moveSpeed, &world);
            float dx = wanderTarget.x - position.x;
            float dz = wanderTarget.z - position.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist < 1.0f || stateTimer <= 0) {
                aiState = AIState::Idle;
                stateTimer = 40 + (std::rand() % 80);  // 2-6秒
                hasWanderTarget = false;
            }
        } else {
            aiState = AIState::Idle;
            stateTimer = 40 + (std::rand() % 80);
        }
        break;

    case AIState::Flee: {
        if (stateTimer <= 0) {
            aiState = AIState::Idle;
            stateTimer = 40 + (std::rand() % 60);
            panicTicks = 0;
            break;
        }
        // 逃跑：远离玩家方向 + 随机偏移，模拟惊慌乱跑
        glm::vec3 away = position - player.position;
        if (glm::length(away) > 0.01f) {
            away = glm::normalize(away);
            // 每隔一段时间随机偏转逃跑方向，模拟乱跑
            if (stateTimer % 15 == 0) {
                float randAngle = ((std::rand() % 120) - 60) * 3.14159f / 180.0f;
                float cosA = std::cos(randAngle), sinA = std::sin(randAngle);
                float nx = away.x * cosA - away.z * sinA;
                float nz = away.x * sinA + away.z * cosA;
                away.x = nx;
                away.z = nz;
            }
        } else {
            // 如果和玩家重叠，随机方向逃跑
            float angle = (std::rand() % 360) * 3.14159f / 180.0f;
            away = glm::vec3(std::cos(angle), 0, std::sin(angle));
        }
        // 恐慌时用更快的速度逃跑
        float fleeSpeed = (panicTicks > 0) ? moveSpeed * 2.5f : moveSpeed * 1.5f;
        moveToward(position + away * 5.0f, fleeSpeed, &world);
        break;
    }

    default:
        aiState = AIState::Idle;
        stateTimer = 20;
        break;
    }
}

void MobEntity::tickHostileAI(World& world, Player& player, EntityManager& mgr) {
    const auto& props = MobRegistry::instance().get(mobType);
    float distToPlayer = glm::length(position - player.position);

    stateTimer--;

    switch (aiState) {
    case AIState::Idle:
    case AIState::Wander:
        // 检测玩家
        if (distToPlayer <= props.detectRange && !player.dead) {
            // 蜘蛛白天中立：白天不主动追踪，除非被攻击过（provoked）
            bool shouldChase = true;
            if (mobType == MobType::Spider && sDayNight && sDayNight->isDay()) {
                shouldChase = provoked;  // 白天只有被攻击后才追踪
            }
            if (shouldChase && canSeePlayer(world, player)) {
                aiState = AIState::Chase;
                stateTimer = 200;
                break;
            }
        }
        // 普通漫步
        if (aiState == AIState::Idle && stateTimer <= 0) {
            aiState = AIState::Wander;
            stateTimer = 40 + (std::rand() % 120);
            float angle = (std::rand() % 360) * 3.14159f / 180.0f;
            float dist = 3.0f + (std::rand() % 5);
            wanderTarget = position + glm::vec3(std::cos(angle) * dist, 0, std::sin(angle) * dist);
            hasWanderTarget = true;
        } else if (aiState == AIState::Wander) {
            if (hasWanderTarget) {
                moveToward(wanderTarget, moveSpeed, &world);
                float d = glm::length(glm::vec2(wanderTarget.x - position.x, wanderTarget.z - position.z));
                if (d < 1.0f || stateTimer <= 0) {
                    aiState = AIState::Idle;
                    stateTimer = 40 + (std::rand() % 80);
                    hasWanderTarget = false;
                }
            }
        }
        break;

    case AIState::Chase: {
        if (player.dead || distToPlayer > props.detectRange * 1.5f) {
            aiState = AIState::Idle;
            stateTimer = 40;
            path.clear();
            break;
        }

        // 更新寻路
        pathUpdateTimer--;
        if (pathUpdateTimer <= 0 || path.empty()) {
            pathUpdateTimer = 20;  // 每秒更新一次
            glm::ivec3 start(static_cast<int>(std::floor(position.x)),
                             static_cast<int>(std::floor(position.y - halfExtents.y)),
                             static_cast<int>(std::floor(position.z)));
            glm::ivec3 goal(static_cast<int>(std::floor(player.position.x)),
                            static_cast<int>(std::floor(player.position.y)),
                            static_cast<int>(std::floor(player.position.z)));
            path = findPath(world, start, goal, mobHeight, false, 24);
            pathIndex = 1;  // 跳过起点
        }

        // 沿路径移动
        if (!path.empty() && pathIndex < static_cast<int>(path.size())) {
            glm::vec3 target(path[pathIndex].x + 0.5f, position.y, path[pathIndex].y + 0.5f);
            moveToward(target, moveSpeed, &world);
            float d = glm::length(glm::vec2(target.x - position.x, target.z - position.z));
            if (d < 0.5f) pathIndex++;
        } else {
            // 直接朝玩家移动
            moveToward(player.position, moveSpeed, &world);
        }

        // 进入攻击范围
        if (distToPlayer <= attackRange && attackCooldown <= 0) {
            if (mobType == MobType::Creeper) {
                aiState = AIState::Exploding;
                fuseTimer = 30;  // 1.5秒
                ignited = true;
            } else {
                aiState = AIState::Attack;
                stateTimer = 10;
            }
        }
        break;
    }

    case AIState::Attack: {
        if (stateTimer <= 0) {
            tickCombat(player, mgr);
            aiState = AIState::Cooldown;
            const auto& p = MobRegistry::instance().get(mobType);
            stateTimer = p.attackCooldownTicks;
            attackCooldown = p.attackCooldownTicks;
        }
        break;
    }

    case AIState::Cooldown:
        if (stateTimer <= 0) {
            aiState = AIState::Chase;
            stateTimer = 200;
        }
        break;

    case AIState::Exploding: {
        fuseTimer--;
        // 苦力怕膨胀：如果玩家跑远了就取消
        if (distToPlayer > attackRange * 2.0f) {
            aiState = AIState::Chase;
            fuseTimer = 0;
            ignited = false;
            stateTimer = 200;
            break;
        }
        if (fuseTimer <= 0) {
            // 爆炸！
            // 播放爆炸音效
            getSoundEngine().play(SoundEventId::Explode, position, 1.0f);

            // 对玩家造成范围伤害
            if (distToPlayer < 6.0f) {
                float dmgFactor = 1.0f - (distToPlayer / 6.0f);
                int dmg = static_cast<int>(49.0f * dmgFactor);  // MC苦力怕最大49伤害
                if (dmg > 0) {
                    glm::vec3 kb = glm::normalize(player.position - position);
                    player.takeDamage(dmg);
                    player.velocity += kb * 8.0f;
                    player.velocity.y += 4.0f;
                }
            }
            // 破坏周围方块（3格半径）
            int cx = static_cast<int>(std::floor(position.x));
            int cy = static_cast<int>(std::floor(position.y));
            int cz = static_cast<int>(std::floor(position.z));
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
            alive = false;  // 苦力怕自毁
        }
        break;
    }

    default:
        aiState = AIState::Idle;
        stateTimer = 20;
        break;
    }
}

// ========== 战斗 ==========

void MobEntity::tickCombat(Player& player, EntityManager& mgr) {
    float dist = glm::length(position - player.position);
    if (dist > attackRange * 1.5f) return;

    if (mobType == MobType::Skeleton) {
        // 骷髅射箭 — 生成箭矢实体，沿抛物线飞向玩家
        // 发射位置：骷髅眼睛高度
        glm::vec3 eyePos = position + glm::vec3(0, mobHeight * 0.4f, 0);
        // 瞄准玩家身体中心（加一点高度补偿重力下坠）
        glm::vec3 targetPos = player.position + glm::vec3(0, 1.0f, 0);
        glm::vec3 dir = targetPos - eyePos;
        float horizDist = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        // 补偿重力：根据水平距离给一个向上的偏移
        // MC 原版骷髅箭速约 1.6 blocks/tick，这里用类似值
        float arrowSpeed = 1.6f;
        float flightTime = horizDist / arrowSpeed;
        // 补偿重力下坠：额外向上瞄 (0.5 * g * t)
        dir.y += 0.5f * ArrowEntity::GRAVITY * flightTime;
        dir = glm::normalize(dir);

        mgr.spawnArrow(eyePos, dir, arrowSpeed, static_cast<int>(attackDamage), false);
    } else {
        // 近战攻击
        if (dist <= attackRange) {
            glm::vec3 kb = glm::normalize(player.position - position);
            player.takeDamage(static_cast<int>(attackDamage));
            player.velocity += kb * 5.0f;
            player.velocity.y += 4.0f;
        }
    }
}

void MobEntity::takeDamage(int amount, const glm::vec3& knockbackDir) {
    if (invulnerableTicks > 0 || isDying) return;

    hp -= amount;
    hurtTicks = 10;
    invulnerableTicks = 10;  // 0.5秒无敌

    // 击退：给一个明显的击飞感
    velocity += knockbackDir * 8.0f;
    velocity.y += 5.0f;

    // 播放生物受伤音效
    SoundEventId hurtSound = SoundEventId::DamageHit;
    switch (mobType) {
        case MobType::Cow:      hurtSound = SoundEventId::MobCowHurt; break;
        case MobType::Zombie:   hurtSound = SoundEventId::MobZombieHurt; break;
        case MobType::Skeleton: hurtSound = SoundEventId::MobSkeletonHurt; break;
        default: break; // 其他生物用通用受伤音效
    }
    getSoundEngine().play(hurtSound, position, 0.5f);

    if (hp <= 0) {
        hp = 0;
        isDying = true;
        deathTicks = 0;

        // 播放生物死亡音效
        SoundEventId deathSound = SoundEventId::DamageHit;
        switch (mobType) {
            case MobType::Pig:      deathSound = SoundEventId::MobPigDeath; break;
            case MobType::Zombie:   deathSound = SoundEventId::MobZombieDeath; break;
            case MobType::Skeleton: deathSound = SoundEventId::MobSkeletonDeath; break;
            case MobType::Spider:   deathSound = SoundEventId::MobSpiderDeath; break;
            case MobType::Creeper:  deathSound = SoundEventId::MobCreeperDeath; break;
            default: break;
        }
        getSoundEngine().play(deathSound, position, 0.6f);
    } else {
        // 被动生物受伤后逃跑 + 恐慌加速
        const auto& props = MobRegistry::instance().get(mobType);
        if (!props.isHostile) {
            aiState = AIState::Flee;
            stateTimer = 60 + (std::rand() % 40);  // 3-5秒
            panicTicks = 80 + (std::rand() % 40);  // 4-6秒恐慌（加速摆腿+加速移动）
        }
        // 蜘蛛被攻击后标记为被激怒，白天也会追踪玩家
        if (mobType == MobType::Spider) {
            provoked = true;
        }
    }
}

// ========== 掉落物 ==========

void MobEntity::spawnLoot(World& world, EntityManager& mgr) {
    auto spawn = [&](ItemId id, int minCount, int maxCount) {
        int count = minCount + (maxCount > minCount ? (std::rand() % (maxCount - minCount + 1)) : 0);
        if (count > 0) {
            mgr.spawnItem(position + glm::vec3(0, 0.5f, 0), {id, static_cast<uint16_t>(count), 0});
        }
    };

    switch (mobType) {
    case MobType::Pig:
        spawn(Item::RawPorkchop, 1, 3);
        break;
    case MobType::Cow:
        spawn(Item::RawBeef, 1, 3);
        spawn(Item::Leather, 0, 2);
        break;
    case MobType::Sheep:
        spawn(Item::WhiteWool, 1, 1);
        break;
    case MobType::Chicken:
        spawn(Item::RawChicken, 1, 1);
        spawn(Item::Feather, 0, 2);
        break;
    case MobType::Zombie:
        // 僵尸掉落腐肉（暂用生牛排代替，因为没有腐肉物品）
        break;
    case MobType::Skeleton:
        spawn(Item::Bone, 0, 2);
        spawn(Item::Arrow, 0, 2);
        break;
    case MobType::Spider:
        spawn(Item::StringItem, 0, 2);
        spawn(Item::SpiderEye, 0, 1);
        break;
    case MobType::Creeper:
        spawn(Item::Gunpowder, 0, 2);
        break;
    default:
        break;
    }

    // MC 原版经验球掉落（只有被玩家击杀才掉经验）
    int xpDrop = 0;
    switch (mobType) {
    case MobType::Pig:     xpDrop = 1 + std::rand() % 3; break; // 1-3
    case MobType::Cow:     xpDrop = 1 + std::rand() % 3; break; // 1-3
    case MobType::Sheep:   xpDrop = 1 + std::rand() % 3; break; // 1-3
    case MobType::Chicken: xpDrop = 1 + std::rand() % 3; break; // 1-3
    case MobType::Zombie:  xpDrop = 5;                    break; // 5
    case MobType::Skeleton:xpDrop = 5;                    break; // 5
    case MobType::Spider:  xpDrop = 5;                    break; // 5
    case MobType::Creeper: xpDrop = 5;                    break; // 5
    default: break;
    }
    if (xpDrop > 0) {
        mgr.spawnXPOrbs(position, xpDrop);
    }
}

// ========== 燃烧 ==========

void MobEntity::tickBurning(World& world) {
    const auto& props = MobRegistry::instance().get(mobType);
    if (!props.burnInSunlight) return;

    // MC原版燃烧条件：白天 + 头顶无遮挡
    // 夜晚不燃烧，有遮挡不燃烧
    bool isDaytime = sDayNight ? sDayNight->isDay() : true;

    if (isDaytime) {
        // 检查头顶是否有遮挡
        // 优化：不遍历到256，而是从头顶向上最多搜索64格
        // 大多数情况下很快就能找到遮挡或确认暴露
        int bx = static_cast<int>(std::floor(position.x));
        int bz = static_cast<int>(std::floor(position.z));
        int by = static_cast<int>(std::floor(position.y + halfExtents.y));
        int maxY = std::min(by + 64, 255); // 最多向上检查64格

        bool exposed = true;
        for (int y = by + 1; y <= maxY; y++) {
            BlockId b = world.getBlock(bx, y, bz);
            if (BlockRegistry::instance().isSolid(b) || BlockRegistry::instance().isLiquid(b)) {
                exposed = false;
                break;
            }
        }

        if (exposed && fireTicks <= 0) {
            fireTicks = 160;  // 8秒燃烧
        }
    }
    // 夜晚不会新增燃烧，但已有的燃烧继续消耗（MC原版行为）

    if (fireTicks > 0) {
        fireTicks--;
        fireTimer++;
        if (fireTimer >= 20) {  // 每秒1点伤害
            fireTimer = 0;
            takeDamage(1, glm::vec3(0));
        }
    }
}

// ========== 移动辅助 ==========

bool MobEntity::canSeePlayer(const World& world, const Player& player) const {
    // 简化视线检查：直线距离内没有实心方块
    glm::vec3 start = position + glm::vec3(0, halfExtents.y * 0.8f, 0);
    glm::vec3 end = player.getEyePosition();
    glm::vec3 dir = end - start;
    float dist = glm::length(dir);
    if (dist < 0.01f) return true;
    dir /= dist;

    for (float t = 0.5f; t < dist; t += 0.5f) {
        glm::vec3 p = start + dir * t;
        int bx = static_cast<int>(std::floor(p.x));
        int by = static_cast<int>(std::floor(p.y));
        int bz = static_cast<int>(std::floor(p.z));
        if (BlockRegistry::instance().isSolid(world.getBlock(bx, by, bz))) {
            return false;
        }
    }
    return true;
}

void MobEntity::moveToward(const glm::vec3& target, float speed, World* world) {
    glm::vec3 dir = target - position;
    dir.y = 0;
    float dist = glm::length(dir);
    if (dist < 0.01f) return;
    dir /= dist;

    // 更新朝向
    bodyYaw = std::atan2(dir.x, dir.z);

    // 自动跳跃：检测前方1格高障碍
    if (onGround && world) {
        // 前方脚底位置
        float feetY = position.y - halfExtents.y;
        int checkX = static_cast<int>(std::floor(position.x + dir.x * 0.6f));
        int checkZ = static_cast<int>(std::floor(position.z + dir.z * 0.6f));
        int checkY = static_cast<int>(std::floor(feetY));
        // 前方脚底高度有实心方块（1格高障碍）
        if (BlockRegistry::instance().isSolid(world->getBlock(checkX, checkY, checkZ))) {
            // 障碍上方必须是空的（可以跳上去）
            int headBlocks = static_cast<int>(std::ceil(mobHeight));
            bool canJump = true;
            for (int dy = 1; dy <= headBlocks; dy++) {
                if (BlockRegistry::instance().isSolid(world->getBlock(checkX, checkY + dy, checkZ))) {
                    canJump = false;
                    break;
                }
            }
            if (canJump) {
                tryJump();
            }
        }
    }

    // 应用速度
    float accel = onGround ? speed * 4.0f : speed * 1.0f;
    velocity.x += dir.x * accel;
    velocity.z += dir.z * accel;

    // 限速
    float hSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    float maxSpeed = speed * 4.317f;  // 转换为 blocks/sec
    if (hSpeed > maxSpeed) {
        float scale = maxSpeed / hSpeed;
        velocity.x *= scale;
        velocity.z *= scale;
    }
}

void MobEntity::moveAlongPath(float speed) {
    if (path.empty() || pathIndex >= static_cast<int>(path.size())) return;
    glm::vec3 target(path[pathIndex].x + 0.5f, position.y, path[pathIndex].y + 0.5f);
    moveToward(target, speed);
}

bool MobEntity::tryJump() {
    if (onGround) {
        velocity.y = 9.0f;
        return true;
    }
    return false;
}


