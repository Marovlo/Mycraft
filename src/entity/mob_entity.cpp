#include "mob_entity.h"
#include "entity_manager.h"
#include "world/world.h"
#include "player/player.h"
#include "player/inventory.h"
#include "core/block.h"
#include "core/item.h"
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <cstdlib>

// ========== MobRegistry ==========

MobRegistry& MobRegistry::instance() {
    static MobRegistry reg;
    return reg;
}

void MobRegistry::registerDefaults() {
    mobs_[static_cast<int>(MobType::Pig)] = {
        "pig", MobType::Pig, 10, 0.25f, 0.0f, 0.0f, 0, 0.9f, 0.9f, 0.0f, false, false
    };
    mobs_[static_cast<int>(MobType::Cow)] = {
        "cow", MobType::Cow, 10, 0.2f, 0.0f, 0.0f, 0, 0.9f, 1.4f, 0.0f, false, false
    };
    mobs_[static_cast<int>(MobType::Sheep)] = {
        "sheep", MobType::Sheep, 8, 0.23f, 0.0f, 0.0f, 0, 0.9f, 1.3f, 0.0f, false, false
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

// ========== 简化 A* 寻路 ==========

struct PathNode {
    int x, z;
    float g, h;
    int parentIdx;
    float f() const { return g + h; }
};

static bool isWalkable(const World& world, int x, int y, int z, float mobHeight) {
    // 脚下必须是实心方块
    if (!BlockRegistry::instance().isSolid(world.getBlock(x, y - 1, z))) return false;
    // 身体占据的空间必须是空气/非实心
    int headBlocks = static_cast<int>(std::ceil(mobHeight));
    for (int dy = 0; dy < headBlocks; dy++) {
        BlockId b = world.getBlock(x, y + dy, z);
        if (BlockRegistry::instance().isSolid(b)) return false;
    }
    return true;
}

static std::vector<glm::ivec2> findPath(const World& world, glm::ivec3 start, glm::ivec3 goal,
                                          float mobHeight, int maxSteps = 16) {
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
    nodes.push_back({start.x, start.z, 0.0f, h0, -1});
    open.push({h0, 0});

    static const int dx[] = {1, -1, 0, 0, 1, -1, 1, -1};
    static const int dz[] = {0, 0, 1, -1, 1, 1, -1, -1};

    while (!open.empty() && nodes.size() < 512) {
        auto [f, idx] = open.top();
        open.pop();

        auto& cur = nodes[idx];
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

            // 找到可行走的 Y 坐标（允许上下1格台阶）
            int baseY = start.y;
            bool found = false;
            for (int tryY = baseY + 1; tryY >= baseY - 1; tryY--) {
                if (isWalkable(world, nx, tryY, nz, mobHeight)) {
                    found = true;
                    break;
                }
            }
            if (!found) continue;

            float cost = (d < 4) ? 1.0f : 1.414f;
            float ng = cur.g + cost;
            float nh = std::abs(static_cast<float>(goal.x - nx)) +
                       std::abs(static_cast<float>(goal.z - nz));
            int newIdx = static_cast<int>(nodes.size());
            nodes.push_back({nx, nz, ng, nh, idx});
            open.push({ng + nh, newIdx});
        }
    }

    return {};  // 没找到路径
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

    // 物理碰撞
    integrateMotion(world, 0.05f);

    // AI
    tickAI(world, player, mgr);

    // 燃烧
    tickBurning(world);

    // 行走动画
    float speed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    if (speed > 0.01f) {
        walkCycle += speed * 2.5f;
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
            // 选择随机漫步目标
            float angle = (std::rand() % 360) * 3.14159f / 180.0f;
            float dist = 3.0f + (std::rand() % 5);
            wanderTarget = position + glm::vec3(std::cos(angle) * dist, 0, std::sin(angle) * dist);
            hasWanderTarget = true;
        }
        break;

    case AIState::Wander:
        if (hasWanderTarget) {
            moveToward(wanderTarget, moveSpeed);
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
            break;
        }
        // 远离玩家
        glm::vec3 away = position - player.position;
        if (glm::length(away) > 0.01f) {
            away = glm::normalize(away);
            moveToward(position + away * 5.0f, moveSpeed * 1.5f);
        }
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
            if (canSeePlayer(world, player)) {
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
                moveToward(wanderTarget, moveSpeed);
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
            path = findPath(world, start, goal, mobHeight);
            pathIndex = 1;  // 跳过起点
        }

        // 沿路径移动
        if (!path.empty() && pathIndex < static_cast<int>(path.size())) {
            glm::vec3 target(path[pathIndex].x + 0.5f, position.y, path[pathIndex].y + 0.5f);
            moveToward(target, moveSpeed);
            float d = glm::length(glm::vec2(target.x - position.x, target.z - position.z));
            if (d < 0.5f) pathIndex++;
        } else {
            // 直接朝玩家移动
            moveToward(player.position, moveSpeed);
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
        // 骷髅射箭 — 简化：直接对玩家造成远程伤害
        glm::vec3 dir = glm::normalize(player.position + glm::vec3(0, 1.0f, 0) - position);
        player.takeDamage(static_cast<int>(attackDamage));
        player.velocity += dir * 3.0f;
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

    // 击退
    velocity += knockbackDir * 6.0f;
    velocity.y += 4.0f;

    if (hp <= 0) {
        hp = 0;
        isDying = true;
        deathTicks = 0;
    } else {
        // 被动生物受伤后逃跑
        const auto& props = MobRegistry::instance().get(mobType);
        if (!props.isHostile) {
            aiState = AIState::Flee;
            stateTimer = 60 + (std::rand() % 40);  // 3-5秒
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
}

// ========== 燃烧 ==========

void MobEntity::tickBurning(World& world) {
    const auto& props = MobRegistry::instance().get(mobType);
    if (!props.burnInSunlight) return;

    // 检查是否在阳光下（简化：Y坐标以上没有实心方块）
    int bx = static_cast<int>(std::floor(position.x));
    int bz = static_cast<int>(std::floor(position.z));
    int by = static_cast<int>(std::floor(position.y + halfExtents.y));

    bool exposed = true;
    for (int y = by + 1; y < 256; y++) {
        BlockId b = world.getBlock(bx, y, bz);
        if (BlockRegistry::instance().isSolid(b) || BlockRegistry::instance().isLiquid(b)) {
            exposed = false;
            break;
        }
    }

    if (exposed && fireTicks <= 0) {
        fireTicks = 160;  // 8秒燃烧
    }

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

void MobEntity::moveToward(const glm::vec3& target, float speed) {
    glm::vec3 dir = target - position;
    dir.y = 0;
    float dist = glm::length(dir);
    if (dist < 0.01f) return;
    dir /= dist;

    // 更新朝向
    bodyYaw = std::atan2(dir.x, dir.z);

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
