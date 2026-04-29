#pragma once

#include "core/block.h"
#include "core/common.h"
#include <vector>
#include <queue>
#include <cstdint>
#include <unordered_set>

class World;
class EntityManager;
class Player;

// 计划刻条目：在指定 tick 后对某个位置执行方块更新
struct ScheduledTick {
    int x, y, z;
    uint64_t executeTick;   // 到达此 tick 时执行
    BlockId blockId;        // 触发时的方块类型（防止方块已被替换）
    int priority = 0;       // 优先级（数值小 = 高优先级）

    // 优先队列排序：先执行 tick 小的，同 tick 按优先级
    bool operator>(const ScheduledTick& other) const {
        if (executeTick != other.executeTick) return executeTick > other.executeTick;
        return priority > other.priority;
    }
};

// 方块更新系统
// 负责：
// 1. 邻居通知：放置/破坏方块时通知相邻6个方块
// 2. 计划刻：延迟执行的方块更新（水流动、沙子下落等）
// 3. 每 tick 处理到期的计划刻
class BlockUpdateSystem {
public:
    // 初始化
    void init();

    // 每游戏 tick 调用，处理到期的计划刻
    void tick(World& world, EntityManager& entityMgr, Player& player, uint64_t currentTick);

    // 当方块被放置或破坏时调用，通知相邻方块
    void notifyNeighbors(World& world, int x, int y, int z, uint64_t currentTick);

    // 对单个方块执行邻居变更响应
    void onNeighborChanged(World& world, int x, int y, int z, uint64_t currentTick);

    // 添加计划刻（延迟 delayTicks 后执行）
    void scheduleTick(int x, int y, int z, BlockId blockId, int delayTicks, uint64_t currentTick, int priority = 0);

    // 获取待处理的计划刻数量（调试用）
    size_t pendingTickCount() const { return scheduledTicks_.size(); }

private:
    // 处理重力方块（沙子、砂砾）的下落
    void handleGravityBlock(World& world, EntityManager& entityMgr, int x, int y, int z);

    // 处理水流动
    void handleWaterFlow(World& world, int x, int y, int z, uint64_t currentTick);

    // 检查方块是否为重力方块
    bool isGravityBlock(BlockId id) const;

    // 检查方块是否为流体
    bool isFluid(BlockId id) const;

    // 计算水的流动距离（BFS查找最近水源）
    int countWaterDistance(World& world, int x, int y, int z) const;

    // 计划刻优先队列（最小堆）
    std::priority_queue<ScheduledTick, std::vector<ScheduledTick>, std::greater<ScheduledTick>> scheduledTicks_;

    // 防止重复添加计划刻的哈希集合（O(1) 查找）
    std::unordered_set<int64_t> pendingKeySet_;
};
