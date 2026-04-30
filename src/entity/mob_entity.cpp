#include "mob_entity.h"
#include "ai_goals.h"
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
#include <cstdlib>
#include <random>
#include "world/day_night_cycle.h"

// 线程局部 RNG（替代 std::rand，线程安全且随机质量更好）
static thread_local std::mt19937 sMobRng{std::random_device{}()};
static int mobRandInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(sMobRng);
}
static float mobRandFloat() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(sMobRng);
}

// 静态成员定义
const DayNightCycle* MobEntity::sDayNight = nullptr;

// ========== MobRegistry ==========

MobRegistry& MobRegistry::instance() {
    static MobRegistry reg;
    return reg;
}

void MobRegistry::registerDefaults() {
    mobs_.clear();
    mobs_.resize(static_cast<int>(MobType::COUNT));

    // ========== 被动生物 ==========

    mobs_[static_cast<int>(MobType::Pig)] = {
        .name = "pig",
        .type = MobType::Pig,
        .maxHp = 10,
        .moveSpeed = 0.25f,
        .attackDamage = 0.0f,
        .attackRange = 0.0f,
        .attackCooldownTicks = 0,
        .width = 0.6f, .height = 0.9f,
        .detectRange = 0.0f,
        .isHostile = false,
        .burnInSunlight = false,
        .lootTable = {{Item::RawPorkchop, 1, 3, 1.0f}},
        .xpDropMin = 1, .xpDropMax = 3,
        .sounds = {
            static_cast<uint16_t>(SoundEventId::MobPigSay),
            0xFFFF,  // 猪没有专用受伤音效
            static_cast<uint16_t>(SoundEventId::MobPigDeath)
        },
        .aiGoalFactories = {
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<WanderGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<FleeGoal>(); },
        },
    };

    mobs_[static_cast<int>(MobType::Cow)] = {
        .name = "cow",
        .type = MobType::Cow,
        .maxHp = 10,
        .moveSpeed = 0.2f,
        .attackDamage = 0.0f,
        .attackRange = 0.0f,
        .attackCooldownTicks = 0,
        .width = 0.7f, .height = 1.4f,
        .detectRange = 0.0f,
        .isHostile = false,
        .burnInSunlight = false,
        .lootTable = {
            {Item::RawBeef, 1, 3, 1.0f},
            {Item::Leather, 0, 2, 1.0f},
        },
        .xpDropMin = 1, .xpDropMax = 3,
        .sounds = {
            static_cast<uint16_t>(SoundEventId::MobCowSay),
            static_cast<uint16_t>(SoundEventId::MobCowHurt),
            0xFFFF,
        },
        .aiGoalFactories = {
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<WanderGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<FleeGoal>(); },
        },
    };

    mobs_[static_cast<int>(MobType::Sheep)] = {
        .name = "sheep",
        .type = MobType::Sheep,
        .maxHp = 8,
        .moveSpeed = 0.23f,
        .attackDamage = 0.0f,
        .attackRange = 0.0f,
        .attackCooldownTicks = 0,
        .width = 0.6f, .height = 1.3f,
        .detectRange = 0.0f,
        .isHostile = false,
        .burnInSunlight = false,
        .lootTable = {{Item::WhiteWool, 1, 1, 1.0f}},
        .xpDropMin = 1, .xpDropMax = 3,
        .sounds = {
            static_cast<uint16_t>(SoundEventId::MobSheepSay),
            0xFFFF,
            0xFFFF,
        },
        .aiGoalFactories = {
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<WanderGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<FleeGoal>(); },
        },
    };

    mobs_[static_cast<int>(MobType::Chicken)] = {
        .name = "chicken",
        .type = MobType::Chicken,
        .maxHp = 4,
        .moveSpeed = 0.25f,
        .attackDamage = 0.0f,
        .attackRange = 0.0f,
        .attackCooldownTicks = 0,
        .width = 0.4f, .height = 0.7f,
        .detectRange = 0.0f,
        .isHostile = false,
        .burnInSunlight = false,
        .lootTable = {
            {Item::RawChicken, 1, 1, 1.0f},
            {Item::Feather, 0, 2, 1.0f},
        },
        .xpDropMin = 1, .xpDropMax = 3,
        .sounds = {
            static_cast<uint16_t>(SoundEventId::MobChickenSay),
            0xFFFF,
            0xFFFF,
        },
        .aiGoalFactories = {
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<WanderGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<FleeGoal>(); },
        },
    };

    // ========== 敌对生物 ==========

    mobs_[static_cast<int>(MobType::Zombie)] = {
        .name = "zombie",
        .type = MobType::Zombie,
        .maxHp = 20,
        .moveSpeed = 0.23f,
        .attackDamage = 3.0f,
        .attackRange = 1.5f,
        .attackCooldownTicks = 20,
        .width = 0.6f, .height = 1.95f,
        .detectRange = 40.0f,
        .isHostile = true,
        .burnInSunlight = true,
        .lootTable = {},  // 僵尸暂无掉落物（腐肉未实现）
        .xpDropMin = 5, .xpDropMax = 5,
        .sounds = {
            static_cast<uint16_t>(SoundEventId::MobZombieSay),
            static_cast<uint16_t>(SoundEventId::MobZombieHurt),
            static_cast<uint16_t>(SoundEventId::MobZombieDeath),
        },
        .aiGoalFactories = {
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<WanderGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<ChaseGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<MeleeAttackGoal>(); },
        },
    };

    mobs_[static_cast<int>(MobType::Skeleton)] = {
        .name = "skeleton",
        .type = MobType::Skeleton,
        .maxHp = 20,
        .moveSpeed = 0.25f,
        .attackDamage = 3.0f,
        .attackRange = 16.0f,
        .attackCooldownTicks = 40,
        .width = 0.6f, .height = 1.99f,
        .detectRange = 16.0f,
        .isHostile = true,
        .burnInSunlight = true,
        .lootTable = {
            {Item::Bone, 0, 2, 1.0f},
            {Item::Arrow, 0, 2, 1.0f},
        },
        .xpDropMin = 5, .xpDropMax = 5,
        .sounds = {
            static_cast<uint16_t>(SoundEventId::MobSkeletonSay),
            static_cast<uint16_t>(SoundEventId::MobSkeletonHurt),
            static_cast<uint16_t>(SoundEventId::MobSkeletonDeath),
        },
        .aiGoalFactories = {
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<WanderGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<ChaseGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<RangedAttackGoal>(); },
        },
    };

    mobs_[static_cast<int>(MobType::Spider)] = {
        .name = "spider",
        .type = MobType::Spider,
        .maxHp = 16,
        .moveSpeed = 0.3f,
        .attackDamage = 2.0f,
        .attackRange = 1.5f,
        .attackCooldownTicks = 20,
        .width = 1.4f, .height = 0.9f,
        .detectRange = 16.0f,
        .isHostile = true,
        .burnInSunlight = false,
        .lootTable = {
            {Item::StringItem, 0, 2, 1.0f},
            {Item::SpiderEye, 0, 1, 0.33f},
        },
        .xpDropMin = 5, .xpDropMax = 5,
        .sounds = {
            static_cast<uint16_t>(SoundEventId::MobSpiderSay),
            0xFFFF,
            static_cast<uint16_t>(SoundEventId::MobSpiderDeath),
        },
        .aiGoalFactories = {
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<WanderGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<SpiderNeutralGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<ChaseGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<MeleeAttackGoal>(); },
        },
    };

    mobs_[static_cast<int>(MobType::Creeper)] = {
        .name = "creeper",
        .type = MobType::Creeper,
        .maxHp = 20,
        .moveSpeed = 0.25f,
        .attackDamage = 0.0f,
        .attackRange = 3.0f,
        .attackCooldownTicks = 0,
        .width = 0.6f, .height = 1.7f,
        .detectRange = 16.0f,
        .isHostile = true,
        .burnInSunlight = false,
        .lootTable = {{Item::Gunpowder, 0, 2, 1.0f}},
        .xpDropMin = 5, .xpDropMax = 5,
        .sounds = {
            0xFFFF,  // 苦力怕没有环境叫声
            0xFFFF,
            static_cast<uint16_t>(SoundEventId::MobCreeperDeath),
        },
        .aiGoalFactories = {
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<WanderGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<ChaseGoal>(); },
            []() -> std::unique_ptr<AIGoal> { return std::make_unique<CreeperExplodeGoal>(); },
        },
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

    // 从属性中创建 AI 目标组件实例
    for (const auto& factory : props.aiGoalFactories) {
        aiGoals_.push_back(factory());
    }
}

