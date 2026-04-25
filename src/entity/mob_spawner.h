#pragma once

#include "entity/mob_entity.h"
#include <glm/glm.hpp>
#include <cstdint>

class World;
class Player;
class EntityManager;
class DayNightCycle;

// 生物生成管理器
// 负责自然生成和消失规则，对标 MC 原版
class MobSpawner {
public:
    // 每 tick 调用，处理生成和消失
    void tick(World& world, Player& player, EntityManager& mgr,
              const DayNightCycle& dayNight);

    // 世界初始生成时放置被动生物
    void spawnInitialMobs(World& world, EntityManager& mgr, int chunkX, int chunkZ);

    // 统计当前生物数量
    int countMobs(const EntityManager& mgr, bool hostile) const;

private:
    int spawnTimer_ = 0;

    // 尝试在指定位置生成一组生物
    bool trySpawnGroup(World& world, EntityManager& mgr,
                       MobType type, int x, int y, int z, int count);

    // 检查生成条件
    bool canSpawnAt(const World& world, MobType type, int x, int y, int z) const;

    // 获取地表 Y 坐标
    int getSurfaceY(const World& world, int x, int z) const;

    // 消失规则
    void despawnFarMobs(EntityManager& mgr, const glm::vec3& playerPos);
};
