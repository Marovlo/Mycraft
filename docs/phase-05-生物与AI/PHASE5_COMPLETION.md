# Phase 5 补完 + 跨阶段遗留工作 — 开发文档

> **目标**：补齐 Phase 5 生物与AI 的剩余细节，以及 Phase 1-4 中适合当前阶段完成的遗留功能。
> 使项目在进入 Phase 6（维度与高级世界）之前，拥有一个完整、稳固的基础。
>
> **原则**：[[memory:k3om2260]] 一步到位，不写会被推翻的代码。

---

## 一、当前已完成功能回顾

### Phase 5 已完成的 Batch

| Batch | 内容 | 完成度 | 说明 |
|-------|------|--------|------|
| Batch 8 | 昼夜循环 | ✅ 完成 | 24000 tick 周期，天空/雾色插值，skyLight 因子 |
| Batch 1 | 生物基础框架 | ✅ 完成 | MobEntity、MobRegistry(8种)、碰撞箱/受击箱、生命值 |
| Batch 2 | AI 基础 | ✅ 基本完成 | FSM 7 状态、简化直线寻路+跳跃、悬崖检测 |
| Batch 3 | 被动生物 | ✅ 完成 | 猪/牛/羊/鸡、原版贴图+UV、行走动画、受伤逃跑恐慌加速 |
| Batch 4 | 敌对生物 | ✅ 基本完成 | 僵尸/骷髅/蜘蛛/苦力怕、原版贴图+UV、追踪玩家 |
| Batch 5 | 战斗系统 | ✅ 基本完成 | 左键攻击、击退、无敌帧、死亡动画、受伤闪红 |
| Batch 6 | 生物生成规则 | ✅ 基本完成 | 初始+自然生成、消失规则、/summon 命令 |
| Batch 7 | 掉落物+经验值 | ⚠️ 部分完成 | 掉落物已实现(spawnLoot)，**经验值系统未实现** |

### 额外已完成

- 游戏控制台 (`GameConsole`) — `/summon`、`/time`、`/kill` 等命令
- 原版 MC 资源文件已全部导入（3600+ PNG 纹理）

---

## 二、Phase 5 剩余工作（本次补完）

### Batch A：A* 寻路系统

**现状**：当前生物使用简化直线寻路（`moveToward` 直线移动 + 障碍跳跃），无法绕过障碍物。

**目标**：实现简化 A* 寻路，使生物能绕过墙壁、水域等障碍。

**实现要点**：
- 以方块为单位的 2D 网格寻路（XZ 平面）
- 可行走判断：脚下实心 + 头部空间足够（根据生物高度）
- 路径长度限制：最多 16 格（性能考虑）
- 每 20 tick 重新寻路一次（已有 `pathUpdateTimer` 框架）
- 被动生物悬崖检测：不走下 3 格以上的悬崖
- 水域检测：陆地生物避开水

**新增/修改文件**：
- `src/entity/mob_entity.cpp` — 替换 `moveToward` 中的直线逻辑为 A* 路径跟随
- 寻路逻辑直接内联在 `mob_entity.cpp` 中（不需要独立文件，规模不大）

**预估工作量**：中等

---

### Batch B：骷髅远程射箭（箭矢实体）

**现状**：骷髅攻击是"简化远程"——直接对玩家造成伤害，无箭矢飞行过程。

**目标**：实现箭矢实体（ArrowEntity），骷髅射出箭矢，箭矢飞行后命中玩家造成伤害。

**实现要点**：
- `ArrowEntity` 继承 `Entity`
  - 属性：发射方向、速度、重力、伤害值、存活时间
  - 物理：抛物线飞行（受重力影响）
  - 碰撞：与方块碰撞后停止（插在方块上）、与玩家碰撞后造成伤害+击退
  - 渲染：简单的长条模型（朝飞行方向旋转）
- 骷髅 `tickCombat` 改为生成 `ArrowEntity` 而非直接伤害
- 箭矢纹理：使用原版 `arrow.png`
- 箭矢在地上存在 60 秒后消失
- 玩家可拾取地上的箭矢（加入背包）

**新增文件**：
- `src/entity/arrow_entity.h/.cpp` — 箭矢实体

**修改文件**：
- `src/entity/mob_entity.cpp` — 骷髅攻击逻辑
- `src/entity/entity_manager.h/.cpp` — 管理箭矢实体
- `src/renderer/mob_renderer.cpp` 或新增 `arrow_renderer` — 箭矢渲染

