# Batch 3.5 — 实体系统基础 + 物品实体（ItemEntity）设计文档

> 这是 Batch 3 完成后插入的小批次。Batch 3 的方块挖掘直接把掉落物塞进背包是临时方案，MC 原版是：方块破坏 → 世界生成物品实体 → 玩家靠近自动拾取。
>
> 本批次为**整个实体生命周期管理系统**打地基，后面所有生物/投掷物/经验球/箭都会复用这一层。

---

## 1. 为什么现在做

1. **MC 原版语义**：挖方块必须掉物品实体，不能瞬移到背包。
2. **架构收益**：Entity 基类一次写完，Batch 4（武器投掷）、Phase 5（生物 AI）、Phase 7（红石弹射物）都不用重构。
3. **测试可见性**：玩家能看到自己挖出来的东西在地上飘，挖掘反馈闭环。

---

## 2. 子系统概览

```
┌────────────────────────────────────────────────────────┐
│ Entity（基类）                                          │
│ ─ 位置 / 速度 / AABB                                    │
│ ─ onGround / inWater 标志                              │
│ ─ tickCount（用于拾取冷却、消亡判定）                    │
│ ─ virtual void tick(World&, EntityManager&)             │
│ ─ virtual void renderPrepare(...) = 0                   │
│ ─ alive 标记（死亡后被 EntityManager 清理）             │
└────────────────────────────────────────────────────────┘
          │
          ▼
┌────────────────────────────────────────────────────────┐
│ ItemEntity（物品实体）                                   │
│ ─ ItemStack stack                                       │
│ ─ pickupDelayTicks（前 20 tick 不可被拾取，避免吸回）     │
│ ─ lifetimeTicks（总寿命 6000 tick = 5 分钟）             │
│ ─ 大小约 0.25×0.25×0.25 的迷你方块                       │
│ ─ 旋转角度（每 tick 匀速，渲染时使用）                    │
│ ─ tick()：应用重力 + AABB 对世界方块的碰撞 + 玩家磁吸拾取  │
└────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────┐
│ EntityManager                                           │
│ ─ 持有所有实体（std::vector<std::unique_ptr<Entity>>）  │
│ ─ tick() 轮询所有实体，收集死亡的一批集中删除            │
│ ─ spawnItem(world pos, ItemStack, optional velocity)    │
│ ─ 远离玩家 > 128 格的实体立即失活（MC 行为）             │
└────────────────────────────────────────────────────────┘
```

---

## 3. 数据结构细节

### 3.1 Entity 基类

```
class Entity {
public:
    glm::vec3 position {};       // 中心点（不是脚底，因为实体尺寸多样）
    glm::vec3 velocity {};
    glm::vec3 halfExtents {};    // AABB 半宽：position ± halfExtents 得 AABB
    bool onGround = false;
    bool alive    = true;
    int  tickCount = 0;

    virtual ~Entity() = default;
    virtual void tick(World& world, EntityManager& mgr, Player& player) = 0;
    virtual EntityKind kind() const = 0;  // 运行时类型分支（避免 dynamic_cast 热路径）

    // Standard physics helper available to all entities: apply velocity with
    // swept-AABB against solid blocks, toggling onGround / clipping velocity.
    void integrateMotion(World& world, float dt);
};

enum class EntityKind : uint8_t { Item, /* Mob, Projectile, XPOrb, ... future */ };
```

`integrateMotion` 复用现有 `Physics::update` 的 AABB 逻辑，抽一个独立版本出来（玩家碰撞和实体碰撞共享同一套代码）。

### 3.2 ItemEntity

```
class ItemEntity : public Entity {
public:
    ItemStack stack;
    int  pickupDelayTicks = 20;  // 1 second before pickup-eligible
    int  lifetimeTicks    = 6000; // 5 minutes; then self-destruct

    // Visual only: spin around Y axis and bob up/down.
    float visualYaw   = 0.0f;    // accumulated radians
    float visualPhase = 0.0f;    // randomized so items don't bob in sync

    void tick(World& world, EntityManager& mgr, Player& player) override;
    EntityKind kind() const override { return EntityKind::Item; }
};
```

物理参数：
- halfExtents = (0.125, 0.125, 0.125)  —— 0.25³ 的迷你方块
- 重力：`velocity.y -= 0.04` per tick（MC 实际 0.04）
- 水平阻力：`velocity.xz *= 0.98`；在地面上额外 `*= 0.6` 快速刹车
- 垂直阻力：`velocity.y *= 0.98`
- 生成初速度：随机 xz ∈ [-0.1, 0.1]，y = 0.2（稍微弹起来）

拾取：
- `pickupDelayTicks > 0` 时不响应
- 否则：若与玩家 AABB 中心距离 < 1.5 格，尝试 `Inventory::addItem(stack)`；全部进了则 `alive = false`；剩余保留到下一 tick 继续吸
- 额外的"磁吸"效果：距离 < 1.5 且已过冷却，每 tick 把 velocity 向玩家方向加一个 0.05 的推力

