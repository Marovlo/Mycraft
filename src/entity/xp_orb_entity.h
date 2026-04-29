#pragma once

#include "entity.h"
#include <glm/glm.hpp>
#include <vector>

// 经验球实体 — 生物死亡时掉落，飞向附近玩家并被拾取
// MC 原版行为：
// - 经验球有磁吸效果，6 格内飞向玩家
// - 接触玩家后被拾取，增加经验值
// - 5 分钟后消失
// - 有轻微弹跳和旋转动画
class XPOrbEntity : public Entity {
public:
    int xpValue = 1;                // 该经验球包含的经验值

    int lifetimeTicks = 6000;       // 5 分钟后消失（6000 ticks @ 20 TPS）
    int pickupDelayTicks = 10;      // 生成后短暂延迟才能被拾取

    // 视觉效果
    float visualYaw = 0.0f;         // 旋转角度
    float prevVisualYaw = 0.0f;     // 上一帧旋转角度（插值用）
    float visualPhase = 0.0f;       // 浮动相位偏移

    XPOrbEntity() {
        halfExtents = glm::vec3(0.1f); // 比物品更小
    }

    explicit XPOrbEntity(int xp) : xpValue(xp) {
        halfExtents = glm::vec3(0.1f);
    }

    void tick(World& world, EntityManager& mgr,
              Player& player, Inventory& inventory) override;
    EntityKind kind() const override { return EntityKind::XPOrb; }

    void captureInterpState() override {
        Entity::captureInterpState();
        prevVisualYaw = visualYaw;
    }

    // MC 原版：根据总经验值拆分成多个经验球
    // 返回每个经验球应包含的经验值列表
    static std::vector<int> splitXP(int totalXP);

    // MC 原版：根据经验球的 xpValue 确定视觉大小（用于渲染）
    float getVisualSize() const;

    // MC 原版：根据经验球的 xpValue 确定颜色（绿色→黄色→蓝色）
    glm::vec3 getColor() const;

private:
    static constexpr float ATTRACT_RANGE = 6.0f;     // 磁吸范围
    static constexpr float ATTRACT_SPEED = 0.15f;     // 磁吸加速度
    static constexpr float PICKUP_RANGE  = 1.2f;      // 拾取范围
};
