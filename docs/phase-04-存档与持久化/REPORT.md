# Phase 4 完成报告：存档与持久化 + 多线程区块系统 + 代码重构

> **完成时间**：2026-04-25
> **状态**：✅ 全部完成

---

## 一、Phase 4 存档与持久化 — 完成情况

### Batch 1 ✅ 二进制序列化框架
- `src/core/serialization.h/.cpp` — `BinaryWriter` / `BinaryReader`
- 小端字节序，纯 C++ fstream，支持 U8/U16/U32/I32/I64/F32/String/Bytes
- 所有文件使用 Magic + Version 头部

### Batch 2 ✅ 玩家数据保存/加载
- `SaveManager::savePlayer()` / `loadPlayer()` — 位置、朝向、HP、饥饿、空气、背包 36 槽
- 文件路径：`saves/<world>/player.dat`

### Batch 3 ✅ 区块序列化
- `ChunkSerializer::serialize()` / `deserialize()` — 调色板压缩格式
- 只保存被修改过的区块（`Chunk::isModified_`）
- 光照不持久化，加载时重算

### Batch 4 ✅ 区域文件系统
- `RegionFile` — 32×32 区块打包到 `.mca` 文件
- 4KB 扇区对齐，Header 1024 条目
- `SaveManager` 管理 RegionFile 缓存

### Batch 5 ✅ 自动保存 + 退出保存
- 每 6000 tick（5 分钟）自动保存玩家数据 + level.dat
- 每帧增量保存最多 2 个脏区块（分摊 IO，不卡帧）
- `Game::~Game()` 中 `saveAll()` 保存所有数据
- 原子写入：先写 `.tmp` 再 rename

### Batch 6 ✅ 调色板压缩
- 区块通常只用 5-15 种方块 → 3-4 bit/方块
- 128 KB → ~24 KB（压缩率 80%+）
- `ChunkSerializer` 内置调色板编解码

### Batch 7 ✅ 世界管理
- `level.dat` — 世界名称、种子、总 tick 数、出生点
- 目录结构：`saves/<world>/region/r.X.Z.mca`
- F8 键显示世界存档信息

### 额外完成：容器持久化
- 箱子内容 → `chests.dat`（ChestManager 序列化）
- 熔炉状态 → `furnaces.dat`（FurnaceManager 序列化，含燃烧进度）
- 掉落物实体 → `entities.dat`（EntityManager 序列化）

---

## 二、多线程区块系统

### 架构
```
主线程                          工作线程池 (N-1 核)
  │                                │
  ├─ updateChunks()                │
  │   └─ submitGenTask() ──────────┼─→ 地形生成 / 磁盘加载
  │                                │     └─→ genResultQueue_
  ├─ pollChunkGenResults() ←───────┤
  │                                │
  ├─ submitPendingMeshTasks() ─────┼─→ mesh 构建 (ChunkNeighbors)
  │                                │     └─→ meshResultQueue_
  ├─ pollMeshResults() ←───────────┤
  │   └─ uploadMesh() (GPU)        │
  │                                │
  └─ buildMeshes()                 │
      └─ 同步构建 (方块修改即时更新)
```

### 新增文件
| 文件 | 行数 | 职责 |
|------|------|------|
| `src/core/thread_pool.h` | 171 | 通用线程池 + ConcurrentQueue |
| `src/world/chunk_task_manager.h` | 117 | 区块任务管理器接口 |
| `src/world/chunk_task_manager.cpp` | 186 | 异步调度实现 |

### 关键设计
- **ChunkState 状态机**：Empty → Pending → Generating → DataReady → MeshPending → MeshBuilding → Ready
- **ChunkNeighbors**：主线程预捕获 5 个区块指针，工作线程只读访问，不触碰 World 的 unordered_map
- **MeshBuilder 池**：每个工作线程获取独立实例，避免锁
- **线程安全保证**：FastNoiseLite::GetNoise() 是 const；SaveManager 通过 mutex 串行化；GPU 上传仅主线程

---

## 三、代码重构

### game.cpp 拆分（1400 行 → 4 个文件）
| 文件 | 行数 | 职责 |
|------|------|------|
| `game.cpp` | 1055 | 核心循环、初始化、保存、输入、区块管理、渲染 |
| `game_survival.cpp` | 122 | 摔落/虚空伤害、饥饿/饱食度、进食、水下呼吸 |
| `game_highlight.cpp` | 158 | 方块选择高亮 + 破坏裂纹覆盖层 |
| `game_debug.cpp` | 113 | F2-F8 调试快捷键 |

### 死代码清理
- 删除 `loadWorld()` — 功能已被 `init()` 中的直接调用替代
- `mesh_builder.cpp` 消除重复：`build(World, Chunk)` 委托给 `build(ChunkNeighbors)`，减少 ~80 行

---

## 四、项目统计

| 指标 | 数值 |
|------|------|
| 源文件数 (.h + .cpp) | 81 |
| 总代码行数 | ~13,100 |
| 方块种类 | 35 |
| 物品种类 | 60 |
| 合成配方 | 32 |
| 熔炼配方 | 6 |
| 纹理 PNG | 142 |
| 着色器 | 4 (basic.vert/frag + ui.vert/frag) |

### 目录结构
```
src/
├── core/          # 基础设施（block, item, serialization, thread_pool, debug, input, tick_clock）
├── engine/        # Vulkan 引擎（vulkan_engine）
├── game/          # 游戏主循环（game, game_survival, game_highlight, game_debug, block_interaction）
├── player/        # 玩家系统（player, physics, inventory）
├── world/         # 世界系统（world, chunk, terrain_generator, light_engine, save_manager, region_file, chunk_serializer, chunk_task_manager, chest_manager, furnace_manager）
├── renderer/      # 渲染器（mesh_builder, ui_renderer, entity_renderer, texture_atlas, block_model）
├── entity/        # 实体系统（entity, entity_manager, item_entity）
├── crafting/      # 合成系统（recipe, smelting_recipe）
└── ui/            # GUI 界面（hud, container_screen, inventory_screen, crafting_screen, furnace_screen, chest_screen）
```

---

## 五、已完成阶段总览

| 阶段 | 状态 | 核心内容 |
|------|------|----------|
| Phase 1 | ✅ | Vulkan 渲染管线、区块系统、纹理图集、雾效、20TPS Tick |
| Phase 2 | ✅ | 物品/背包/工作台/熔炉、生命值/饥饿、挖掘/掉落物、死亡界面 |
| Phase 3 | ✅ | 矿石/工具、洞穴/植被、光照 BFS、4 群系、火把、水面透明 |
| Phase 4 | ✅ | 二进制序列化、区域文件、调色板压缩、自动保存、多线程区块、代码重构 |

---

## 六、遗留问题与后续优化方向

1. **昼夜循环**：Phase 3 延后，需要动态天空盒 + 光照时间因子
2. **天气系统**：雨/雪粒子 + 天空变暗
3. **透明排序**：当前水面按区块距离粗排序，未来可能需要更精细的排序
4. **GPU 上传优化**：当前每次 uploadMesh 调用 vkQueueWaitIdle，可改为 fence 异步
5. **区块加载优先级**：当前按遍历顺序加载，可改为按距离优先（螺旋遍历）
