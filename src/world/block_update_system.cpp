#include "block_update_system.h"
#include "world.h"
#include "entity/entity_manager.h"
#include "player/player.h"

#include <cmath>
#include <algorithm>
#include <iostream>
#include <unordered_set>

void BlockUpdateSystem::init() {
    // 清空队列
    while (!scheduledTicks_.empty()) scheduledTicks_.pop();
    pendingKeySet_.clear();
}

void BlockUpdateSystem::tick(World& world, EntityManager& entityMgr, Player& player, uint64_t currentTick) {
    // 每 tick 最多处理的计划刻数量（防止爆炸式连锁反应卡帧）
    constexpr int MAX_TICKS_PER_FRAME = 256;
    int processed = 0;

    while (!scheduledTicks_.empty() && processed < MAX_TICKS_PER_FRAME) {
        const auto& top = scheduledTicks_.top();
        if (top.executeTick > currentTick) break; // 还没到时间

        ScheduledTick st = top;
        scheduledTicks_.pop();
        processed++;

        // 从 pendingKeySet_ 中移除
        int64_t packedKey = (static_cast<int64_t>(st.x) & 0xFFFFF) |
                            ((static_cast<int64_t>(st.y) & 0xFFF) << 20) |
                            ((static_cast<int64_t>(st.z) & 0xFFFFF) << 32) |
                            (static_cast<int64_t>(st.blockId) << 52);
        pendingKeySet_.erase(packedKey);

        // 验证方块是否仍然是预期类型（可能已被玩家替换）
        BlockId current = world.getBlock(st.x, st.y, st.z);
        if (current != st.blockId) continue;

        // 根据方块类型执行对应逻辑
        if (isGravityBlock(current)) {
            handleGravityBlock(world, entityMgr, st.x, st.y, st.z);
        } else if (isFluid(current)) {
            handleWaterFlow(world, st.x, st.y, st.z, currentTick);
        }
    }
}

void BlockUpdateSystem::notifyNeighbors(World& world, int x, int y, int z, uint64_t currentTick) {
    // 通知6个相邻方块
    static const int offsets[6][3] = {
        {1, 0, 0}, {-1, 0, 0},
        {0, 1, 0}, {0, -1, 0},
        {0, 0, 1}, {0, 0, -1}
    };

    for (auto& off : offsets) {
        int nx = x + off[0];
        int ny = y + off[1];
        int nz = z + off[2];
        if (ny < 0 || ny >= CHUNK_HEIGHT) continue;
        onNeighborChanged(world, nx, ny, nz, currentTick);
    }

    // 也检查自身位置（放置方块时自身可能需要响应，如水源放置）
    onNeighborChanged(world, x, y, z, currentTick);
}

void BlockUpdateSystem::onNeighborChanged(World& world, int x, int y, int z, uint64_t currentTick) {
    BlockId block = world.getBlock(x, y, z);
    if (block == Block::Air) return;

    if (isGravityBlock(block)) {
        // 沙子/砂砾：检查下方是否为空，安排 2 tick 后下落
        BlockId below = (y > 0) ? world.getBlock(x, y - 1, z) : Block::Bedrock;
        const auto& reg = BlockRegistry::instance();
        if (below == Block::Air || reg.isLiquid(below)) {
            scheduleTick(x, y, z, block, 2, currentTick, 0);
        }
    } else if (isFluid(block)) {
        // 水：安排 5 tick 后流动更新
        scheduleTick(x, y, z, block, 5, currentTick, 1);
    }
}

void BlockUpdateSystem::scheduleTick(int x, int y, int z, BlockId blockId, int delayTicks, uint64_t currentTick, int priority) {
    // 检查是否已有相同的计划刻（使用哈希集合 O(1) 查找）
    int64_t packedKey = (static_cast<int64_t>(x) & 0xFFFFF) |
                        ((static_cast<int64_t>(y) & 0xFFF) << 20) |
                        ((static_cast<int64_t>(z) & 0xFFFFF) << 32) |
                        (static_cast<int64_t>(blockId) << 52);
    if (pendingKeySet_.count(packedKey)) return; // 已存在，不重复添加

    ScheduledTick st;
    st.x = x;
    st.y = y;
    st.z = z;
    st.executeTick = currentTick + delayTicks;
    st.blockId = blockId;
    st.priority = priority;

    scheduledTicks_.push(st);
    pendingKeySet_.insert(packedKey);
}

