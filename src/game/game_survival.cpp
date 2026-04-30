// game_survival.cpp — 玩家生存系统 tick 逻辑
// 从 game.cpp 的 gameTick() 中提取，包含：
// - 摔落伤害、虚空伤害
// - 饥饿/饱食度消耗与恢复
// - 进食
// - 水下呼吸/溺水

#include "game.h"
#include "core/debug.h"
#include "entity/mob_entity.h"
#include "audio/sound_engine.h"
#include "audio/block_sound_map.h"
#include <cmath>
#include <algorithm>

void Game::tickFallDamage() {
    if (!player_.onGround && !player_.wasFalling) {
        player_.fallStartY = player_.position.y;
        player_.wasFalling = true;
    }
    if (!player_.onGround && player_.position.y > player_.fallStartY) {
        player_.fallStartY = player_.position.y;
    }
    if (player_.onGround && player_.wasFalling) {
        float fallDist = player_.fallStartY - player_.position.y;
        if (fallDist > 3.0f) {
            int dmg = static_cast<int>(fallDist - 3.0f);
            player_.takeDamage(dmg);
            VLOG(DebugCat::Physics, "fall damage: dist=%.1f dmg=%d hp=%d", fallDist, dmg, player_.hp);

            // MC 原版：摔落伤害音效（大摔 > 4 格，小摔 > 3 格）
            if (fallDist > 4.0f) {
                getSoundEngine().play(SoundEventId::DamageFallBig, player_.position);
            } else {
                getSoundEngine().play(SoundEventId::DamageFallSmall, player_.position);
            }
        }
        player_.wasFalling = false;
    }
}

void Game::tickVoidDamage() {
    if (player_.position.y < -64.0f && !player_.dead) {
        if (tickClock_.getTotalTicks() % 10 == 0) {
            player_.takeDamage(4);
            VLOG(DebugCat::Physics, "void damage: hp=%d", player_.hp);
        }
    }
}

void Game::tickHunger() {
    // Sprint hunger drain
    if (player_.sprinting && glm::length(player_.velocity) > 0.01f) {
        player_.saturation -= 0.1f;
    }
    if (player_.saturation < 0.0f) {
        player_.hunger += static_cast<int>(player_.saturation);
        player_.saturation = 0.0f;
        if (player_.hunger < 0) player_.hunger = 0;
    }
    if (player_.saturation > static_cast<float>(player_.hunger)) {
        player_.saturation = static_cast<float>(player_.hunger);
    }

    ++player_.hungerTickTimer;

    // Fast regen (hunger >= 18, has saturation)
    if (player_.hunger >= 18 && player_.hp < player_.maxHp && player_.saturation > 0.0f) {
        if (player_.hungerTickTimer % 10 == 0) {
            player_.hp = std::min(player_.hp + 1, player_.maxHp);
            player_.saturation -= 1.5f;
            if (player_.saturation < 0.0f) {
                player_.hunger = std::max(0, player_.hunger - 1);
                player_.saturation = 0.0f;
            }
        }
    } else if (player_.hungerTickTimer >= 80) {
        player_.hungerTickTimer = 0;
        if (player_.hunger >= 18 && player_.hp < player_.maxHp) {
            player_.hp = std::min(player_.hp + 1, player_.maxHp);
            player_.hunger = std::max(0, player_.hunger - 1);
        } else if (player_.hunger <= 0 && player_.hp > 1) {
            player_.takeDamage(1);
        }
    }
}

void Game::tickEating() {
    if (!player_.isEating) return;

    ++player_.eatingTicks;

    // MC 原版：吃东西时每 4 tick 播放咀嚼音效 + 生成食物碎片粒子
    if (player_.eatingTicks % 4 == 0) {
        getSoundEngine().play2D(SoundEventId::PlayerEat, 0.5f);
        const ItemStack& held = inventory_.getHeldItem();
        if (!held.isEmpty()) {
            const auto& fp = ItemRegistry::instance().get(held.id);
            if (!fp.iconTileName.empty()) {
                uint16_t tileIdx = textureAtlas_.getTileIndex(fp.iconTileName);
                glm::vec3 eyePos = player_.getEyePosition();
                glm::vec3 forward = player_.getForward();
                // 嘴巴位置：眼睛前方偏下
                glm::vec3 mouthPos = eyePos + forward * 0.3f - glm::vec3(0.0f, 0.15f, 0.0f);
                particleSystem_.spawnEating(mouthPos, forward, tileIdx);
            }
        }
    }

    if (player_.eatingTicks >= 32) {
        ItemStack& held = inventory_.getHeldItem();
        if (!held.isEmpty()) {
            const auto& fp = ItemRegistry::instance().get(held.id);
            if (fp.type == ItemType::Food) {
                player_.hunger = std::min(player_.hunger + fp.hungerRestore, player_.maxHunger);
                player_.saturation = std::min(player_.saturation + fp.saturationRestore,
                                              static_cast<float>(player_.hunger));
                inventory_.consumeHeldItem(1);
                VLOG(DebugCat::Input, "ate %s hunger=%d sat=%.1f",
                     fp.displayName.c_str(), player_.hunger, player_.saturation);

                // MC 原版：吃完食物播放打嗝音效
                getSoundEngine().play2D(SoundEventId::PlayerBurp, 0.5f);
            }
        }
        player_.eatingTicks = 0;
        player_.isEating = false;
    }
}

