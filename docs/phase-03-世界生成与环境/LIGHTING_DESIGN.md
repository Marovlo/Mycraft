# 光照系统技术设计

> 第三阶段最核心的架构改动，影响 6 层代码。必须一次做对，不能返工。

## 1. 总体架构

```
Chunk (数据层)
  └── lightData_[BLOCK_COUNT]  ←── 每方块 1 byte: high nibble=skyLight, low nibble=blockLight
         ↑
LightEngine (传播层)
  ├── initSkyLight(chunk)      ←── 区块生成后初始化天空光
  ├── propagateBlockLight(...)  ←── 放置/移除光源时 BFS
  ├── removeLightBFS(...)       ←── 移除光源的暗化传播
  └── getLight(world, x, y, z) ←── mesh 构建时查询
         ↓
MeshBuilder (mesh 层)
  └── addFace(pos, dir, texId, lightLevel)  ←── 新增参数
         ↓
Vertex (GPU 层)
  └── { position, normal, texCoord, lightLevel(float) }  ←── 新增属性
         ↓
basic.vert → basic.frag (着色器层)
  └── finalColor *= lightMultiplier(lightLevel)
```

## 2. 数据结构

### Chunk 扩展
```cpp
// chunk.h 新增:
std::array<uint8_t, BLOCK_COUNT> lightData_{};  // packed: sky<<4 | block

uint8_t getSkyLight(int x, int y, int z) const;
uint8_t getBlockLight(int x, int y, int z) const;
uint8_t getMaxLight(int x, int y, int z) const;  // max(sky, block)
void setSkyLight(int x, int y, int z, uint8_t val);
void setBlockLight(int x, int y, int z, uint8_t val);

// Heightmap: 最高非透明方块的 Y 值（天空光快速计算用）
std::array<uint8_t, CHUNK_SIZE * CHUNK_SIZE> heightMap_{};
void updateHeightMap();
```

**内存开销**: 65,536 bytes/chunk（光照）+ 256 bytes/chunk（高度图）= ~64 KB/chunk。当前 blocks_ = 128 KB/chunk，增加 50%。在 RENDER_DISTANCE=8（~200 chunks）下总计 ~12 MB，完全可接受。

### LightEngine（新文件）
```
src/world/light_engine.h/.cpp
```
独立类，不嵌入 Chunk 或 World，通过引用操作。方便后续扩展（如多线程光照更新）。

## 3. 天空光传播

### 初始化（区块生成时）
1. `updateHeightMap()`: 扫描每列找最高非透明方块
2. 从 y=255 开始向下，直到 heightMap 对应高度：skyLight=15（垂直不衰减）
3. heightMap 以下：skyLight=0
4. BFS 从 heightMap 边缘向洞穴内水平传播（每步 -1）

### 跨 chunk 传播
- 当相邻 chunk 也已生成时，在 chunk 边界处做 BFS 接续
- 触发条件：新 chunk 生成完毕后，检查 4 个邻居 chunk 的边界光照

## 4. 方块光传播

### 放置光源
1. 在光源位置设置 blockLight = emitLevel
2. BFS 扩展：每步 blockLight -= 1，穿过透明方块
3. 更新所有被修改的 chunk 的 meshDirty

### 移除光源
1. 从光源位置开始"暗化 BFS"：记录需要重新传播的位置
2. 清零原光照值
3. 从记录的边界位置重新 BFS（重新点亮邻近光源的影响）

### 方块放置/破坏
- 放置不透明方块：可能遮挡天空光 → 更新 heightMap + 暗化 BFS + 重新传播
- 破坏方块：可能暴露天空光 → 更新 heightMap + 重新传播

## 5. Vertex 格式改动

```cpp
struct Vertex {
    glm::vec3 position;   // 12 bytes
    glm::vec3 normal;     // 12 bytes
    glm::vec2 texCoord;   // 8 bytes
    float     light;      // 4 bytes — 0.0 (dark) to 1.0 (full bright)
};
// sizeof = 36 bytes（从 32 增加 4 bytes = 12.5% 增长）
```

**为什么用 float 而非 uint8_t**：
- Vulkan 管线需要 4 字节对齐
- 一个 float 可以直接在着色器中使用，无需归一化
- 如果用 uint8_t 需要 `VK_FORMAT_R8_UNORM`，自动归一化到 [0,1]，也行，但 float 更简单

**光照值计算**（mesh 构建时）：
```
lightLevel = max(skyLight, blockLight)
vertexLight = lightCurve[lightLevel]  // 查表: 0→0.05, 1→0.07, ..., 15→1.0
```

MC 使用的光照曲线不是线性的，而是指数：`brightness = 0.8^(15-level)` 近似。

## 6. 着色器改动

### basic.vert
```glsl
layout(location = 3) in float inLight;
layout(location = 3) out float fragLight;
// ...
fragLight = inLight;
```

### basic.frag
```glsl
layout(location = 3) in float fragLight;
// ...
// 替换硬编码光照：
float faceShade = ...; // 保留 MC 风格面着色 (top=1.0, bottom=0.5, sides=0.7-0.8)
vec3 litColor = texColor.rgb * fragLight * faceShade;
```

## 7. 性能约束

| 操作 | 目标 | 方案 |
|------|------|------|
| 初始天空光计算 | <2ms/chunk | heightMap O(16²) + 垂直扫描 O(16²×256) |
| 方块光 BFS | <1ms/次 | 限制 BFS 范围 ≤15 步，最多 ~3000 格 |
| mesh 重建时查光照 | 零额外开销 | 光照值已在 chunk.lightData_ 中，O(1) 查表 |
| 跨 chunk 传播 | 不阻塞主线程 | 当前单线程 OK，后续可异步 |

## 8. 实现顺序（Sub-batch）

1. **6a**: Chunk 光照存储 + heightMap + 天空光初始化（数据层，无视觉变化）
2. **6b**: LightEngine BFS（方块光+天空光传播，数据层完成）
3. **6c**: Vertex 扩展 + MeshBuilder 读光照 + 着色器集成（视觉变化！）
4. **6d**: 方块放置/破坏触发光照更新 + 火把方块注册
5. **6e**: 跨 chunk 光照传播（边界处理）