**预估工作量**：较大

---

### Batch C：蜘蛛特殊行为

**现状**：蜘蛛只有基础的追踪+近战攻击，无爬墙和白天中立行为。

**目标**：实现蜘蛛的两个标志性行为。

**实现要点**：
1. **爬墙**：
   - 蜘蛛碰到垂直方块面时，velocity.y 设为正值（向上攀爬）
   - 攀爬速度 = 水平移速
   - 到达顶部后恢复正常移动
2. **白天中立**：
   - 白天（`dayNight.isDay()`）蜘蛛不主动追踪玩家
   - 被攻击后仍会反击（进入 Chase 状态）
   - 夜晚正常敌对

**修改文件**：
- `src/entity/mob_entity.cpp` — `tickHostileAI` 中蜘蛛分支

**预估工作量**：小

---

### Batch D：苦力怕爆炸粒子效果（预留）

**现状**：苦力怕爆炸已实现（范围伤害+破坏方块），但没有视觉爆炸效果。

**说明**：爆炸粒子效果依赖粒子系统（Batch H），此处仅标记依赖关系。苦力怕爆炸的核心逻辑已完成，粒子效果在粒子系统实现后补充。

---

### Batch E：阳光燃烧与昼夜联动验证

**现状**：`tickBurning` 已实现（检查头顶遮挡 + 每秒1点伤害），但未与昼夜循环的 `skyLightFactor` 联动——当前只检查头顶是否有遮挡，不检查是否是白天。

**目标**：燃烧条件 = 白天 + 头顶无遮挡。

**实现要点**：
- `tickBurning` 需要接收 `DayNightCycle` 引用
- 燃烧条件：`dayNight.isDay() && exposed`
- 夜晚或阴天（未来天气系统）不燃烧
- 僵尸/骷髅头戴头盔时不燃烧（未来装备系统预留）

**修改文件**：
- `src/entity/mob_entity.h/.cpp` — `tickBurning` 签名和逻辑

**预估工作量**：小

---

### Batch F：经验值系统

**现状**：完全未实现。

**目标**：实现经验球实体、经验等级、经验条 HUD。

**实现要点**：
1. **经验球实体** (`ExperienceOrbEntity`)：
   - 击杀生物时生成（不同生物不同经验值）
   - 物理：受重力影响，落地后弹跳
   - 磁吸：靠近玩家时飞向玩家（类似掉落物）
   - 拾取：接触玩家后增加经验值
2. **经验等级**：
   - MC 原版公式：0-16 级每级 2*level+7 经验，17-31 级每级 5*level-38，32+ 级每级 9*level-158
   - `Player` 新增 `experienceLevel`、`experienceProgress`、`totalExperience`
3. **经验条 HUD**：
   - 快捷栏上方显示经验条（绿色进度条 + 等级数字）
4. **掉落经验表**：
   - 被动生物：1-3 经验
   - 僵尸/骷髅/蜘蛛：5 经验
   - 苦力怕：5 经验
5. **死亡掉落经验**：玩家死亡时掉落部分经验

**新增文件**：
- `src/entity/experience_orb.h/.cpp` — 经验球实体

**修改文件**：
- `src/player/player.h/.cpp` — 经验属性
- `src/entity/mob_entity.cpp` — 死亡时生成经验球
- `src/ui/hud.cpp` — 经验条渲染
- `src/entity/entity_manager.h/.cpp` — 管理经验球实体
- `src/world/save_manager.cpp` — 经验值持久化

**预估工作量**：较大

---

### Batch G：生物持久化（存档）

**现状**：生物数据未纳入存档系统，退出游戏后所有生物消失。

**目标**：生物数据随区块保存/加载。

**实现要点**：
1. **序列化格式**：
   - 每个区块保存其范围内的生物列表
   - 每只生物：MobType、位置、速度、HP、AIState、bodyYaw、fireTicks 等
2. **保存时机**：
   - 区块卸载时保存其中的生物
   - 自动保存时保存所有脏区块的生物
3. **加载时机**：
   - 区块加载时恢复其中的生物
4. **消失规则兼容**：
   - 加载后的生物仍遵循消失规则

**修改文件**：
- `src/entity/mob_entity.h/.cpp` — 序列化/反序列化方法
- `src/entity/entity_manager.h/.cpp` — 按区块保存/加载生物
- `src/world/save_manager.cpp` — 生物数据文件管理
- `src/world/chunk_serializer.cpp` — 区块序列化中包含生物数据