void BlockUpdateSystem::handleGravityBlock(World& world, EntityManager& entityMgr, int x, int y, int z) {
    // 检查下方是否可以下落
    if (y <= 0) return;

    BlockId below = world.getBlock(x, y - 1, z);
    const auto& reg = BlockRegistry::instance();

    if (below != Block::Air && !reg.isLiquid(below)) return; // 下方有实心方块，不下落

    BlockId thisBlock = world.getBlock(x, y, z);

    // 简化实现：直接找到最终落点（不创建下落实体，避免复杂度）
    // MC 原版用 FallingBlockEntity，但对于我们的项目，直接瞬移到底部更简洁
    int targetY = y - 1;
    while (targetY > 0) {
        BlockId check = world.getBlock(x, targetY - 1, z);
        if (check != Block::Air && !reg.isLiquid(check)) break;
        targetY--;
    }

    // 移除原位置方块
    world.setBlock(x, y, z, Block::Air);
    // 放置到目标位置
    world.setBlock(x, targetY, z, thisBlock);

    // 通知原位置和目标位置的邻居
    // 注意：这里不直接调用 notifyNeighbors，而是通过 scheduleTick 延迟处理
    // 避免无限递归。我们在下一个 tick 中处理连锁反应。
    // 原位置上方的沙子可能也需要下落
    if (y + 1 < CHUNK_HEIGHT) {
        BlockId above = world.getBlock(x, y + 1, z);
        if (isGravityBlock(above)) {
            // 注意：handleGravityBlock 没有 currentTick 参数，
            // 使用 delayTicks=2 + currentTick=1 确保下一次 tick 处理
            // 这里传入 1 作为基准 tick，scheduleTick 会加上 delay
            // 实际上由于 tick() 中 executeTick <= currentTick 就会执行，
            // 传入较小值等效于"尽快执行"
            scheduleTick(x, y + 1, z, above, 0, 1, 0); // 尽快执行（下一次 tick）
        }
    }
}

void BlockUpdateSystem::handleWaterFlow(World& world, int x, int y, int z, uint64_t currentTick) {
    BlockId block = world.getBlock(x, y, z);
    if (block != Block::Water) return;

    const auto& reg = BlockRegistry::instance();

    // 水向下流动（优先）
    if (y > 0) {
        BlockId below = world.getBlock(x, y - 1, z);
        if (below == Block::Air) {
            world.setBlock(x, y - 1, z, Block::Water);
            // 继续向下流动
            scheduleTick(x, y - 1, z, Block::Water, 5, currentTick, 1);
            return; // 向下流动时不向四周扩散
        }
    }

    // 水向四周流动（水平扩散，最多7格距离）
    // 简化实现：只检查水源是否能向四周扩散一格
    static const int horizontal[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    // 计算当前水的流动距离（通过向上追溯水源）
    // 简化：水只从水源向外扩散最多 4 格
    int flowDistance = 0;
    int checkY = y;
    // 如果上方是水，说明这是瀑布底部，可以继续扩散
    bool fromAbove = (y + 1 < CHUNK_HEIGHT && world.getBlock(x, y + 1, z) == Block::Water);

    if (!fromAbove) {
        // 计算水平流动距离：从当前位置向四周查找最近的水源
        // 简化：限制水最多扩散 4 格
        flowDistance = countWaterDistance(world, x, y, z);
    }

    if (flowDistance >= 4 && !fromAbove) return; // 已达最大流动距离

    for (auto& dir : horizontal) {
        int nx = x + dir[0];
        int nz = z + dir[1];
        BlockId neighbor = world.getBlock(nx, y, nz);
        if (neighbor == Block::Air) {
            world.setBlock(nx, y, nz, Block::Water);
            scheduleTick(nx, y, nz, Block::Water, 5, currentTick, 1);
        }
    }
}

bool BlockUpdateSystem::isGravityBlock(BlockId id) const {
    return id == Block::Sand || id == Block::Gravel;
}

bool BlockUpdateSystem::isFluid(BlockId id) const {
    return id == Block::Water;
}

int BlockUpdateSystem::countWaterDistance(World& world, int x, int y, int z) const {
    // BFS 查找最近的"水源"（上方有水的水方块或玩家放置的水源）
    // 简化实现：向四周搜索，找到上方有水的格子则认为是水源
    // 返回到最近水源的曼哈顿距离
    // 如果找不到水源，返回一个大值

    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    // BFS，最大搜索 4 格，使用哈希集合加速已访问检查
    struct Pos { int x, z, dist; };
    std::vector<Pos> bfsQueue;
    bfsQueue.reserve(32);
    std::unordered_set<int64_t> visited;

    auto packXZ = [](int px, int pz) -> int64_t {
        return (static_cast<int64_t>(px) << 32) | (static_cast<int64_t>(pz) & 0xFFFFFFFF);
    };

    bfsQueue.push_back({x, z, 0});
    visited.insert(packXZ(x, z));

    int idx = 0;
    while (idx < static_cast<int>(bfsQueue.size())) {
        auto [cx, cz, dist] = bfsQueue[idx++];

        if (dist > 0) {
            // 检查此位置上方是否有水（瀑布水源）
            if (y + 1 < CHUNK_HEIGHT && world.getBlock(cx, y + 1, cz) == Block::Water) {
                return dist;
            }
        }

        if (dist >= 4) continue;

        for (auto& d : dirs) {
            int nx2 = cx + d[0];
            int nz2 = cz + d[1];
            int64_t vkey = packXZ(nx2, nz2);
            if (visited.count(vkey)) continue;

            if (world.getBlock(nx2, y, nz2) == Block::Water) {
                visited.insert(vkey);
                bfsQueue.push_back({nx2, nz2, dist + 1});
            }
        }
    }

    return 7; // 未找到水源，返回大值
}
