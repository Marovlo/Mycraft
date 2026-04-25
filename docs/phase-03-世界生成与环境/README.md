# 第三阶段：世界生成与环境

> 目标：让世界从"平原+树"进化为有洞穴、矿石、多种地形、光照和昼夜的真实 MC 体验。
> 开发原则：从简单到困难，每一步都能编译运行+看到效果，不做大爆炸式重构。

---

## 开发顺序概览

按 **依赖关系 + 游戏体验增益** 排序，从最独立、最高收益的开始：

| Batch | 内容 | 依赖 | 新增方块/物品 | 预估复杂度 |
|-------|------|------|-------------|-----------|
| 1 | 矿石方块 + 物品 + 纹理 | 无 | ~15 方块、~20 物品 | 低 |
| 2 | 矿脉生成 | Batch 1 | 无 | 中 |
| 3 | 铁/金/钻石工具 + 熔炉 | Batch 1-2 | ~20 物品、1 方块、~30 配方 | 中 |
| 4 | 洞穴生成 | 无 | 无 | 中 |
| 5 | 地表装饰（花/草/蘑菇/仙人掌） | 无 | ~10 方块 | 低 |
| 6 | 光照系统（方块光 + 天空光） | 无（渲染改动） | 火把等 | 高 |
| 7 | 简单群系（平原/森林/沙漠/雪原） | Batch 5 | 少量方块 | 中 |
| 8 | 昼夜循环 + 天空渲染 | Batch 6 | 无 | 高 |
| 9 | 天气系统（雨/雪/雷暴） | Batch 7-8 | 无 | 中 |
| 10 | 透明/半透明渲染（玻璃/水面） | Batch 6 | 玻璃等 | 高 |

---

## Batch 1：矿石方块 + 物品 + 纹理

**目标**：注册所有矿石方块和对应的原矿/锭物品，生成纹理，但暂不生成在世界中。

**新增方块**：
- `Block::CoalOre` / `Block::IronOre` / `Block::GoldOre` / `Block::DiamondOre`
- `Block::RedstoneOre` / `Block::LapisOre` / `Block::EmeraldOre`
- `Block::CopperOre`（1.17+ 风格）
- `Block::DeepslateCoalOre` 等深层变种（可选，优先级低）

**新增物品**：
- 原矿类：`Item::RawIron` / `Item::RawGold` / `Item::RawCopper`
- 锭类：`Item::IronIngot` / `Item::GoldIngot` / `Item::CopperIngot`
- 其他矿物：`Item::Coal` / `Item::Diamond` / `Item::Emerald` / `Item::LapisLazuli` / `Item::Redstone`

**实现步骤**：
1. 用 Python 脚本 `gen_ore_textures.py` 生成 16×16 矿石纹理（在石头底色上画彩色斑点）
2. 在 `block.h` 添加 Block ID，`block.cpp` 注册属性（hardness/requiredTool/miningLevel/drops）
3. 在 `item.h` 添加 Item ID，`item.cpp` 注册属性
4. 编译验证

**矿石属性参考（MC 原版）**：
| 矿石 | 硬度 | 工具 | 最低等级 | 掉落物 |
|------|------|------|---------|--------|
| 煤矿 | 3.0 | 镐 | 木(0) | 煤 ×1 |
| 铁矿 | 3.0 | 镐 | 石(1) | 粗铁 ×1 |
| 金矿 | 3.0 | 镐 | 铁(2) | 粗金 ×1 |
| 钻石矿 | 3.0 | 镐 | 铁(2) | 钻石 ×1 |
| 红石矿 | 3.0 | 镐 | 铁(2) | 红石粉 ×4-5 |
| 青金石矿 | 3.0 | 镐 | 石(1) | 青金石 ×4-8 |
| 绿宝石矿 | 3.0 | 镐 | 铁(2) | 绿宝石 ×1 |

---

## Batch 2：矿脉生成

**目标**：在 `OverworldGenerator` 中按 MC 高度分布规则在地下生成矿脉。