**预估工作量**：中等

---

## 三、跨阶段遗留工作（适合当前做）

以下是 Phase 1-4 中延后的功能，它们不依赖后续阶段的系统，且对当前游戏体验有显著提升。

### Batch H：粒子系统

**原属阶段**：Phase 1

**现状**：完全未实现。

**为什么现在做**：
- 方块破坏碎片、苦力怕爆炸、火焰、受伤效果等都需要粒子
- 是纯渲染层功能，不依赖任何后续系统
- 对游戏"手感"提升巨大

**实现要点**：
1. **粒子系统框架**：
   - `ParticleEmitter` — 粒子发射器（位置、方向、速率、生命周期）
   - `Particle` — 单个粒子（位置、速度、颜色、大小、剩余生命）
   - 粒子池（预分配，避免每帧 new/delete）
   - 最大粒子数限制（如 2000）
2. **粒子类型**：
   - 方块破坏碎片：使用方块纹理的随机小块
   - 暴击星星：黄色星形粒子
   - 火焰/烟雾：火把、熔炉
   - 爆炸：苦力怕爆炸的烟雾+碎片
3. **渲染**：
   - Billboard 面片（始终朝向摄像机）
   - 使用现有着色器 + 粒子纹理图集
   - 半透明排序

**新增文件**：
- `src/renderer/particle_system.h/.cpp` — 粒子系统

**预估工作量**：较大

---

### Batch I：天空盒渲染（日月星空）

**原属阶段**：Phase 1

**现状**：完全未实现。当前只有纯色天空（由 `DayNightCycle::getSkyColor()` 驱动 clearColor）。

**为什么现在做**：
- 昼夜循环已完成，天空盒是其自然延伸
- `getSunAngle()` 已实现但无处使用
- 对视觉效果提升极大

**实现要点**：
1. **天空渲染管线**：
   - 独立的 Vulkan 管线（无深度写入，最先渲染）
   - 全屏四边形或球体网格
   - 天空颜色渐变（天顶→地平线）
2. **太阳/月亮**：
   - Billboard 面片，跟随 `getSunAngle()` 旋转
   - 使用原版 `sun.png` 和 `moon_phases.png`
3. **星空**：
   - 夜晚显示随机星点（小白色面片）
   - 随时间缓慢旋转

**新增文件**：
- `src/renderer/sky_renderer.h/.cpp` — 天空渲染器
- 可能需要新的着色器：`sky.vert` / `sky.frag`

**预估工作量**：较大

---

### Batch J：纹理动画（水/岩浆）

**原属阶段**：Phase 1

**现状**：完全未实现。水面和岩浆是静态纹理。

**为什么现在做**：
- 纹理图集系统已完成
- 水面已有透明渲染
- 实现简单，效果显著

**实现要点**：
1. **帧动画系统**：
   - 原版 MC 的水/岩浆纹理是多帧 PNG（如 `water_still.png` 是 16×512 = 32 帧）
   - 每 2 tick 切换一帧
   - 更新纹理图集中对应区域的像素数据
2. **实现方式**：
   - 方案 A：每帧更新纹理图集的子区域（`vkCmdCopyBufferToImage` 局部更新）
   - 方案 B：使用纹理数组，着色器中根据帧索引采样

**修改文件**：
- `src/renderer/texture_atlas.h/.cpp` — 动画帧管理
- 着色器可能需要传入帧索引 uniform

**预估工作量**：中等

---

### Batch K：F3 调试屏幕

**原属阶段**：Phase 1

**现状**：有基础字体渲染能力（UIRenderer），有部分 F 键调试功能（`game_debug.cpp`），但无完整 F3 调试屏。

**为什么现在做**：
- 文字渲染基础已有
- 对开发调试极其有用
- 工作量不大

**实现要点**：
- F3 切换显示/隐藏
- 显示信息：
  - FPS / TPS
  - 玩家坐标 (X, Y, Z)
  - 朝向 (Yaw, Pitch)
  - 所在区块坐标
  - 所在群系
  - 光照等级（脚下方块的 skyLight + blockLight）
  - 已加载区块数
  - 实体数量（掉落物 + 生物）
  - 内存使用（GPU 缓冲区大小）
  - 昼夜时间
  - 渲染距离

