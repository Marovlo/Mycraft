#include "arrow_entity.h"
#include "entity_manager.h"
#include "world/world.h"
#include "player/player.h"
#include "player/inventory.h"
#include "core/item.h"
#include "core/block.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

void ArrowEntity::launch(const glm::vec3& from, const glm::vec3& dir, float spd, int dmg) {
    position = from;
    prevPosition = from;
    direction = glm::normalize(dir);
    speed = spd;
    damage = dmg;
    velocity = direction * speed;

    // 计算初始朝向
    yaw = std::atan2(direction.x, direction.z);
    pitch = std::asin(-direction.y);
    prevYaw = yaw;
    prevPitch = pitch;
}

void ArrowEntity::tick(World& world, EntityManager& mgr,
                       Player& player, Inventory& inventory) {
    tickCount++;
    lifetimeTicks++;

    if (inGround) {
        // 箭矢已插在方块上
        groundTimer++;

        // 超时消失
        if (groundTimer >= MAX_GROUND_TIME) {
            alive = false;
            return;
        }

        // 玩家拾取
        if (pickupDelay <= 0) {
            float dist = glm::length(position - player.position);
            if (dist < 1.5f && !player.dead) {
                // 尝试加入背包
                ItemStack arrowStack;
                arrowStack.id = Item::Arrow;
                arrowStack.count = 1;
                if (inventory.addItem(arrowStack)) {
                    alive = false;
                }
            }
        } else {
            pickupDelay--;
        }
        return;
    }

    // === 飞行中 ===

    // 超时消失
    if (lifetimeTicks >= MAX_FLIGHT_TIME) {
        alive = false;
        return;
    }

    // 应用重力
    velocity.y -= GRAVITY;

    // 空气阻力
    velocity *= AIR_DRAG;

    // 更新朝向（跟随速度方向）
    float hSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    if (hSpeed > 0.001f) {
        yaw = std::atan2(velocity.x, velocity.z);
    }
    pitch = std::atan2(-velocity.y, hSpeed);

    // 检测方块碰撞（基于下一帧位置）
    if (checkBlockCollision(world)) {
        inGround = true;
        velocity = glm::vec3(0.0f);
        return;
    }

    // 检测玩家碰撞（基于下一帧位置）
    if (!fromPlayer && checkPlayerCollision(player)) {
        alive = false;
        return;
    }

    // 碰撞检测通过后才更新位置
    position += velocity;
}

bool ArrowEntity::checkBlockCollision(const World& world) {
    // 射线步进检测：从当前位置沿速度方向检测
    glm::vec3 nextPos = position + velocity;

    // 简化：检测下一帧位置所在的方块是否为实心
    int bx = static_cast<int>(std::floor(nextPos.x));
    int by = static_cast<int>(std::floor(nextPos.y));
    int bz = static_cast<int>(std::floor(nextPos.z));

    BlockId block = world.getBlock(bx, by, bz);
    if (block != Block::Air && block != Block::Water) {
        const auto& props = BlockRegistry::instance().get(block);
        if (props.isSolid) {
            // 插在方块表面：将位置设为碰撞点（近似）
            // 沿速度方向回退一小段
            glm::vec3 dir = glm::normalize(velocity);
            position = nextPos - dir * 0.1f;
            return true;
        }
    }

    // 额外检测：沿路径中间点（防止高速穿墙）
    if (glm::length(velocity) > 0.5f) {
        glm::vec3 midPos = position + velocity * 0.5f;
        int mx = static_cast<int>(std::floor(midPos.x));
        int my = static_cast<int>(std::floor(midPos.y));
        int mz = static_cast<int>(std::floor(midPos.z));
        BlockId midBlock = world.getBlock(mx, my, mz);
        if (midBlock != Block::Air && midBlock != Block::Water) {
            const auto& midProps = BlockRegistry::instance().get(midBlock);
            if (midProps.isSolid) {
                position = midPos - glm::normalize(velocity) * 0.1f;
                return true;
            }
        }
    }

    return false;
}

bool ArrowEntity::checkPlayerCollision(Player& player) {
    if (player.dead || player.invulnerableTicks > 0) return false;

    // 玩家 AABB
    glm::vec3 pMin = player.position - glm::vec3(0.3f, 0.0f, 0.3f);
    glm::vec3 pMax = player.position + glm::vec3(0.3f, 1.8f, 0.3f);

    // 箭矢下一帧位置
    glm::vec3 nextPos = position + velocity;

    // 简单 AABB 包含检测
    if (nextPos.x >= pMin.x && nextPos.x <= pMax.x &&
        nextPos.y >= pMin.y && nextPos.y <= pMax.y &&
        nextPos.z >= pMin.z && nextPos.z <= pMax.z) {
        // 命中！造成伤害 + 击退
        player.takeDamage(damage);

        // 击退方向：箭矢飞行方向的水平分量
        glm::vec3 hVel(velocity.x, 0.0f, velocity.z);
        float hLen = glm::length(hVel);
        if (hLen > 0.001f) {
            glm::vec3 kb = hVel / hLen;
            // 设置击退速度而非累加，避免多箭同时命中时弹飞
            player.velocity.x = kb.x * 6.0f;
            player.velocity.z = kb.z * 6.0f;
            player.velocity.y = 4.0f;
        }
        return true;
    }

    return false;
}