**实现方案**：
- 新增 `generateOres(Chunk&)` 方法，在 `generateTerrain` 之后、`generateTrees` 之前调用
- 每种矿石定义一个 `OreConfig`：{blockId, minY, maxY, veinSize, veinsPerChunk}
- 矿脉形状：以种子点为中心随机扩展的不规则团簇（3D 随机游走），MC 原版也用类似方式
- 用 chunk-local 的伪随机数（基于 seed + chunkX + chunkZ + oreType 的哈希）保证确定性

**高度分布参考**：
| 矿石 | Y范围 | 脉大小 | 每区块次数 |
|------|-------|--------|-----------|
| 煤 | 0-128 | 17 | 20 |
| 铁 | -64-72 | 9 | 20 |
| 金 | -64-32 | 9 | 2 |
| 钻石 | -64-16 | 8 | 1 |
| 红石 | -64-16 | 8 | 8 |
| 青金石 | -32-32 | 7 | 1 |
| 绿宝石 | -16-48 | 1 | 1 |

（注：当前 CHUNK_HEIGHT=256 且无负 Y，所有 Y 值偏移 +64 映射到 0-based）

---

## Batch 3：铁/金/钻石工具 + 熔炉

**目标**：完整的工具链 + 熔炼系统。

**新增物品**（仿 Phase 2 regTool 模式）：
- 铁工具套（镐/斧/锹/剑/锄）× 5 = 5 物品
- 金工具套 × 5 = 5 物品
- 钻石工具套 × 5 = 5 物品
- 对应 15 张工具 PNG 图标（用 gen_icon_pngs.py 扩展，只需改颜色）

**新增方块 + GUI**：
- `Block::Furnace` — 可交互方块，右键打开 `FurnaceScreen`
- `FurnaceScreen : ContainerScreen` — 1 个输入格 + 1 个燃料格 + 1 个输出格 + 进度条
- 燃烧逻辑：在 `gameTick()` 中驱动（燃料消耗 + 冶炼进度），状态存在方块实体或 FurnaceScreen 内

**新增配方**：
- 铁/金/钻石工具各 5 种 = 15 条 shaped 配方
- 冶炼：粗铁→铁锭、粗金→金锭（熔炉专用配方，与 RecipeRegistry 分开存储或加 recipeType 标签）

**实现步骤**：
1. 注册铁/金/钻石工具物品 + 图标 PNG
2. 注册工具配方
3. 注册 `Block::Furnace` + 纹理
4. 实现 `FurnaceScreen`（继承 ContainerScreen，覆写 getCraftGridDims={0,0}，自定义 slot 布局）
5. 实现冶炼逻辑（SmeltingRecipe 结构 + tick 驱动）

---

## Batch 4：洞穴生成

**目标**：地下有可探索的洞穴网络。

**实现方案**：
- **噪声虫洞（Perlin Worm）**：3D Perlin 噪声值 < 阈值处镂空为 Air
- 新增 `caveNoise_` FastNoiseLite 实例（3D OpenSimplex2，频率 ~0.02）
- 在 `generateTerrain()` 的石头层生成后，对每个方块检查 `caveNoise_.GetNoise(wx, y, wz)` 是否 < 阈值（约 -0.35）
- 洞穴不穿透海平面以下的水体（y < SEA_LEVEL 时不镂空水旁的方块）
- 洞穴不穿透最底层基岩

**性能考虑**：
- 3D 噪声采样比 2D 慢，但 FastNoiseLite 内部优化充分
- 只在石头层（非 Air/Water/Bedrock）的方块上做洞穴检查，减少采样量
- 每区块约 ~60,000 次 3D 噪声查询（16×16 × ~230 非空层），在现代 CPU 上 <1ms

---

## Batch 5：地表装饰

**目标**：花、草丛、蘑菇、仙人掌等让地表不那么单调。

**新增方块**（非全尺寸方块，渲染为交叉面片 X 形）：
- `Block::TallGrass` / `Block::Poppy` / `Block::Dandelion` / `Block::BlueOrchid`
- `Block::BrownMushroom` / `Block::RedMushroom`
- `Block::Cactus`（全尺寸，带伤害 — 后续）
- `Block::DeadBush`