**修改文件**：
- `src/game/game_debug.cpp` — F3 逻辑
- `src/ui/hud.cpp` — 调试信息渲染

**预估工作量**：小

---

### Batch L：方块更新系统（沙子下落 + 水流动）

**原属阶段**：Phase 2

**现状**：完全未实现。沙子放置后悬浮，水不会流动。

**为什么现在做**：
- 是核心 MC 体验的一部分
- 不依赖红石等高级系统
- 需要计划刻系统（`ScheduledTick`），这也是后续红石的前置

**实现要点**：
1. **方块更新机制**：
   - 放置/破坏方块时，通知相邻 6 个方块进行更新
   - 每种方块定义 `onNeighborChanged` 行为
2. **计划刻**：
   - `ScheduledTick` 队列：(位置, 延迟tick数, 优先级)
   - 每 tick 处理到期的计划刻
3. **沙子/砂砾下落**：
   - 下方为空气/水 → 变为下落实体（`FallingSandEntity`）
   - 下落实体碰到实心方块 → 放置为方块
4. **水流动**：
   - 水源方块向四周扩散（7 格范围，每格衰减 1 级）
   - 向下无限流动
   - 移除水源后，流动水逐渐消失
   - 水+岩浆交互：生成圆石/黑曜石/石头

**新增文件**：
- `src/world/block_update.h/.cpp` — 方块更新 + 计划刻系统
- `src/entity/falling_block_entity.h/.cpp` — 下落方块实体

**修改文件**：
- `src/world/world.h/.cpp` — `setBlock` 触发邻居更新
- `src/core/block.h/.cpp` — 方块更新行为定义

**预估工作量**：大

---

## 四、建议实施顺序

按照依赖关系和性价比排序：

```
优先级 1（小改动，立竿见影）：
  Batch E: 阳光燃烧与昼夜联动 .............. 小
  Batch C: 蜘蛛特殊行为 .................... 小
  Batch K: F3 调试屏幕 ..................... 小

优先级 2（中等改动，核心体验）：
  Batch A: A* 寻路系统 ..................... 中等
  Batch G: 生物持久化 ...................... 中等
  Batch J: 纹理动画（水/岩浆）.............. 中等

优先级 3（较大改动，重要功能）：
  Batch B: 骷髅远程射箭 .................... 较大
  Batch F: 经验值系统 ...................... 较大
  Batch I: 天空盒渲染 ...................... 较大
  Batch H: 粒子系统 ....................... 较大

优先级 4（大改动，可分步）：
  Batch L: 方块更新系统 .................... 大
```

**推荐执行路线**：
```
E → C → K → A → G → J → B → F → I → H → L
```

---

## 五、延后到其他 Phase 的工作

以下功能虽然也是遗留的，但依赖后续阶段的系统，不适合现在做：

| 功能 | 原属阶段 | 建议归入 | 原因 |
|------|---------|---------|------|
| 装备栏（护甲 4 槽） | Phase 2 | Phase 6 或 7 | 需要护甲渲染（模型外层）、伤害减免公式，与附魔系统关联 |
| 远程武器（弓/弩） | Phase 2 | Phase 6 或 7 | 依赖箭矢实体（Batch B 完成后可提前），需要蓄力机制 |
| 方块放置规则（支撑/方向性） | Phase 2 | Phase 7 | 与红石组件（活塞、中继器方向）强关联 |
| 天气系统（雨/雪/雷暴） | Phase 3 | Phase 6 | 需要粒子系统（Batch H）、天空盒（Batch I）作为前置 |
| 透明排序优化 | Phase 3 | Phase 8 | 当前粗排序可用，精细排序是优化工作 |
| 子区块优化（16³） | Phase 1 | Phase 8 | 性能优化，当前不是瓶颈 |
| 区块加载优先级（视线方向） | Phase 1 | Phase 8 | 性能优化 |
| GPU 上传优化（fence 异步） | Phase 4 | Phase 8 | 性能优化，当前 `vkQueueWaitIdle` 可用 |

---

## 六、项目统计（当前）

| 指标 | 数值 |
|------|------|
| 源文件数 (.h + .cpp) | 88 |
| 总代码行数 | ~16,300 |
| 方块种类 | 35 |
| 物品种类 | 60 |
| 合成配方 | 32 |
| 熔炼配方 | 6 |
| 生物种类 | 8（4 被动 + 4 敌对） |
| 纹理 PNG | 3600+（含原版 MC 全部资源） |
| 着色器 | 4 (basic.vert/frag + ui.vert/frag) |

