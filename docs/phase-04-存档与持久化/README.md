# 第四阶段：存档与持久化

> 目标：退出游戏后进度不丢失。实现自动保存、增量加载、崩溃恢复。
> 原则：从简单到复杂，先能存能读，再优化压缩和异步。

---

## 开发顺序

| Batch | 内容 | 依赖 | 复杂度 |
|-------|------|------|--------|
| 1 | 二进制序列化框架 | 无 | 低 |
| 2 | 玩家数据保存/加载 | Batch 1 | 低 |
| 3 | 区块序列化（无压缩） | Batch 1 | 中 |
| 4 | 区域文件系统 + 脏区块追踪 | Batch 3 | 中 |
| 5 | 自动保存 + 退出保存 | Batch 2-4 | 低 |
| 6 | 调色板压缩 | Batch 3 | 中 |
| 7 | 世界管理（创建/列表/删除） | Batch 2-5 | 中 |

---

## Batch 1：二进制序列化框架

**目标**：通用的二进制读写工具，后续所有持久化代码都基于它。

**新文件**：`src/core/serialization.h/.cpp`

**接口设计**：
```
class BinaryWriter {
    BinaryWriter(const std::string& filepath);
    void writeU8/U16/U32/I32/F32/String/Bytes(...)
    void close();
};

class BinaryReader {
    BinaryReader(const std::string& filepath);
    bool isValid() const;
    uint8_t readU8(); uint16_t readU16(); ...
    std::string readString();
    void readBytes(uint8_t* buf, size_t len);
};
```

**文件头格式**：
```
[4 bytes] Magic: "VCFT" (VoxelCraft File)
[2 bytes] Format version (uint16_t)
[2 bytes] File type (0=level, 1=region, 2=player)
```

**设计原则**：
- 小端字节序（x86/ARM 原生，无需转换）
- 不使用第三方库（纯 C++ fstream）
- 所有字段固定大小或长度前缀（不依赖换行符/分隔符）
- 版本号前置，方便向后兼容迁移

---

## Batch 2：玩家数据保存/加载

**目标**：退出/进入游戏时保存/恢复玩家状态。最小可用版本。

**保存的数据**：
```
Player State:
  - position (3 × float)
  - yaw, pitch (2 × float)
  - hp, maxHp (2 × int32)
  - hunger, maxHunger (2 × int32)
  - saturation (float)
  - air (int32)

Inventory (36 slots):
  for each slot:
    - itemId (uint16)
    - count (uint16)
    - durability (uint16)
```

**文件路径**：`saves/<world>/player.dat`

**触发时机**：
- 保存：Game 析构时 / 定时自动保存
- 加载：Game::init() 中，若文件存在则读取

**实现步骤**：
1. Player 新增 `serialize(BinaryWriter&)` / `deserialize(BinaryReader&)`
2. Inventory 同上
3. Game::init() 尝试加载，Game 析构时保存

---

## Batch 3：区块序列化（无压缩版）

**目标**：保存/加载修改过的区块方块数据。初始版本不压缩。

**区块数据格式**：
```
Chunk File (per chunk, 临时方案):
  [4 bytes] chunkX (int32)
  [4 bytes] chunkZ (int32)
  [1 byte]  flags (bit 0 = hasData, bit 1 = hasLightData)
  [BLOCK_COUNT × 2 bytes] blocks (uint16 × 65536 = 128 KB)
  // 光照不存——加载时重新计算
```

**存储策略**：
- 只保存被玩家修改过的区块（脏标记）
- 未修改区块从种子重新生成（零存储开销）
- 需要新增 `Chunk::isModified_` 标记（放置/破坏方块时设置）

**加载流程**：
1. 尝试从磁盘读取区块
2. 若不存在 → 用 TerrainGenerator 生成（当前行为）
3. 若存在 → 反序列化 blocks_ → initSkyLight + initBlockLight

---

## Batch 4：区域文件系统

**目标**：将 32×32 区块打包到单个 `.mca` 文件，减少文件数量。