// ========== MobEntity::tick ==========

void MobEntity::tick(World& world, EntityManager& mgr,
                     Player& player, Inventory& inventory) {
    if (!alive) return;
    tickCount++;

    // 死亡动画
    if (isDying) {
        deathTicks++;
        if (deathTicks >= 20) {
            spawnLoot(world, mgr);
            alive = false;
        }
        return;
    }

    // 计时器递减
    if (invulnerableTicks > 0) invulnerableTicks--;
    if (hurtTicks > 0) hurtTicks--;
    if (attackCooldown > 0) attackCooldown--;
    if (panicTicks > 0) panicTicks--;

    // MC 原版：生物随机环境叫声（数据驱动，从 MobProperties::sounds 读取）
    if (ambientSoundTimer_ <= 0) {
        const auto& props = MobRegistry::instance().get(mobType);
        if (props.sounds.ambient != 0xFFFF) {
            getSoundEngine().play(static_cast<SoundEventId>(props.sounds.ambient), position, 0.4f);
        }
        ambientSoundTimer_ = 80 + mobRandInt(0, 119);
    } else {
        ambientSoundTimer_--;
    }

    // 重力
    if (!onGround) {
        velocity.y -= 28.0f * 0.05f;
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

    // 蜘蛛爬墙
    if (mobType == MobType::Spider && !onGround) {
        float feetY = position.y - halfExtents.y;
        int bx = static_cast<int>(std::floor(position.x));
        int by = static_cast<int>(std::floor(feetY));
        int bz = static_cast<int>(std::floor(position.z));
        bool touchingWall = false;
        float hw = halfExtents.x + 0.05f;
        int checkPositions[][2] = {
            {static_cast<int>(std::floor(position.x + hw)), bz},
            {static_cast<int>(std::floor(position.x - hw)), bz},
            {bx, static_cast<int>(std::floor(position.z + hw))},
            {bx, static_cast<int>(std::floor(position.z - hw))}
        };
        for (auto& cp : checkPositions) {
            for (int dy = 0; dy <= static_cast<int>(std::ceil(mobHeight)); dy++) {
                if (BlockRegistry::instance().isSolid(world.getBlock(cp[0], by + dy, cp[1]))) {
                    touchingWall = true;
                    break;
                }
            }
            if (touchingWall) break;
        }
        if (touchingWall) {
            if (velocity.y < 0) velocity.y = 0;
            velocity.y = moveSpeed * 4.0f;
        }
    }

    // 物理碰撞
    integrateMotion(world, 0.05f);

    // AI（组件化）
    tickAI(world, player, mgr);

    // 燃烧
    tickBurning(world);

    // 行走动画
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

// ========== AI（组件化） ==========

void MobEntity::tickAI(World& world, Player& player, EntityManager& mgr) {
    // 遍历 AI 目标，按优先级找到第一个可用的目标执行
    int bestGoal = -1;
    int bestPriority = 999;

    for (int i = 0; i < static_cast<int>(aiGoals_.size()); i++) {
        if (aiGoals_[i]->canUse(*this, world, player)) {
            if (aiGoals_[i]->priority < bestPriority) {
                bestPriority = aiGoals_[i]->priority;
                bestGoal = i;
            }
        }
    }

    // 如果有可用目标，执行它
    if (bestGoal >= 0) {
        activeGoalIndex_ = bestGoal;
        aiGoals_[bestGoal]->tick(*this, world, player, mgr);
    } else {
        // 没有可用目标时，默认 Idle
        activeGoalIndex_ = -1;
        stateTimer--;
        if (stateTimer <= 0) {
            aiState = AIState::Idle;
            stateTimer = 40 + mobRandInt(0, 79);
        }
    }
}

// ========== 受伤（数据驱动音效） ==========

void MobEntity::takeDamage(int amount, const glm::vec3& knockbackDir) {
    if (invulnerableTicks > 0 || isDying) return;

    hp -= amount;
    hurtTicks = 10;
    invulnerableTicks = 10;

    // 击退
    velocity += knockbackDir * 8.0f;
    velocity.y += 5.0f;

    // 播放受伤音效（数据驱动）
    const auto& props = MobRegistry::instance().get(mobType);
    if (props.sounds.hurt != 0xFFFF) {
        getSoundEngine().play(static_cast<SoundEventId>(props.sounds.hurt), position, 0.5f);
    } else {
        getSoundEngine().play(SoundEventId::DamageHit, position, 0.5f);
    }

    if (hp <= 0) {
        hp = 0;
        isDying = true;
        deathTicks = 0;

        // 播放死亡音效（数据驱动）
        if (props.sounds.death != 0xFFFF) {
            getSoundEngine().play(static_cast<SoundEventId>(props.sounds.death), position, 0.6f);
        }
    } else {
    // 被动生物受伤后逃跑
        if (!props.isHostile) {
            aiState = AIState::Flee;
            stateTimer = 60 + mobRandInt(0, 39);
            panicTicks = 80 + mobRandInt(0, 39);
        }
        // 蜘蛛被攻击后标记为被激怒
        if (mobType == MobType::Spider) {
            provoked = true;
        }
    }
}

// ========== 掉落物（数据驱动） ==========

void MobEntity::spawnLoot(World& world, EntityManager& mgr) {
    const auto& props = MobRegistry::instance().get(mobType);

    // 从 lootTable 生成掉落物
    for (const auto& entry : props.lootTable) {
        // 概率检查
        if (entry.chance < 1.0f) {
            if (mobRandFloat() > entry.chance) continue;
        }
        int count = entry.minCount;
        if (entry.maxCount > entry.minCount) {
            count += mobRandInt(0, entry.maxCount - entry.minCount);
        }
        if (count > 0) {
            mgr.spawnItem(position + glm::vec3(0, 0.5f, 0),
                          {entry.itemId, static_cast<uint16_t>(count), 0});
        }
    }

    // 经验球掉落（数据驱动）
    int xpDrop = props.xpDropMin;
    if (props.xpDropMax > props.xpDropMin) {
        xpDrop += mobRandInt(0, props.xpDropMax - props.xpDropMin);
    }
    if (xpDrop > 0) {
        mgr.spawnXPOrbs(position, xpDrop);
    }
}

// ========== 燃烧 ==========

void MobEntity::tickBurning(World& world) {
    const auto& props = MobRegistry::instance().get(mobType);
    if (!props.burnInSunlight) return;

    bool isDaytime = sDayNight ? sDayNight->isDay() : true;

    if (isDaytime) {
        int bx = static_cast<int>(std::floor(position.x));
        int bz = static_cast<int>(std::floor(position.z));
        int by = static_cast<int>(std::floor(position.y + halfExtents.y));
        int maxY = std::min(by + 64, 255);

        bool exposed = true;
        for (int y = by + 1; y <= maxY; y++) {
            BlockId b = world.getBlock(bx, y, bz);
            if (BlockRegistry::instance().isSolid(b) || BlockRegistry::instance().isLiquid(b)) {
                exposed = false;
                break;
            }
        }

        if (exposed && fireTicks <= 0) {
            fireTicks = 160;
        }
    }

    if (fireTicks > 0) {
        fireTicks--;
        fireTimer++;
        if (fireTimer >= 20) {
            fireTimer = 0;
            takeDamage(1, glm::vec3(0));
        }
    }
}

// ========== 移动辅助 ==========

bool MobEntity::canSeePlayer(const World& world, const Player& player) const {
    glm::vec3 start = position + glm::vec3(0, halfExtents.y * 0.8f, 0);
    glm::vec3 end = player.getEyePosition();
    glm::vec3 dir = end - start;
    float dist = glm::length(dir);
    if (dist < 0.01f) return true;
    dir /= dist;

    // 优化：根据距离动态调整步进大小
    // 近距离（<8格）用 0.5 步进，远距离用 1.0 步进
    float step = (dist > 8.0f) ? 1.0f : 0.5f;

    for (float t = step; t < dist; t += step) {
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

    bodyYaw = std::atan2(dir.x, dir.z);

    // 自动跳跃
    if (onGround && world) {
        float feetY = position.y - halfExtents.y;
        int checkX = static_cast<int>(std::floor(position.x + dir.x * 0.6f));
        int checkZ = static_cast<int>(std::floor(position.z + dir.z * 0.6f));
        int checkY = static_cast<int>(std::floor(feetY));
        if (BlockRegistry::instance().isSolid(world->getBlock(checkX, checkY, checkZ))) {
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

    float accel = onGround ? speed * 4.0f : speed * 1.0f;
    velocity.x += dir.x * accel;
    velocity.z += dir.z * accel;

    float hSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    float maxSpeed = speed * 4.317f;
    if (hSpeed > maxSpeed) {
        float scale = maxSpeed / hSpeed;
        velocity.x *= scale;
        velocity.z *= scale;
    }
}

bool MobEntity::tryJump() {
    if (onGround) {
        velocity.y = 9.0f;
        return true;
    }
    return false;
}