---

## 七、完成标准

每个 Batch 完成后需满足：
1. 编译通过，无警告
2. 功能可通过游戏内验证（手动测试）
3. 性能无明显退化（维持 60 FPS @ 200 只生物）
4. 代码风格与现有代码一致

---

## 八、Phase 5 Extra 实现记录

> 以下为 Phase 5 补完阶段的实际开发记录，包含所有已实现的功能和优化。

---

### Extra 1：完整弓箭系统（MC 原版逻辑）

**日期**：2026-04-30

**实现内容**：

| 功能 | 说明 |
|------|------|
| 玩家弓蓄力系统 | 右键持弓 → 检查背包有箭矢 → 开始蓄力（0~20 tick） → 松开射箭 |
| MC 原版蓄力公式 | `chargeRatio = min(ticks/20, 1.0)`，`speed = ratio*(ratio+2)/3*3.0`，`damage = floor(speed²+0.5)` |
| 最小蓄力门槛 | 蓄力 ≥ 3 tick 才能射出箭矢，否则取消 |
| 满蓄力暴击 | speed ≥ 3.0 时为暴击箭（额外伤害） |
| 非满蓄力散布 | 非满蓄力时箭矢有随机偏移，满蓄力精准 |
| 弓耐久消耗 | 每次射箭消耗 1 点耐久（最大 384），耐久归零弓损坏 |
| 第一人称拉弓动画 | 手臂向左上方移动 + 弓贴图切换（bow → pulling_0/1/2） + 满蓄力抖动 |
| 骷髅拉弓动画 | 20 tick 蓄力延迟 + 右臂逐渐举起到水平 + 左臂辅助拉弦 |
| 原版贴图资源 | 从 MC 原版复制 bow_pulling_0/1/2.png |

**修改文件**：
- `src/player/player.h` — 添加弓蓄力状态字段和方法
- `src/player/player.cpp` — 实现蓄力公式
- `src/game/game.h` — 添加 `releaseBow()` 声明
- `src/game/game.cpp` — 右键蓄力逻辑 + 松开射箭 + `releaseBow()` 实现
- `src/renderer/player_renderer.cpp` — 拉弓动画 + 弓贴图切换
- `src/entity/ai_goals.h` — `RangedAttackGoal` 支持蓄力状态
- `src/entity/ai_goals.cpp` — 骷髅 20 tick 蓄力延迟
- `src/entity/mob_entity.h` — 添加 `bowChargeTicks`/`isChargingBow` 字段
- `src/renderer/mob_renderer.cpp` — 骷髅拉弓手臂动画

---

### Extra 2：GUI 贴图系统（原版 MC 精灵图）

**日期**：2026-04-30

**实现内容**：

| 功能 | 说明 |
|------|------|
| GuiAtlas 系统 | 独立的 GUI 纹理图集，自动扫描 hud/container/widget/title 目录 |
| UIRenderer 双阶段渲染 | 先渲染方块图集内容，再切换 descriptor set 渲染 GUI 图集 |
| HUD 贴图化 | hotbar.png、hotbar_selection.png、crosshair.png、heart 系列、food 系列、experience_bar、air 气泡 |
| 容器界面贴图化 | 使用 container/slot.png 替代纯色矩形格子 |
| 主菜单贴图化 | title/minecraft.png Logo + widget/button 系列按钮精灵图 |
| 世界选择/创建界面 | 按钮使用原版 widget/button_highlighted/disabled 精灵图 |

**新增文件**：
- `src/renderer/gui_atlas.h/.cpp` — GUI 纹理图集系统

**修改文件**：
- `src/renderer/ui_renderer.h/.cpp` — 添加 GUI 精灵绘制 + 双阶段 flush
- `src/ui/hud.h/.cpp` — 完全重写，使用原版精灵图
- `src/ui/container_screen.cpp` — drawSlot 使用 slot.png
- `src/ui/main_menu_screen.cpp` — Logo + 按钮贴图化
- `src/game/game.h/.cpp` — 初始化 GuiAtlas

---

### Extra 3：洞穴环境音效系统

**日期**：2026-04-30

**实现内容**：

