# Batch 3 — 方块交互增强 设计文档

> 完成 Batch 2（背包 + 快捷栏 + HUD 图标）之后，本批次为方块挖掘/破坏建立完整的"硬度 + 工具 + 进度 + 掉落"循环。
> 对标 Minecraft Java Edition 1.16+ 的挖掘公式。

---

## 1. 子系统概览

```
玩家按住左键
    │
    ▼
BlockInteraction::tick()
    │  raycast → 取目标方块
    │  与上一帧目标比较，不同则重置进度
    │  根据手中工具 + 方块属性算 breakSpeed
    │  累加 progress += breakSpeed * dt
    │  当 progress ≥ 1.0：
    │     1) 取 BlockProperties::drops 决定掉落
    │     2) world.setBlock(target, Air) — 邻居 dirty 由 World 自己处理
    │     3) 把掉落 ItemStack 加入 Inventory（addItem）
    │     4) 工具消耗 1 耐久（如果有 toolType）
    │     5) 重置进度
    │
玩家松开 / 切换目标 / 切换 hotbar → 立即重置进度
```

后续阶段会把"直接进背包"替换为"生成物品实体掉落 → 拾取"，本批次先按文档约定走简化路径。

---

## 2. 数据结构改动

### 2.1 BlockProperties 新增 drops（多条掉落规则）

为支持 `silk_touch` / `时运` / `条件掉落` 等后续扩展，掉落字段不是单个 `ItemId`，而是一组 `BlockDrop` 规则（按声明顺序匹配，第一条命中即用）。

每条规则字段：
- `item`: 掉落 ItemId（0 表示什么都不掉，例如树叶在没有剪刀时不出物品）
- `minCount` / `maxCount`: 掉落数量范围（默认 1/1）
- `requireTool`: 是否需要正确工具才有掉落（默认 false）
- `requiredToolType`: 当 requireTool=true 时要求的 ToolType
- `requiredMiningLevel`: 工具最低挖掘等级（0=木 / 1=石 / 2=铁…）

第二阶段先用最简形式：每个可破坏方块只有一条 drop 规则，`requireTool=false`。

### 2.2 BlockProperties 新增挖掘属性

- `requiredToolType` (ToolType)：能加速挖掘的工具类型（例如 Stone 需要 Pickaxe）
- `requiredMiningLevel` (int)：必须达到该等级才掉落物品（不到等级仍可挖但无掉落，复刻 MC 行为）

### 2.3 ItemProperties 已就绪

`toolType` / `miningSpeed` / `miningLevel` / `durability` 已在 Batch 1 注册时定义，本批次直接使用。

---

## 3. 挖掘公式（参照 MC Java 1.16+ 简化版）

```
baseDamage = 1 / hardness                 // 每 tick 不带工具的进度
if 工具与方块匹配（toolType 一致）:
    speed = item.miningSpeed              // 例：木镐挖石 = 2.0
    if requireTool 且 miningLevel 不足:
        damage = baseDamage / 100         // 几乎挖不动
    else:
        damage = (speed * baseDamage) / 30  // 30 = canHarvest 系数
else:
    damage = baseDamage / 100             // 不匹配工具：极慢
```

补充：
- `damage` 是每 *tick*（1/20 秒）的进度增量。
- `progress` 累加到 ≥ 1.0 时方块破坏。
- `hardness < 0` 视为不可破坏（基岩、水），`damage = 0`。
- `hardness == 0` 视为瞬间破坏（草、火），`damage = 1.0` 一 tick 完成。

第二阶段挖掘等级映射：
- 木镐 = 1（可挖石头）
- 石镐 = 2（可挖铁矿，但本阶段没铁矿）
- 工具 ItemRegistry 中的 `miningLevel` 当前为 0/1，需要在加 1 偏移后做比较，或者直接把 wooden 改成 1、stone 改成 2。

为简单起见，**保持 ItemRegistry 中现有数值**（wooden=0、stone=1），方块的 `requiredMiningLevel` 也按同套规则填（stone=0、ironOre=1）。后续加铁工具时统一 +1 偏移。

---

## 4. BlockInteraction 状态机

