#pragma once

#include "entity.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>

class World;
class Player;
class EntityManager;
class DayNightCycle;
class Inventory;

// ========== 生物类型 ==========
// 保留枚举用于快速索引，但注册系统不再依赖 COUNT
enum class MobType : uint8_t {
    Pig = 0,
    Cow,
    Sheep,
    Chicken,
    Zombie,
    Skeleton,
    Spider,
    Creeper,
    COUNT  // 当前已注册的生物数量上限（仅用于静态数组兼容）
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

// ========== AI 行为组件 ==========
// 组件化 AI：每种生物可以组合不同的 AI 目标
class MobEntity;  // 前向声明

struct AIGoal {
    virtual ~AIGoal() = default;
    // 返回 true 表示该目标当前可以激活
    virtual bool canUse(const MobEntity& mob, const World& world, const Player& player) const = 0;
    // 每 tick 执行
    virtual void tick(MobEntity& mob, World& world, Player& player, EntityManager& mgr) = 0;
    // 目标优先级（越小越优先）
    int priority = 0;
};

// ========== 掉落物表 ==========
struct LootEntry {
    uint16_t itemId;     // ItemId
    uint16_t minCount;
    uint16_t maxCount;
    float    chance = 1.0f;  // 掉落概率 (0.0~1.0)
};

// ========== 生物音效配置 ==========
struct MobSounds {
    // 音效事件 ID（SoundEventId 的 uint16_t 值）
    // 0xFFFF = 无音效
    uint16_t ambient = 0xFFFF;   // 环境叫声
    uint16_t hurt    = 0xFFFF;   // 受伤音效
    uint16_t death   = 0xFFFF;   // 死亡音效
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

    // --- 数据驱动的掉落物表（消除 spawnLoot 的 switch/case） ---
    std::vector<LootEntry> lootTable;

    // --- 经验值掉落 ---
    int xpDropMin = 0;
    int xpDropMax = 0;

    // --- 音效配置（消除音效 switch/case） ---
    MobSounds sounds;

    // --- AI 行为配置 ---
    // AI 目标工厂函数列表：注册时指定该生物使用哪些 AI 行为
    using AIGoalFactory = std::function<std::unique_ptr<AIGoal>()>;
    std::vector<AIGoalFactory> aiGoalFactories;
};

class MobRegistry {
public:
    static MobRegistry& instance();
    void registerDefaults();

    const MobProperties& get(MobType type) const { return mobs_[static_cast<int>(type)]; }
    const MobProperties& getByIndex(int index) const { return mobs_[index]; }
    int mobCount() const { return static_cast<int>(mobs_.size()); }

private:
    MobRegistry() = default;
    std::vector<MobProperties> mobs_;
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

    // 骷髅拉弓蓄力（MC 原版：骷髅射箭前有 20 tick 蓄力动画）
    int bowChargeTicks = 0;    // 当前蓄力 tick 数（0 = 未蓄力）
    bool isChargingBow = false; // 是否正在拉弓

    // 受击加速：被攻击后一段时间内加速移动和摆腿
    int panicTicks = 0;        // 受击恐慌剩余tick数（被动生物用）

    // 蜘蛛专用：被激怒标记（白天被攻击后会追踪玩家）
    bool provoked = false;

    // 羊专用：剪毛/吃草机制（MC原版）
    bool isSheared = false;        // 是否被剪毛
    int eatGrassTimer = 0;         // 吃草动画计时器（>0 表示正在吃草）
    int grassRegrowTimer = 0;      // 吃草后毛重新生长的计时器

    // 燃烧（僵尸/骷髅阳光下）
    int fireTicks = 0;
    int fireTimer = 0;         // 每秒伤害计时

    // 环境叫声计时器（MC 原版：生物随机间隔发出叫声）
    int ambientSoundTimer_ = 100;  // 初始延迟，避免生成时立即叫

    // AI 目标组件列表
    std::vector<std::unique_ptr<AIGoal>> aiGoals_;
    int activeGoalIndex_ = -1;  // 当前激活的 AI 目标索引

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
    glm::vec3 getHitboxMin() const {
        float feetY = position.y - mobHeight * 0.5f;
        float hitW = mobWidth * 0.5f + 0.15f;
        return glm::vec3(position.x - hitW, feetY - 0.1f, position.z - hitW);
    }
    glm::vec3 getHitboxMax() const {
        float topY = position.y + mobHeight * 0.5f + 0.3f;
        float hitW = mobWidth * 0.5f + 0.15f;
        return glm::vec3(position.x + hitW, topY, position.z + hitW);
    }

    // 公开的移动辅助（AI 目标组件需要调用）
    void moveToward(const glm::vec3& target, float speed, World* world = nullptr);
    bool tryJump();
    bool canSeePlayer(const World& world, const Player& player) const;

private:
    void tickAI(World& world, Player& player, EntityManager& mgr);
    void tickMovement(World& world);
    void tickCombat(Player& player, EntityManager& mgr);
    void tickBurning(World& world);

    // 掉落物（数据驱动，从 MobProperties::lootTable 读取）
    void spawnLoot(World& world, EntityManager& mgr);
};