| 功能 | 说明 |
|------|------|
| MC 原版触发机制 | 每 tick 1/6000 概率触发 |
| 光照检测 | 玩家周围 16 格范围内随机采样 10 次，找到光照 ≤ 3 的空气方块 |
| 3D 空间音效 | 从暗处方向播放洞穴音效（cave1~23.ogg） |
| 天气音效预留 | 雨声/雷声接口已实现，天气系统未实现时不触发 |

**新增文件**：
- `src/game/game_ambient.cpp` — 洞穴/天气环境音效

**修改文件**：
- `src/audio/sound_engine.h/.cpp` — 注册 AmbientCave/Rain/Thunder 事件

---

### Extra 4：可扩展性全面重构

**日期**：2026-04-30

**重构内容**：

| 模块 | 重构前 | 重构后 | 扩展方式 |
|------|--------|--------|---------|
| 方块音效材质 | BlockSoundMap 硬编码 86 行映射 | BlockProperties.soundMaterial | 注册方块时一处设置 |
| 生物属性注册 | 固定大小数组 `mobs_[COUNT]` | `std::vector<MobProperties>` | push 新属性即可 |
| 生物掉落物 | 40 行 switch/case | `MobProperties::lootTable` | 注册时声明 lootTable |
| 生物音效 | 3 个 switch/case | `MobProperties::sounds` | 注册时声明 sounds |
| 生物模型 | 固定数组 `models_[COUNT]` | `std::vector<MobModelDef>` | resize + 填充 |
| 生物 AI | 巨大的 tickPassiveAI/tickHostileAI | AIGoal 组件系统 | 新建 AIGoal 子类 |

**新增文件**：
- `src/audio/sound_material.h` — 独立的 SoundMaterial 枚举
- `src/entity/ai_goals.h/.cpp` — AI 目标组件系统（6 个具体组件 + A* 寻路）

**修改文件**：
- `src/core/block.h/.cpp` — BlockProperties 添加 soundMaterial 字段
- `src/audio/sound_engine.h` — 引用 sound_material.h
- `src/audio/block_sound_map.cpp` — 从 BlockRegistry 读取（25 行替代 86 行）
- `src/entity/mob_entity.h/.cpp` — 完全重写，数据驱动 + AI 组件化
- `src/renderer/mob_renderer.h/.cpp` — 动态容器

---

### Extra 5：性能优化代码审阅

**日期**：2026-04-30

**已确认的优化（已在代码中）**：

| 优化项 | 位置 | 效果 |
|--------|------|------|
| 活跃生物索引缓存 | entity_manager.cpp | O(n²) 碰撞推挤中避免重复判断 kind/alive |
| 距离平方预筛选 | entity_manager.cpp | 碰撞检测避免不必要的 sqrt |
| mt19937 替代 rand() | entity_manager.cpp, particle_system.cpp | 更好的随机质量 + 线程安全 |
| 像素缓存 (getOrBuildPixelCache) | player_renderer.cpp | 避免每帧重新扫描 256 像素 |
| 列向量替代矩阵乘法 | player_renderer.cpp | 手持物品渲染快 ~3x |
| compact 方式移除死亡粒子 | particle_system.cpp | O(n) 无需移动存活粒子 |
| unordered_set O(1) 查找 | block_update_system.h | 防止重复计划刻的查找从 O(n) → O(1) |
| 确定性散射 (position hash) | entity_manager.cpp | 掉落物散射无需 RNG 调用 |

**审阅结论**：
- 所有性能关键路径已优化
- 剩余 `std::rand()` 调用（27 处）在低频路径中（生物生成每 400 tick、AI 状态切换），不是瓶颈
- 粒子系统有 2048 上限保护
- 实体系统每帧只遍历活跃实体
- 碰撞检测使用距离平方预筛选避免 sqrt

---

### 项目统计更新

| 指标 | 数值 |
|------|------|
| 源文件数 (.h + .cpp) | 92 |
| 总代码行数 | ~18,500 |
| 方块种类 | 35 |
| 物品种类 | 60 |
| 合成配方 | 32 |
| 熔炼配方 | 6 |
| 生物种类 | 8（4 被动 + 4 敌对） |
| AI 目标组件 | 7（Wander/Flee/Chase/Melee/Ranged/Explode/SpiderNeutral） |
| 纹理 PNG | 3600+（含原版 MC 全部资源） |
| GUI 精灵图 | 50+（hotbar/heart/food/xp/crosshair/button/slot/title） |
| 音效事件 | 30+（方块/生物/环境/UI） |
| 着色器 | 4 (basic.vert/frag + ui.vert/frag) |
