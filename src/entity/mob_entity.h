#pragma once

#include "entity.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

class World;
class Player;
class EntityManager;
class DayNightCycle;

// ========== 生物类型 ==========
enum class MobType : uint8_t {
    Pig = 0,
    Cow,
    Sheep,
    Chicken,
    Zombie,
    Skeleton,
    Spider,
    Creeper,
    COUNT
};

// ========== AI 状态 ==========
enum class AIState : uint8_t {
    Idle,       // 站立不动
    Wander,     // 随机漫步
    Flee,       // 逃跑（被动生物受伤后）
    Chase,      // 追踪玩家（敌对生物）
    Attack,     // 攻击中
    Cooldown,   // 攻击冷却
    Exploding,  // 苦力怕膨胀中
};

// ========== 生物属性注册表 ==========
struct MobProperties {
    std::string name;
    MobType type;
    int maxHp;
    float moveSpeed;
    float attackDamage;
    float attackRange;
    int attackCooldownTicks;
    float width, height;    // 碰撞箱尺寸
    float detectRange;      // 检测玩家距离
    bool isHostile;         // 是否敌对
    bool burnInSunlight;    // 是否在阳光下燃烧
};

class MobRegistry {
public:
    static MobRegistry& instance();
    void registerDefaults();
    const MobProperties& get(MobType type) const { return mobs_[static_cast<int>(type)]; }

private:
    MobRegistry() = default;
    MobProperties mobs_[static_cast<int>(MobType::COUNT)];
};

// ========== 生物实体 ==========
class MobEntity : public Entity {
public:
    MobType mobType;

    // 基础属性
    int hp, maxHp;
    float moveSpeed;
    float attackDamage;
    float attackRange;

    // AI 状态
    AIState aiState = AIState::Idle;
    int stateTimer = 0;         // 当前状态剩余 tick
    glm::vec3 wanderTarget{0};  // 漫步目标点
    bool hasWanderTarget = false;

    // 寻路
    std::vector<glm::ivec2> path;  // A* 路径（XZ 方块坐标）
    int pathIndex = 0;
    int pathUpdateTimer = 0;       // 每 20 tick 更新一次路径

    // 碰撞箱
    float mobWidth, mobHeight;

    // 视觉
    float bodyYaw = 0.0f;      // 身体朝向（弧度）
    float headYaw = 0.0f;      // 头部朝向
    float headPitch = 0.0f;
    float walkCycle = 0.0f;    // 行走动画周期 (0-2π)
    float prevBodyYaw = 0.0f;
    float prevWalkCycle = 0.0f;
    float prevHeadYaw = 0.0f;
    float prevHeadPitch = 0.0f;

    // 战斗
    int attackCooldown = 0;
    int hurtTicks = 0;         // 受伤闪红
    int invulnerableTicks = 0; // 无敌帧
    int deathTicks = 0;        // 死亡动画
    bool isDying = false;

    // 苦力怕专用
    int fuseTimer = 0;         // 爆炸倒计时 (30 ticks = 1.5s)
    bool ignited = false;

    // 受击加速：被攻击后一段时间内加速移动和摆腿
    int panicTicks = 0;        // 受击恐慌剩余tick数（被动生物用）

    // 蜘蛛专用：被激怒标记（白天被攻击后会追踪玩家）
    bool provoked = false;

    // 燃烧（僵尸/骷髅阳光下）
    int fireTicks = 0;
    int fireTimer = 0;         // 每秒伤害计时

    // 环境叫声计时器（MC 原版：生物随机间隔发出叫声）
    int ambientSoundTimer_ = 100;  // 初始延迟，避免生成时立即叫

    // 静态昼夜循环引用，由 Game::gameTick() 每 tick 设置
    static const DayNightCycle* sDayNight;

    MobEntity() = default;
    explicit MobEntity(MobType type);

    void tick(World& world, EntityManager& mgr,
              Player& player, Inventory& inventory) override;
    EntityKind kind() const override { return EntityKind::Mob; }

    void captureInterpState() override {
        Entity::captureInterpState();
        prevBodyYaw = bodyYaw;
        prevWalkCycle = walkCycle;
        prevHeadYaw = headYaw;
        prevHeadPitch = headPitch;
    }

    // 受伤
    void takeDamage(int amount, const glm::vec3& knockbackDir);

    // 获取碰撞箱 AABB（用于物理碰撞）
    glm::vec3 getMinBounds() const {
        return position - glm::vec3(mobWidth * 0.5f, mobHeight * 0.5f, mobWidth * 0.5f);
    }
    glm::vec3 getMaxBounds() const {
        return position + glm::vec3(mobWidth * 0.5f, mobHeight * 0.5f, mobWidth * 0.5f);
    }

    // 获取受击箱 AABB（用于攻击判定，包含头部区域）
    // 受击箱从脚底开始，覆盖整个可见模型（包括头部）
    // 比碰撞箱更大，确保从任何方向都能命中
    glm::vec3 getHitboxMin() const {
        // 脚底位置 = position.y - mobHeight/2
        float feetY = position.y - mobHeight * 0.5f;
        // 水平方向用碰撞箱宽度，上下方向额外扩展确保头部可命中
        float hitW = mobWidth * 0.5f + 0.15f;
        return glm::vec3(position.x - hitW, feetY - 0.1f, position.z - hitW);
    }
    glm::vec3 getHitboxMax() const {
        // 头顶位置 = position.y + mobHeight/2，再额外加余量覆盖头部上方
        float topY = position.y + mobHeight * 0.5f + 0.3f;
        float hitW = mobWidth * 0.5f + 0.15f;
        return glm::vec3(position.x + hitW, topY, position.z + hitW);
    }

private:
    void tickAI(World& world, Player& player, EntityManager& mgr);
    void tickPassiveAI(World& world, Player& player);
    void tickHostileAI(World& world, Player& player, EntityManager& mgr);
    void tickMovement(World& world);
    void tickCombat(Player& player, EntityManager& mgr);
    void tickBurning(World& world);

    // 掉落物
    void spawnLoot(World& world, EntityManager& mgr);

    // AI 辅助
    bool canSeePlayer(const World& world, const Player& player) const;
    void moveToward(const glm::vec3& target, float speed, World* world = nullptr);
    void moveAlongPath(float speed);
    bool tryJump();

};
