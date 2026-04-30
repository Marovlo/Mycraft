#pragma once

#include "mob_entity.h"
#include "world/world.h"
#include "player/player.h"
#include "entity_manager.h"
#include "arrow_entity.h"
#include "audio/sound_engine.h"
#include "world/day_night_cycle.h"
#include <cstdlib>
#include <cmath>
#include <glm/glm.hpp>

// ========== 被动生物 AI 目标 ==========

// 随机漫步目标
struct WanderGoal : AIGoal {
    int cooldown = 0;
    WanderGoal() { priority = 5; }

    bool canUse(const MobEntity& mob, const World&, const Player&) const override {
        return mob.aiState == AIState::Idle && cooldown <= 0;
    }

    void tick(MobEntity& mob, World& world, Player&, EntityManager&) override;
};

// 逃跑目标（被动生物受伤后）
struct FleeGoal : AIGoal {
    FleeGoal() { priority = 1; }  // 高优先级

    bool canUse(const MobEntity& mob, const World&, const Player&) const override {
        return mob.aiState == AIState::Flee;
    }

    void tick(MobEntity& mob, World& world, Player& player, EntityManager&) override;
};

// ========== 敌对生物 AI 目标 ==========

// 追踪玩家目标
struct ChaseGoal : AIGoal {
    int pathUpdateTimer = 0;
    ChaseGoal() { priority = 3; }

    bool canUse(const MobEntity& mob, const World& world, const Player& player) const override;
    void tick(MobEntity& mob, World& world, Player& player, EntityManager&) override;
};

// 近战攻击目标
struct MeleeAttackGoal : AIGoal {
    MeleeAttackGoal() { priority = 2; }

    bool canUse(const MobEntity& mob, const World&, const Player& player) const override {
        float dist = glm::length(mob.position - player.position);
        return mob.aiState == AIState::Chase && dist <= mob.attackRange && mob.attackCooldown <= 0;
    }

    void tick(MobEntity& mob, World& world, Player& player, EntityManager& mgr) override;
};

// 远程攻击目标（骷髅射箭）
struct RangedAttackGoal : AIGoal {
    RangedAttackGoal() { priority = 2; }

    bool canUse(const MobEntity& mob, const World&, const Player& player) const override {
        float dist = glm::length(mob.position - player.position);
        // MC 原版：骷髅在攻击范围内且攻击冷却结束时可以开始蓄力
        // 或者已经在蓄力中（继续蓄力直到射出）
        return mob.aiState == AIState::Chase && dist <= mob.attackRange &&
               (mob.attackCooldown <= 0 || mob.isChargingBow);
    }

    void tick(MobEntity& mob, World& world, Player& player, EntityManager& mgr) override;
};

// 苦力怕爆炸目标
struct CreeperExplodeGoal : AIGoal {
    CreeperExplodeGoal() { priority = 1; }

    bool canUse(const MobEntity& mob, const World&, const Player& player) const override {
        float dist = glm::length(mob.position - player.position);
        return mob.aiState == AIState::Chase && dist <= mob.attackRange && mob.attackCooldown <= 0;
    }

    void tick(MobEntity& mob, World& world, Player& player, EntityManager& mgr) override;
};

// 蜘蛛白天中立目标（覆盖追踪行为）
struct SpiderNeutralGoal : AIGoal {
    SpiderNeutralGoal() { priority = 4; }  // 比 Chase 低优先级

    bool canUse(const MobEntity& mob, const World&, const Player&) const override {
        // 白天且未被激怒时，不追踪
        return MobEntity::sDayNight && MobEntity::sDayNight->isDay() && !mob.provoked;
    }

    void tick(MobEntity& mob, World& world, Player& player, EntityManager& mgr) override {
        // 白天中立时执行漫步行为
        if (mob.aiState == AIState::Chase) {
            mob.aiState = AIState::Idle;
            mob.stateTimer = 40 + (std::rand() % 80);
        }
    }
};