### 4.1 状态变量
```
struct BreakState {
    bool      active;          // 是否正在挖掘
    int       blockX, Y, Z;    // 当前目标方块世界坐标
    BlockId   blockId;         // 目标方块 id（用于检测方块被挤掉的情况）
    float     progress;        // 0..1
    ItemId    toolItemId;      // 开始挖掘时手里的工具 id（用于检测中途换工具）
};
```

### 4.2 每 tick 流程（在 game tick 中调用，20Hz）
1. 若 `!leftMouseHeld || !cursorLocked`：reset；return。
2. raycast 取目标方块。无命中或命中 Air：reset；return。
3. 若 `!active` 或 (target / heldItem) 与记录不同：reset 后重新初始化（active=true，progress=0）。
4. 计算 `damage`（按公式，工具用当前 hotbar 选中物品）。
5. 若 `damage <= 0`（不可破坏）：active=false；return。
6. `progress += damage`。
7. 若 `progress >= 1.0`：
   - 决定掉落：根据 BlockProperties::drops + 工具是否满足。
   - 把 drop 加入 inventory（addItem）。剩余暂时丢弃（后续接物品实体）。
   - 若工具有 durability，在 hotbar 槽里 -1，归零则清空。
   - world.setBlock(Air) → World 已自动 markChunkDirty 邻居。
   - reset。

### 4.3 帧级输入对接
- 现有 `handleFrameInput` 中"按下左键 → 立即破坏方块"的逻辑要**移除**——改为只检测左键按下/松开/持续状态，并把状态喂给 BlockInteraction。
- 输入分层：
  - 帧级：单击放置（右键）、ESC、F2、数字键、滚轮、视角
  - 帧级 → 写入 `bool leftMouseHeld_`（每帧从 input.isMouseButtonDown 取）
  - tick 级：BlockInteraction::tick(world, player, inventory, leftMouseHeld_)

放置方块（右键）暂时维持帧级单次触发 + clickCount 防漏（已存在）。后续 Batch 4 再统一改 tick 化。

---

## 5. HUD 进度指示

简化方案（不做裂纹纹理）：
- 在 HUD 上画一条 hotbar 上方的细横条（宽 = hotbar 宽度，高 = 4 * scale），按 `progress` 填充白色 → 红色渐变。
- 仅当 `BreakState::active` 时显示。
- 后续 Batch 完成裂纹纹理（10 阶段）后替换。

---

## 6. 文件变更清单

新增：
- `src/game/block_interaction.h` / `.cpp` — BlockInteraction 类（状态机 + tick）

修改：
- `core/block.h`：BlockProperties 增加 `requiredToolType`、`requiredMiningLevel`、`drops` 字段；新增 `BlockDrop` 结构。
- `core/block.cpp`：`registerDefaults()` 为每个方块填正确的 drops。
- `core/item.h`：ItemStack 新增 `useDurability()` 辅助，统一工具消耗逻辑。
- `game/game.h/.cpp`：删除 frame-level "瞬间破坏" 分支；在 gameTick() 中调用 BlockInteraction。HUD 在 update() 中读 BreakState 决定是否画进度条。
- `ui/hud.h/.cpp`：新增可选的 break-progress 横条绘制接口。

---

## 7. 序列化对接（第四阶段预留）

挖掘状态是**瞬时**的，不需要持久化。需要持久化的是：
- 每个 hotbar 槽位的 `ItemStack.durability`（已经在 Batch 2 数据结构里）

无新增持久化字段。

---

## 8. 验收点（debug 截图）

1. 长按左键石头：HUD 出现进度条，约 1.5 秒后破坏，背包 +1 圆石。
2. 切换到泥土：长按破坏，背包 +1 泥土。
3. 树叶硬度 0.2，无工具也快速破坏 → 暂无掉落（后续接概率掉落树苗/苹果）。
4. 基岩：长按无任何反应，HUD 不出进度条。
5. 中途换 hotbar 槽（不同工具）：进度立即重置。
6. 工具耐久：连续破坏 59 次后木镐消失（耐久 = 0 自动清空）。

完成后更新 `PROGRESS.md` 与 `FEATURES.md`。