### 3.3 EntityManager

```
class EntityManager {
    std::vector<std::unique_ptr<Entity>> entities_;
public:
    void tick(World& world, Player& player);
    void spawnItem(const glm::vec3& worldPos, const ItemStack& stack,
                   const glm::vec3& initialVel = {});
    const auto& entities() const { return entities_; }
    void clear();
};
```

- `tick()`：遍历所有实体调用各自 `tick()`，之后 `erase_if(!alive)`
- `spawnItem`：构造 ItemEntity 加到 vector，初速度可选
- 按 MC 规则，当实体距玩家平方距离 > 128² 时直接 `alive=false`

---

## 4. 渲染（最终方案）

**方案 D：每帧合并所有 ItemEntity 为一份大 mesh**

参考 chunk mesh 的思路，但按实体重建：
- 每 render 帧，遍历 EntityManager 中所有 ItemEntity
- 为每个实体，计算其 **迷你方块的 6 面顶点**（0.25³ 立方体，旋转 visualYaw 度，平移到 position）
- 6 面各用**方块自身对应的 tile UV**（通过 `BlockRegistry::get(stack.id→blockId).textures`）
- 所有顶点追加到一个 `std::vector<Vertex>`
- 一次 `uploadMesh` + 一次 `drawIndexed`

优点：
- 一次 GPU upload，一次 draw call（O(1) 无论多少实体）
- 复用现有 3D pipeline 和 vertex/fragment shader（无需改 shader）
- 深度测试、雾效、纹理采样全自动正确
- 代码简单，不需要 push constants

缺点：
- 每帧都要重建 mesh；但顶点数很小（每实体 24 顶点 / 36 索引），60 个实体才 1440 顶点，可忽略
- 如果实体多且静止，会浪费 upload；后续可做"实体只在旋转/移动改变时 dirty"优化

实现文件：`src/renderer/entity_renderer.h/.cpp`（EntityRenderer 类）

**复用已有持久化动态 buffer机制**：和 UIRenderer 一样，分配一份 CPU_TO_GPU 的 dynamic buffer，每帧 memcpy 写入，不走 staging → 零 GPU 阻塞。

---

## 5. Tick 流程集成

```
gameTick() {
    handleTickInput();
    Physics::update(player, world);            // 玩家
    blockInteraction_.tick(...);               // 挖掘（破坏时调 entityMgr.spawnItem）
    entityMgr_.tick(world, player);            // 所有实体推进一步
    ...
}
```

BlockInteraction 的 `completeBreak` 改动：
- 不再调用 `inventory.addItem(stack)`
- 改为调用 `entityMgr.spawnItem(blockCenter, stack)`
- 玩家通过 ItemEntity 自动拾取获得物品

---

## 6. 文件变更清单

**新增**：
- `src/entity/entity.h` — Entity 基类
- `src/entity/entity_manager.h/.cpp` — EntityManager
- `src/entity/item_entity.h/.cpp` — ItemEntity 实现
- `src/renderer/entity_renderer.h/.cpp` — 按帧构建合并 mesh + 一次性上传 + 绘制

**修改**：
- `src/player/physics.h/.cpp` — 抽出 `integrateAABB(world, pos, vel, halfExtents)` 辅助，供 Entity 复用
- `src/game/game.h/.cpp` — 持有 EntityManager + EntityRenderer，tick 里调用，render 里绘制
- `src/game/block_interaction.cpp` — completeBreak 改 spawnItem

> 不需要修改 vulkan_engine 或 shaders——方案 D 完全复用现有 3D 管线。

---

## 7. 验收点

1. 挖一块方块：当前位置生成一个旋转的迷你方块
2. 迷你方块受重力下落，站在地面上
3. 走近：自动吸附进 hotbar 对应 slot，count +1（数字刷新）
4. 丢下去（暂无此功能，后续 Batch 4）或放在山顶让它远离自己 > 128 格：实体自动消失
5. 不同方块掉落：cobblestone（挖 stone）、oak_planks（挖 planks，若放置过）、dirt（挖 grass）

---

## 8. 对第四阶段存档的影响

实体需要持久化：
- 类型 kind
- 位置 / 速度
- 对于 ItemEntity：stack（itemId, count, durability）+ pickupDelayTicks + lifetimeTicks

存档格式在第四阶段 NBT 框架落地时预留 `entities[]` 列表，每项含 kind 标签 + 专属字段。

---

## 9. 不包含在本批次

- 丢弃物品（Q 键从手中丢一个）：Batch 4 一起做，和物品实体共用代码
- 实体与实体之间的碰撞：目前 ItemEntity 互相可穿过（MC 也允许），不处理
- 物品合并（两个同种掉落物靠近自动堆叠）：Phase 9 优化
- 爆炸抛射、被推力等复杂力场：需要时再加
