#pragma once

#include "entity.h"
#include <glm/glm.hpp>

class Player;
class Inventory;

// 箭矢实体：由骷髅射出（未来也可由玩家射出），沿抛物线飞行，
// 命中方块后插在方块上，命中玩家后造成伤害+击退。
// 地上的箭矢 60 秒后消失，玩家可拾取。
class ArrowEntity : public Entity {
public:
    // 发射参数
    glm::vec3 direction{0, 0, 1};  // 初始飞行方向（归一化）
    float speed = 0.0f;             // 初始速度（blocks/tick）
    int damage = 2;                 // 伤害值

    // 状态
    bool inGround = false;          // 是否已插在方块上
    int groundTimer = 0;            // 插在地上后的计时（tick）
    int lifetimeTicks = 0;          // 总存活时间
    int pickupDelay = 10;           // 拾取延迟（tick）

    // 视觉
    float yaw = 0.0f;              // 水平朝向（弧度）
    float pitch = 0.0f;            // 俯仰角（弧度）
    float prevYaw = 0.0f;
    float prevPitch = 0.0f;

    // 是否由玩家射出（影响是否可拾取）
    bool fromPlayer = false;

    static constexpr int MAX_GROUND_TIME = 1200;  // 60秒后消失
    static constexpr int MAX_FLIGHT_TIME = 200;    // 飞行最多10秒
    static constexpr float GRAVITY = 0.05f;        // 重力加速度（blocks/tick²）
    static constexpr float AIR_DRAG = 0.99f;       // 空气阻力

    ArrowEntity() {
        halfExtents = glm::vec3(0.05f, 0.05f, 0.25f); // 细长碰撞箱
    }

    void tick(World& world, EntityManager& mgr,
              Player& player, Inventory& inventory) override;
    EntityKind kind() const override { return EntityKind::Arrow; }

    void captureInterpState() override {
        Entity::captureInterpState();
        prevYaw = yaw;
        prevPitch = pitch;
    }

    // 初始化箭矢：从发射位置沿方向以给定速度飞出
    void launch(const glm::vec3& from, const glm::vec3& dir, float spd, int dmg);

private:
    // 检测飞行路径上是否命中方块
    bool checkBlockCollision(const World& world);
    // 检测是否命中玩家
    bool checkPlayerCollision(Player& player);
};
