// game_survival.cpp — 玩家生存系统 tick 逻辑
// 从 game.cpp 的 gameTick() 中提取，包含：
// - 摔落伤害、虚空伤害
// - 饥饿/饱食度消耗与恢复
// - 进食
// - 水下呼吸/溺水

#include "game.h"
#include "core/debug.h"
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
