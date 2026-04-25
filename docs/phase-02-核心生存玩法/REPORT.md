# 第二阶段完成报告

> 截至 2026-04-25 15:54。**第二阶段核心生存玩法全部完成 + 架构优化。**

## 实施总结

| Batch | 功能 | 关键文件 |
|-------|------|---------|
| 0-3.5 | UI管线 / 物品系统 / HUD / 挖掘 / 实体系统 | `ui_renderer`, `item`, `hud`, `block_interaction`, `entity`, `item_entity`, `entity_renderer` |
| 4 | 工具与武器（耐久/丢弃/图标/Viewmodel/攻击冷却） | `item.cpp`(regTool), `hud.cpp`(viewmodel), PNG icons |
| 5 | 制作系统（shaped/shapeless/14条配方/C键自动合成） | `recipe.h/.cpp` |
| 6 | 生命值（20HP/心形图标/摔落伤害/虚空伤害/死亡重生） | `player.h/.cpp` |
| 7 | 饱食度（20点/鸡腿图标/冲刺消耗/回血/饥饿/进食） | `player.h/.cpp` |
| 8 | 背包GUI + 工作台（E键/鼠标拖拽/2×2和3×3合成/右键交互） | `container_screen`, `inventory_screen`, `crafting_screen` |
| 优化 | 架构重构（GUI基类/图标PNG化/ScreenManager/方块交互回调/注册表安全化） | 见下方详细描述 |

## 架构优化详情

### GUI 基类抽取
- `ContainerScreen` 基类：473 行共享逻辑（drawSlot / handleSlotClick / cursorItem / 面板框架）
- `InventoryScreen`：11 行头文件 + 33 行布局代码
- `CraftingScreen`：11 行头文件 + 35 行布局代码
- 新增任意 GUI 界面只需 ~40 行代码

### 图标 PNG 化
- `texture_atlas.cpp`：577行 → 136行（删除230行C++像素数组）
- 所有图标改为 `assets/textures/{items,hud,font}/` 下 PNG 文件
- Atlas 自动扫描 4 个目录（blocks/items/hud/font）
- 新增工具图标：只需放一个 PNG，零 C++ 修改

### ScreenManager + 方块交互回调
- `Game::activeScreen_` 指针替代多个 Screen 成员变量
- `openScreen()` / `closeActiveScreen()` 统一生命周期
- `blockUseHandlers_` 回调表：新增可交互方块 = 一行 lambda
- `handleFrameInput()` 拆分为 `handleGameplayInput()` + `handleRightClick()`，消除 goto

### 注册表安全化
- `BlockRegistry::getIdByName()` / `ItemRegistry::getIdByName()`
- 内部 `nameToId_` 哈希表，注册时自动填充
- 为后续数据驱动（JSON 配置加载）打好基础

## 代码统计

| 指标 | 数值 |
|------|------|
| 源文件 (.cpp + .h) | 55 个 |
| 总代码行数 | ~6,500 行 |
| 方块类型 | 13 种（含工作台） |
| 物品类型 | 24 种 |
| 合成配方 | 14 条 |
| 纹理 PNG | 42 个（15 blocks + 10 items + 7 hud + 10 font） |
| 模块目录 | 11 个（core/engine/game/player/entity/crafting/renderer/world/ui/shaders/tools） |

## 第二阶段未做项（有意延后）

| 功能 | 延后原因 | 计划阶段 |
|------|---------|---------|
| 主手/副手 | 需要装备系统和盾牌 | 后续内容扩展 |
| 装备栏（4个护甲槽） | 需要护甲系统和伤害减免公式 | 后续内容扩展 |
| 挖掘裂纹动画 | 需要额外渲染通道，当前进度条替代可用 | 第三阶段或后续 |
| 方块放置规则（支撑/方向） | 当前方块种类少，不阻塞核心玩法 | 新增方块时按需实现 |
| 方块更新（沙子下落/水流动） | 需要计划刻系统 | 第三阶段（流体/重力方块） |
| 配方书 | 需要完整GUI文字渲染 | 第八阶段 |
| 远程武器（弓/弩） | 需要投射物实体 | 后续内容扩展 |
| 护甲系统/伤害减免 | 需要装备栏先行 | 后续内容扩展 |

## 当前游戏玩法循环

```
砍树 → E打开背包 → 2×2合成木板/棍 → 合成工作台 → 放置
→ 右键工作台打开3×3 → 合成木镐/木斧/木锹 → 挖石
→ 合成石工具 → 摔落受伤 → 吃苹果(树叶掉落)回血
→ 饿了打树叶 → 死了R重生 → 循环
```

## 第三阶段准备情况

**可直接开始的**：
- 洞穴/矿石生成：只需修改 `terrain_generator.cpp`，Block/Item 注册已有 name→ID 安全机制
- 新方块/物品：注册表 + 纹理 PNG 流程已标准化
- 新 GUI 界面：ContainerScreen 基类 + blockUseHandlers 回调就绪

**需要先做基础设施的**：
- 光照系统：需要修改 mesh_builder（顶点增加光照属性）和着色器
- 透明/半透明渲染：需要第二渲染通道
- 昼夜周期：需要天空盒渲染管线