void Game::tickBreathing() {
    glm::vec3 eye = player_.getEyePosition();
    int ex = static_cast<int>(std::floor(eye.x));
    int ey = static_cast<int>(std::floor(eye.y));
    int ez = static_cast<int>(std::floor(eye.z));
    BlockId headBlock = world_.getBlock(ex, ey, ez);
    player_.inWater = (headBlock == Block::Water);

    if (player_.inWater) {
        if (player_.air > 0) {
            --player_.air;
        } else {
            if (tickClock_.getTotalTicks() % 20 == 0) {
                player_.takeDamage(2);
                VLOG(DebugCat::Physics, "drowning damage: hp=%d", player_.hp);
            }
        }
    } else {
        if (player_.air < player_.maxAir) {
            player_.air = std::min(player_.air + 5, player_.maxAir);
        }
    }
}

// ========== 玩家攻击生物 ==========

void Game::tickPlayerAttack() {
    if (player_.dead) return;
    if (!leftMouseHeld_) return;
    if (player_.attackCooldownTicks > 0) return;

    // 检查玩家视线方向是否命中生物
    glm::vec3 eye = player_.getEyePosition();
    glm::vec3 fwd = player_.getForward();

    MobEntity* closestMob = nullptr;
    float closestDist = MAX_REACH + 1.0f;

    for (const auto& e : entityManager_.entities()) {
        if (!e || !e->alive || e->kind() != EntityKind::Mob) continue;
        auto& mob = static_cast<MobEntity&>(*e);
        if (mob.isDying) continue;

        // AABB 射线检测（使用受击箱，比碰撞箱更大，包含头部）
        glm::vec3 minB = mob.getHitboxMin();
        glm::vec3 maxB = mob.getHitboxMax();

        // 射线-AABB 相交测试
        float tmin = 0.0f, tmax = MAX_REACH;
        bool hit = true;
        for (int i = 0; i < 3; i++) {
            if (std::abs(fwd[i]) < 1e-6f) {
                if (eye[i] < minB[i] || eye[i] > maxB[i]) { hit = false; break; }
            } else {
                float invD = 1.0f / fwd[i];
                float t1 = (minB[i] - eye[i]) * invD;
                float t2 = (maxB[i] - eye[i]) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) { hit = false; break; }
            }
        }

        if (hit && tmin < closestDist) {
            closestDist = tmin;
            closestMob = &mob;
        }
    }

    if (closestMob) {
        // 计算伤害
        const ItemStack& held = inventory_.getHeldItem();
        float baseDmg = 1.0f;  // 空手
        if (!held.isEmpty()) {
            const auto& itemProps = ItemRegistry::instance().get(held.id);
            if (itemProps.attackDamage > 0.0f) {
                baseDmg = itemProps.attackDamage;
            }
        }

        // 击退方向
        glm::vec3 kb = glm::normalize(closestMob->position - player_.position);
        kb.y = 0;
        if (glm::length(kb) < 0.01f) kb = fwd;
        kb = glm::normalize(kb);

        closestMob->takeDamage(static_cast<int>(baseDmg), kb);
        player_.attackCooldownTicks = player_.attackCooldownMax;

        // 播放攻击命中音效
        getSoundEngine().play(SoundEventId::SuccessfulHit, closestMob->position, 0.5f);

        // 触发挥动动画
        player_.startSwing();

        // 工具耐久消耗
        if (!held.isEmpty()) {
            const auto& itemProps = ItemRegistry::instance().get(held.id);
            if (itemProps.durability > 0) {
                inventory_.getHeldItem().durability++;
                if (inventory_.getHeldItem().durability >= itemProps.durability) {
                    inventory_.getHeldItem().clear();
                }
            }
        }
    }
}
