#include "xp_orb_entity.h"
#include "world/world.h"
#include "player/player.h"
#include "player/inventory.h"
#include "entity_manager.h"
#include "audio/sound_engine.h"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

void XPOrbEntity::tick(World& world, EntityManager& mgr,
                       Player& player, Inventory& inventory) {
    tickCount++;
    lifetimeTicks--;
    if (lifetimeTicks <= 0) {
        alive = false;
        return;
    }
    if (pickupDelayTicks > 0) pickupDelayTicks--;

    // 旋转动画
    visualYaw += 0.15f;

    // 重力
    if (!onGround) {
        velocity.y -= 0.04f; // MC 原版经验球重力
    } else {
        velocity.y = 0.0f;
        // 轻微弹跳（MC 原版经验球在地面上会小幅弹跳）
        if (tickCount % 40 == 0) {
            velocity.y = 0.15f;
        }
    }

    // 摩擦力
    velocity.x *= 0.8f;
    velocity.z *= 0.8f;
    if (onGround) {
        velocity.x *= 0.7f;
        velocity.z *= 0.7f;
    }

    // 磁吸效果：飞向附近玩家
    if (!player.dead) {
        glm::vec3 diff = player.position + glm::vec3(0, 0.5f, 0) - position;
        float distSq = glm::dot(diff, diff);

        if (distSq < ATTRACT_RANGE * ATTRACT_RANGE && distSq > 0.01f) {
            float dist = std::sqrt(distSq);
            glm::vec3 dir = diff / dist;

            // 磁吸力随距离增强（越近越快）
            float strength = ATTRACT_SPEED * (1.0f - dist / ATTRACT_RANGE);
            strength = std::max(strength, 0.02f);
            velocity += dir * strength;

            // 近距离时直接加速飞向玩家
            if (dist < 2.0f) {
                velocity += dir * 0.1f;
            }
        }

        // 拾取判定
        if (pickupDelayTicks <= 0 && distSq < PICKUP_RANGE * PICKUP_RANGE) {
            player.addXP(xpValue);
            // MC 原版：拾取经验球播放 orb 音效
            getSoundEngine().play2D(SoundEventId::XPOrbPickup, 0.2f);
            alive = false;
            return;
        }
    }

    // 物理运动
    const float dt = 1.0f / 20.0f;
    integrateMotion(world, dt);
}

std::vector<int> XPOrbEntity::splitXP(int totalXP) {
    // MC 原版拆分规则：
    // 尽量用大经验球，减少实体数量
    // 经验球大小档位：1, 3, 7, 17, 37, 73, 149, 307, 617, 1237, 2477
    static const int sizes[] = {2477, 1237, 617, 307, 149, 73, 37, 17, 7, 3, 1};
    std::vector<int> result;
    int remaining = totalXP;
    for (int sz : sizes) {
        while (remaining >= sz) {
            result.push_back(sz);
            remaining -= sz;
        }
    }
    if (remaining > 0) {
        result.push_back(remaining);
    }
    return result;
}

float XPOrbEntity::getVisualSize() const {
    // 根据经验值确定视觉大小
    if (xpValue >= 2477) return 0.5f;
    if (xpValue >= 617)  return 0.45f;
    if (xpValue >= 149)  return 0.4f;
    if (xpValue >= 37)   return 0.35f;
    if (xpValue >= 7)    return 0.3f;
    if (xpValue >= 3)    return 0.25f;
    return 0.2f;
}

glm::vec3 XPOrbEntity::getColor() const {
    // MC 原版经验球颜色：绿色-黄色渐变
    // 小经验球偏绿，大经验球偏黄
    float t = std::clamp(static_cast<float>(xpValue) / 37.0f, 0.0f, 1.0f);
    return glm::vec3(
        0.0f + t * 0.8f,   // R: 0 → 0.8
        0.8f + t * 0.2f,   // G: 0.8 → 1.0
        0.0f                // B: 0
    );
}