**文件格式（简化版 MC 区域文件）**：
```
Region File (r.X.Z.mca):
  Header: 4096 bytes
    - 1024 × 4 bytes: 每区块的 (offset_sectors:3B, size_sectors:1B)
    - offset=0 表示该区块未保存
  Data: 按需分配 4KB 扇区
    - 每个区块: [4 bytes 长度] + [raw chunk data]
```

**区域坐标**：`regionX = chunkX >> 5`, `regionZ = chunkZ >> 5`

**脏区块追踪**：
- `Chunk::isModified_` — 玩家修改方块时设为 true
- `World::getDirtyChunks()` — 收集所有 isModified_ 的区块
- 保存后清除标记

---

## Batch 5：自动保存 + 退出保存

**目标**：玩家不会丢失进度。

**自动保存**：
- 每 6000 tick（5 分钟）触发一次
- 每帧保存最多 2 个脏区块（增量保存，避免卡顿）
- 玩家数据每次自动保存时一起写入

**退出保存**：
- `Game::~Game()` 中保存所有脏区块 + 玩家数据
- ESC 菜单未来可加"保存并退出"按钮

**崩溃恢复**：
- 写入区域文件时先写临时文件（`.mca.tmp`），完成后 rename 替换
- 如果启动时发现 `.tmp` 文件 → 删除（上次写入不完整）

---

## Batch 6：调色板压缩

**目标**：减少区块文件大小 80%+。

**原理**：
- 一个 16×256×16 的区块通常只用 5-15 种方块
- 存储一个方块 → ID 映射表（调色板），每方块只需 log₂(N) bit
- 5 种方块 → 3 bit/方块 → 65536 × 3/8 = 24 KB（对比原始 128 KB）

**格式**：
```
Compressed Chunk:
  [uint16] paletteSize
  [paletteSize × uint16] palette (BlockId 列表)
  [uint8] bitsPerBlock (ceil(log2(paletteSize)))
  [packed bit array] blockIndices (bitsPerBlock × 65536 bits, 向上对齐到 byte)
```

**实现**：
- `ChunkSerializer::compress(Chunk&) → std::vector<uint8_t>`
- `ChunkSerializer::decompress(data, Chunk&)`

---

## Batch 7：世界管理

**目标**：支持多个存档世界。

**存档目录结构**：
```
saves/
├── New World/
│   ├── level.dat       # 世界元数据（名称、种子、游戏模式）
│   ├── player.dat      # 玩家数据
│   └── region/
│       ├── r.0.0.mca
│       └── ...
└── Test World/
    └── ...
```

**level.dat 内容**：
```
- worldName (string)
- seed (int64)
- gameMode (uint8: 0=survival, 1=creative)
- totalTicks (uint64)
- spawnX, spawnY, spawnZ (3 × float)
- formatVersion (uint16)
```

**界面**（简化版）：
- 启动时如果 `saves/` 为空 → 自动创建 "Default World"
- F8 键显示当前世界信息
- 后续可加 GUI 世界选择器

---

## 性能约束

| 操作 | 目标 | 方案 |
|------|------|------|
| 区块保存 | <5ms/chunk | 直接二进制写入，无复杂序列化 |
| 区块加载 | <3ms/chunk | 二进制读取 + 光照重算（~2ms） |
| 自动保存 | 不卡帧 | 每帧最多 2 个脏区块，分摊 IO |
| 调色板压缩 | <1ms/chunk | 位操作，CPU cache 友好 |
| 退出保存 | <2 秒 | 只保存 ~200 个加载的区块中的脏区块 |

---

## 关键设计原则

1. **先能用再优化**：Batch 3 先不压缩，Batch 6 再加调色板压缩
2. **光照不持久化**：加载时重算（~2ms/chunk），避免格式和光照系统耦合
3. **只存修改过的区块**：未修改的从种子重生成，大幅减少存储
4. **版本号前置**：所有文件第一个字段是格式版本，方便未来迁移
5. **原子写入**：写临时文件 → rename，防止崩溃损坏
6. **增量保存**：不一次性写所有脏区块，每帧写一点，避免卡顿