**渲染**：装饰方块用 `BlockRenderType::Cross`（两个交叉面片），需要在 `MeshBuilder::build()` 中添加 Cross 类型的面生成逻辑。

**生成规则**：
- 在 `generateTrees()` 之后新增 `generateVegetation(Chunk&)`
- 根据地表方块类型（草方块→花/草，沙子→仙人掌/枯灌木）随机放置
- 噪声 + 密度参数控制分布

---

## Batch 6：光照系统

**目标**：方块光源（火把等）+ 天空光 + 着色器光照。这是第三阶段最大的单项工作。

**数据结构**：
- 每方块存 2 个 4-bit 值：`blockLight`（0-15）和 `skyLight`（0-15）
- Chunk 中新增 `std::array<uint8_t, BLOCK_COUNT/2> lightData_`（每字节存两个方块的光照）
- 或简化为 `std::array<uint8_t, BLOCK_COUNT> lightData_`（1 byte per block: high nibble=sky, low nibble=block）

**传播算法**：BFS
- 光源放置/移除时，从光源出发 BFS 传播（每步衰减 1）
- 天空光从最高非透明方块向下传播（垂直不衰减，水平衰减 1）

**渲染**：
- 顶点增加 `uint8_t lightLevel` 属性（或压进现有 normal 的未使用分量）
- 片段着色器中 `finalColor = textureColor * lightMultiplier(lightLevel)`
- 光照贴图：lightLevel 0=最暗(0.05), 15=最亮(1.0)

**新增方块**：`Block::Torch`（light=14）、`Block::Glowstone`（light=15）

**实现步骤（建议细分 Sub-batch）**：
1. 6a：数据结构 + 天空光传播（仅影响 mesh 构建时的光照值查询）
2. 6b：方块光传播（火把放置/移除 → BFS）
3. 6c：着色器集成（顶点属性 + 片段着色器乘法）
4. 6d：光照更新触发（方块放置/破坏时重新传播邻域）

---

## Batch 7：简单群系

**目标**：4 种基础群系让世界有地理变化。

**群系定义**：温度 + 湿度 → 群系类型
- **平原**（温暖+中湿）：当前默认地形
- **森林**（温暖+高湿）：更密树木
- **沙漠**（炎热+低湿）：全沙地表，无树，仙人掌
- **雪原**（寒冷+低湿）：雪块覆盖地表，针叶林（云杉树）

**实现**：
- 噪声生成温度/湿度场：`temperatureNoise_` / `humidityNoise_`
- `getBiome(wx, wz)` → `enum Biome { Plains, Forest, Desert, Snowy }`
- 群系影响：地表方块选择、树木密度/类型、装饰物种类
- 群系过渡：温度/湿度是连续的噪声值，自然实现平滑过渡

**新增方块**：`Block::Snow` / `Block::SnowBlock` / `Block::SpruceLog` / `Block::SpruceLeaves` / `Block::Sandstone`

---

## Batch 8-10（概述，到时再细化）

- **Batch 8**：昼夜循环（24000 tick/天 + 天空色渐变 + 太阳/月亮渲染）
- **Batch 9**：天气系统（粒子降雨/降雪 + 天空变暗）
- **Batch 10**：透明渲染（玻璃 + 改进的水面 + alpha blend 排序渲染通道）

---

## 关键设计原则

1. **每个 Batch 独立可运行**：完成后能编译+跑+看到效果，不会出现"前半截做完但没法测"的情况
2. **方块/物品注册用已有流程**：Block/Item constexpr + registerDefaults() + PNG 纹理，不改注册架构
3. **新增 GUI 继承 ContainerScreen**：熔炉等只需 ~50 行子类
4. **新增可交互方块用回调表**：`blockUseHandlers_[Block::Furnace] = ...` 一行搞定
5. **地形生成在 TerrainGenerator 中扩展**：不重写，新增 generate 子方法
6. **光照系统是最大风险项**：涉及数据结构、mesh 构建、着色器三层改动，建议分 4 个 sub-batch 做
7. **性能红线**：矿脉+洞穴生成不能让区块加载时间 >5ms；光照 BFS 每次方块变更 <1ms
