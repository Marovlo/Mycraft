# 第一阶段：引擎基础 — 详细开发文档

> **目标**：补全渲染引擎和世界管理的基础能力，为后续所有阶段提供坚实底层。  
> **预期产出**：纹理加载的真实方块外观、光照雏形（面光照→为第三阶段完整光照铺路）、天空盒和雾效、粒子系统框架、Tick系统、子区块优化、多线程区块生成。

---

## 目录

1. [已完成功能概述](#1-已完成功能概述)
2. [1.1 纹理图集系统](#11-纹理图集系统)
3. [1.2 透明/半透明渲染](#12-透明半透明渲染)
4. [1.3 天空盒与雾效](#13-天空盒与雾效)
5. [1.4 粒子系统框架](#14-粒子系统框架)
6. [1.5 文字渲染](#15-文字渲染)
7. [1.6 子区块优化](#16-子区块优化)
8. [1.7 区块加载优先级](#17-区块加载优先级)
9. [1.8 多线程区块生成](#18-多线程区块生成)
10. [1.9 Tick 系统](#19-tick-系统)
11. [1.10 输入系统增强](#110-输入系统增强)
12. [实施顺序与依赖关系](#实施顺序与依赖关系)

---

## 1. 已完成功能概述

| 子系统 | 状态 | 关键文件 |
|--------|------|---------|
| Vulkan 渲染管线 | ✅ | `engine/vulkan_engine.cpp` (999行) |
| 区块网格构建与面剔除 | ✅ | `renderer/mesh_builder.cpp` |
| 深度测试、背面剔除 | ✅ | 管线 raster.cullMode = BACK |
| 动态视口与窗口缩放 | ✅ | drawFrame() 中动态设置 |
| 程序化纹理图集 | ✅（占位） | `game.cpp::generateBlockTexture()` |
| 基础键鼠输入 | ✅ | `core/input.cpp` |
| WASD移动/跳跃/冲刺 | ✅ | `game.cpp::handleInput()` |

**当前 Vertex 格式**：`{vec3 position, vec3 normal, vec2 texCoord}` = 32 bytes/vertex  
**当前着色器**：basic.vert（MVP变换）+ basic.frag（纹理采样 + 固定方向光 + 面亮度调制）  
**当前纹理**：程序化颜色图集（16×16 像素/tile），用 texId/tileCount 归一化 UV

---

## 1.1 纹理图集系统

### 目标
替换程序化颜色纹理为从 PNG 文件加载的真实 Minecraft 风格方块纹理。

### 游戏逻辑
- Minecraft 使用 16×16 像素的方块纹理 tile
- 所有方块纹理合并到一个纹理图集（texture atlas）中
- 每个方块面通过 UV 坐标索引到图集中的对应 tile
- 后续纹理动画（水、岩浆）在此基础上实现

### 数据结构设计

```cpp
// src/renderer/texture_atlas.h

struct TextureAtlas {
    AllocatedImage image;           // GPU 纹理
    uint32_t tileSize = 16;         // 每个 tile 的像素尺寸
    uint32_t tilesPerRow = 0;       // 图集每行 tile 数
    uint32_t tilesPerCol = 0;       // 图集每列 tile 数
    uint32_t totalTiles = 0;

    // 通过 tile 索引获取归一化 UV 范围 [uMin, uMax, vMin, vMax]
    glm::vec4 getTileUV(uint16_t tileIndex) const;
};

class TextureAtlasBuilder {
public:
    // 从目录加载所有 tile PNG 文件，打包为图集
    // 返回 tile 名称 → tile 索引的映射
    TextureAtlas build(VulkanEngine& engine, const std::string& textureDir);

    // 获取 tile 名称到索引的映射（如 "grass_top" → 0）
    const std::unordered_map<std::string, uint16_t>& getNameMap() const;

private:
    std::unordered_map<std::string, uint16_t> nameToIndex_;
};
```

### 资源目录结构
```
assets/
└── textures/
    └── blocks/
        ├── grass_top.png       (16×16)
        ├── grass_side.png
        ├── dirt.png
        ├── stone.png
        ├── sand.png
        ├── oak_log_side.png
        ├── oak_log_top.png
        ├── oak_leaves.png
        ├── water_still.png
        ├── cobblestone.png
        ├── oak_planks.png
        ├── bedrock.png
        ├── gravel.png
        └── ...
```

### 实现细节
1. **加载阶段**：`stb_image` 加载每个 PNG 为 RGBA 像素
2. **打包阶段**：按 tile 索引排列到一张大图集（正方形，2 的幂次尺寸）
   - 例如 64 种纹理 → 8×8 网格 → 128×128 像素图集
   - 256 种纹理 → 16×16 网格 → 256×256 像素图集
3. **上传阶段**：调用 `engine.uploadTexture()` 上传到 GPU
4. **BlockRegistry 更新**：`BlockFaceTextures` 中的 texId 改为引用图集 tile 索引
5. **MeshBuilder 更新**：`addFace()` 中的 UV 计算使用 `atlas.getTileUV(texId)`

### Sampler 配置
```
magFilter = VK_FILTER_NEAREST  // 方块纹理必须最近邻采样（像素风）
minFilter = VK_FILTER_NEAREST
addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE  // 防止 tile 边缘渗色
```

### 性能考虑
- 单张图集纹理 = 单次纹理绑定，零 draw call 切换开销
- 后续 mipmap 需手动处理（防止不同 tile 之间在远处混色，可用各向异性过滤或 per-tile mipmap padding）

### 对现有代码的改动
| 文件 | 改动 |
|------|------|
| 新增 `renderer/texture_atlas.h/.cpp` | 图集构建器 |
| `core/block.cpp` | `registerDefaults()` 中 texId 改为引用图集名称映射 |
| `renderer/mesh_builder.cpp` | UV 计算使用 atlas 归一化坐标 |
| `game.cpp` | `generateBlockTexture()` 替换为 `TextureAtlasBuilder::build()` |
| `CMakeLists.txt` | ASSET_DIR 已定义，确认 `assets/textures/` 路径 |

---

## 1.2 透明/半透明渲染

### 目标
正确渲染玻璃、水面、树叶等非完全不透明方块。

### 游戏逻辑
- Minecraft 中树叶有两种模式：Fancy（半透明）和 Fast（不透明带镂空）
- 水面半透明，能看到水下方块
- 玻璃完全透明但有边框

### 渲染架构
需要**两个渲染通道**（或两个子通道）：

```
Pass 1: 不透明方块（depth write ON, depth test ON, no blending）
Pass 2: 透明方块  （depth write OFF, depth test ON, alpha blending ON）
```

### 数据结构
```cpp
// MeshBuilder 输出两组网格
struct ChunkMeshData {
    std::vector<Vertex> opaqueVertices;
    std::vector<uint32_t> opaqueIndices;
    std::vector<Vertex> transparentVertices;
    std::vector<uint32_t> transparentIndices;
};

// Chunk 持有两个 Mesh
struct ChunkRenderData {
    Mesh opaqueMesh;
    Mesh transparentMesh;
};
```

### 排序策略
- 透明方块需要**从后到前**排序（back-to-front）
- 粗粒度排序：按区块到摄像机的距离排序区块的透明 mesh 绘制顺序
- 细粒度（可选进阶）：每个区块内按面到摄像机距离排序

### 着色器改动
```glsl
// basic.frag 增加 alpha 支持
outColor = vec4(texColor.rgb * lighting, texColor.a);
```

### 管线改动
```cpp
// 透明管线需要开启 alpha blending
colorBlendAttachment.blendEnable = VK_TRUE;
colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
```

### 性能考虑
- 透明 mesh 远少于不透明 mesh（通常 <5%），排序开销小
- depth write OFF 防止透明面遮挡后面的透明面
- 后续可用 OIT（Order-Independent Transparency）替换简单排序

---

## 1.3 天空盒与雾效

### 目标
渲染蓝天、地平线渐变、日月（预留），以及远处区块的距离雾。

### 天空盒实现方案
**方案选择**：渐变天空穹顶（非 cubemap）— 与 Minecraft 一致。

```
实现方式：全屏三角形 + 片段着色器中根据视线方向计算颜色
- 天顶：深蓝 (0.25, 0.47, 0.82)
- 地平线：浅蓝 (0.53, 0.81, 0.92)
- 地平线以下：与雾色混合
```

### 着色器设计
```glsl
// sky.vert — 全屏三角形，不受 MVP 影响
// sky.frag — 根据视线方向 y 分量计算渐变色

// 新增 UBO 字段
struct UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 fogColor;       // 新增：雾颜色 (r, g, b, density)
    float fogStart;      // 新增：雾开始距离
    float fogEnd;        // 新增：雾结束距离
    float timeOfDay;     // 新增：0-1 表示一天（预留给昼夜）
    float padding;
};
```

### 雾效
```glsl
// basic.frag 中增加雾效
float dist = length(fragWorldPos - viewPos);
float fogFactor = clamp((fogEnd - dist) / (fogEnd - fogStart), 0.0, 1.0);
outColor = mix(fogColor, outColor, fogFactor);
```

**雾参数**：
- `fogStart` = `(RENDER_DISTANCE - 2) * CHUNK_SIZE` — 开始变雾
- `fogEnd` = `RENDER_DISTANCE * CHUNK_SIZE` — 完全变雾
- `fogColor` = 天空地平线颜色（随天气和昼夜变化）

### 渲染顺序
```
1. 天空（关闭深度写入，关闭深度测试）
2. 不透明方块（深度写入 ON）
3. 透明方块（深度写入 OFF）
```

### 对现有代码的改动
| 文件 | 改动 |
|------|------|
| 新增 `shaders/sky.vert` + `sky.frag` | 天空着色器 |
| `vulkan_engine.cpp` | 新增天空管线，修改 render pass |
| `vulkan_engine.h` | UBO 扩展 fogColor/fogStart/fogEnd/timeOfDay |
| `basic.frag` | 增加雾效计算 |
| `basic.vert` | 传递 viewPos 到 frag |
| `game.cpp` | 设置 UBO 中的雾参数 |

---

## 1.4 粒子系统框架

### 目标
建立可扩展的粒子系统框架，支持方块破坏碎片、火焰等效果。

### 设计思路
第一阶段只建框架 + 实现方块破坏粒子，后续阶段扩展效果类型。

### 数据结构
```cpp
// src/renderer/particle_system.h

struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec4 color;        // RGBA
    float life;             // 剩余生命（秒）
    float maxLife;
    float size;
    uint16_t texId;         // 纹理 tile（方块碎片用方块纹理）
};

class ParticleSystem {
public:
    void emit(const glm::vec3& pos, int count, const ParticleParams& params);
    void update(float dt);
    void buildMesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

private:
    std::vector<Particle> particles_;
    static constexpr size_t MAX_PARTICLES = 4096;
};
```

### 渲染方式
- 粒子渲染为面向摄像机的小四边形（billboard）
- 使用与方块相同的纹理图集
- 在透明通道之后绘制（alpha blending）

### 第一阶段只实现
- 方块破坏时在方块位置生成 10-20 个碎片粒子
- 粒子有重力、随机速度、0.5-1.0 秒生命

---

## 1.5 文字渲染

### 目标
能在屏幕上渲染调试文字（F3 信息），为后续 GUI 做基础。

### 实现方案
**位图字体**（Bitmap Font）— 简单高效，与 MC 风格一致：

1. 预制一张包含 ASCII 字符的位图字体纹理（类似 MC 的 ascii.png）
2. 每个字符是 8×8 像素
3. 渲染时为每个字符生成一个小四边形

### 数据结构
```cpp
// src/renderer/text_renderer.h

class TextRenderer {
public:
    void init(VulkanEngine& engine, const std::string& fontTexturePath);

    // 在屏幕坐标 (x, y) 处绘制文字
    // x, y 为像素坐标，左上角为原点
    void drawText(const std::string& text, float x, float y,
                  float scale = 1.0f, glm::vec3 color = {1,1,1});

    // 提交到 command buffer
    void render(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight);

private:
    AllocatedImage fontTexture_;
    VkPipeline textPipeline_;         // 独立管线（正交投影，alpha blend）
    VkPipelineLayout textPipelineLayout_;
    // 每帧收集的文字顶点
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    Mesh dynamicMesh_;
};
```

### 第一阶段只实现
- F3 显示：FPS、玩家坐标(x,y,z)、朝向(yaw/pitch)、加载区块数
- 使用正交投影矩阵覆盖在 3D 场景之上

---

## 1.6 子区块优化

### 目标
将 16×256×16 区块拆分为 16 个 16×16×16 子区块，优化网格构建和内存。

### 当前问题
- `MeshBuilder::build()` 遍历 16×256×16 = 65536 个方块，即使高空全是空气
- 大量空子区块浪费遍历时间

### 数据结构
```cpp
// 修改 chunk.h

class SubChunk {
    std::array<BlockId, 16*16*16> blocks_ = {};
    uint16_t nonAirCount_ = 0;  // 快速跳过全空子区块
public:
    BlockId getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockId id);
    bool isEmpty() const { return nonAirCount_ == 0; }
};

class Chunk {
    static constexpr int SUB_CHUNKS = CHUNK_HEIGHT / 16;  // = 16
    std::array<SubChunk, SUB_CHUNKS> subChunks_;
    // ...每个子区块独立 Mesh
    std::array<Mesh, SUB_CHUNKS> meshes_;
    std::array<bool, SUB_CHUNKS> meshDirty_;
};
```

### 性能收益
- 典型世界中 16 个子区块只有 4-6 个含方块，跳过 60%+ 遍历
- 修改方块只需重建该子区块的 mesh，而非整个区块
- 每个子区块 mesh 更小，上传更快

### 对 MeshBuilder 的改动
- `build()` 改为 `buildSubChunk(world, chunk, subChunkY)`
- 邻居查询需要考虑子区块边界（Y方向上/下子区块）

---

## 1.7 区块加载优先级

### 目标
优先加载玩家面朝方向的区块，减少视野内的空洞。

### 实现方案
```cpp
// 在 updateChunks() 中，收集待加载区块后按优先级排序
float priority(int cx, int cz, int pcx, int pcz, glm::vec3 forward) {
    float dx = cx - pcx, dz = cz - pcz;
    float dist = dx*dx + dz*dz;
    glm::vec2 dir = glm::normalize(glm::vec2(dx, dz));
    glm::vec2 fwd = glm::normalize(glm::vec2(forward.x, forward.z));
    float dot = glm::dot(dir, fwd);  // 1.0 = 正前方, -1.0 = 正后方
    return dist - dot * 4.0f;  // 距离越近、越在前方，优先级越高
}
```

### 实施
- 收集需要生成的区块坐标到 vector
- 按 priority 排序
- 每帧只生成前 N 个（如 4-8 个）

---

## 1.8 多线程区块生成

### 目标
将地形生成和网格构建移出主线程，消除卡顿。

### 架构设计
```
主线程                        工作线程池
  │                              │
  ├─ 收集需要生成的区块 ────────→ 任务队列
  │                              │
  │                         地形生成（纯数据）
  │                         网格构建（纯数据）
  │                              │
  ├─ 从完成队列取结果 ←────────── 完成队列
  │                              
  ├─ uploadMesh()（主线程，需要 Vulkan 访问）
```

### 数据结构
```cpp
// src/core/thread_pool.h

struct ChunkGenTask {
    int cx, cz;
};

struct ChunkGenResult {
    int cx, cz;
    std::vector<BlockId> blockData;     // 生成的方块数据
    std::vector<Vertex> vertices;        // 构建好的网格
    std::vector<uint32_t> indices;
};

class ChunkWorkerPool {
public:
    void init(int numThreads = 4);
    void shutdown();

    void submit(ChunkGenTask task);
    bool tryGetResult(ChunkGenResult& result);

private:
    std::vector<std::thread> workers_;
    // 线程安全队列
    ConcurrentQueue<ChunkGenTask> taskQueue_;
    ConcurrentQueue<ChunkGenResult> resultQueue_;
};
```

### 关键约束
- **地形生成**：纯 CPU 计算，无 Vulkan 调用，可安全并行
- **网格构建**：需要读取邻居区块数据，需确保邻居已生成（或用快照）
- **GPU 上传**：`uploadMesh()` 使用 Vulkan 命令，**必须在主线程执行**
- **区块数据写入**：生成完毕后主线程将数据写入 Chunk 对象

### 性能目标
- 消除新区块加载时的帧率抖动
- 充分利用多核 CPU（M 系列 Mac 有 8-12 核）

---

## 1.9 Tick 系统

### 目标
建立 20 TPS 固定频率的游戏逻辑更新，与渲染帧率解耦。

### 游戏逻辑
Minecraft 的所有游戏逻辑运行在 20 tick/second（每 tick 50ms）的固定频率上。渲染帧率可以更高（60/144 FPS），物理和动画使用插值。

### 架构设计
```cpp
// 修改 VulkanEngine::run() 或 Game::update()

class TickClock {
public:
    static constexpr double TICK_RATE = 20.0;
    static constexpr double TICK_DURATION = 1.0 / TICK_RATE;  // 0.05s

    // 返回本帧需要执行的 tick 数（通常是 0 或 1，卡顿时可能 >1）
    int advance(double currentTime);

    // 渲染插值因子 [0, 1)，用于平滑视觉表现
    float getPartialTick() const;

    // 当前游戏总 tick 数
    uint64_t getTotalTicks() const { return totalTicks_; }

private:
    double accumulator_ = 0.0;
    double lastTime_ = 0.0;
    uint64_t totalTicks_ = 0;
    float partialTick_ = 0.0f;
};
```

### 主循环改造
```
while (!shouldClose) {
    glfwPollEvents();

    double now = glfwGetTime();
    int ticks = tickClock.advance(now);

    for (int i = 0; i < ticks; i++) {
        gameTick();  // 固定 50ms 逻辑步
    }

    float partial = tickClock.getPartialTick();
    render(partial);  // 渲染使用插值
}
```

### gameTick() 内容（第一阶段）
```
gameTick():
  1. input.update()
  2. handleInput()    // 移动、交互
  3. input.postUpdate()
  4. physics.update()  // 玩家物理
  5. worldTime++       // 昼夜推进（24000 循环）
  6. updateChunks()    // 区块加载
  7. // 预留：方块随机刻、实体刻、流体刻
```

### 渲染插值
- 玩家位置在两个 tick 之间插值：`renderPos = prevPos + (currPos - prevPos) * partialTick`
- 摄像机跟随插值后的位置，实现平滑视觉

### 昼夜周期（预留）
- `worldTime` 每 tick +1，范围 0-23999
- 0=日出, 6000=正午, 12000=日落, 18000=午夜
- 第一阶段只递增计数器，天空/光照颜色变化在第三阶段实现

---

## 1.10 输入系统增强

### 目标
支持鼠标滚轮和可配置按键，为后续 GUI 交互做准备。

### 滚轮支持
```cpp
// input.h 新增
double getScrollDelta() const;

// GLFW 回调
static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset);
```

用途：切换快捷栏选中格（第二阶段背包系统使用）。第一阶段先接入，功能在第二阶段生效。

### 按键绑定（预留接口）
```cpp
// 第一阶段只定义接口，硬编码映射
enum class GameAction {
    MoveForward, MoveBack, MoveLeft, MoveRight,
    Jump, Sprint, Sneak,
    Attack, UseItem,
    Inventory, DropItem,
    ToggleDebug,  // F3
    Escape,
    Hotbar1, Hotbar2, /*...*/ Hotbar9,
};

class KeyBindings {
public:
    int getKey(GameAction action) const;
    void setKey(GameAction action, int glfwKey);
private:
    std::unordered_map<GameAction, int> bindings_;
};
```

### 连续破坏（预留）
- 长按左键持续挖掘需要挖掘进度系统（第二阶段 2.3 实现）
- 第一阶段只确保 `isMouseButtonDown()` 可正确报告持续按住状态（已实现）

---

## 实施顺序与依赖关系

```
                    ┌──────────────────┐
                    │ 1.1 纹理图集系统  │ ← 第一优先，所有视觉的基础
                    └────────┬─────────┘
                             │
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
    ┌─────────────┐ ┌──────────────┐ ┌──────────────┐
    │ 1.2 透明渲染 │ │ 1.3 天空+雾效 │ │ 1.6 子区块   │
    └─────────────┘ └──────────────┘ └──────┬───────┘
                                            │
              ┌─────────────────────────────┤
              ▼                             ▼
    ┌──────────────┐              ┌──────────────────┐
    │ 1.9 Tick系统  │              │ 1.8 多线程区块生成 │
    └──────┬───────┘              └──────────────────┘
           │
    ┌──────┴───────┐
    │ 1.7 加载优先级│
    └──────────────┘
    
    并行可做（无依赖）：
    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
    │ 1.4 粒子系统  │  │ 1.5 文字渲染  │  │ 1.10 输入增强 │
    └──────────────┘  └──────────────┘  └──────────────┘
```

### 建议实施批次

| 批次 | 任务 | 预估工作量 | 说明 |
|------|------|-----------|------|
| **Batch 1** | 1.1 纹理图集系统 | 中 | 最高优先，视觉效果质变 |
| **Batch 2** | 1.3 天空盒 + 雾效 | 中 | 让世界看起来完整 |
| **Batch 3** | 1.2 透明/半透明渲染 | 中 | 水和树叶正确显示 |
| **Batch 4** | 1.6 子区块 + 1.8 多线程 | 大 | 性能关键，需要同时做 |
| **Batch 5** | 1.9 Tick 系统 | 中 | 后续所有动态系统的基础 |
| **Batch 6** | 1.5 文字渲染 | 小 | F3 调试信息 |
| **Batch 7** | 1.4 粒子系统 + 1.10 输入增强 | 小 | 框架搭建，后续扩展 |

### 纹理动画说明
纹理动画（水面帧动画）依赖纹理图集系统（1.1），但实现较复杂，建议放在第一阶段末尾或第三阶段天气系统一起做。核心思路：
- 动画纹理在图集中占多个 tile（如水面 32 帧 → 32 个 tile）
- 每 N 个 game tick 切换当前帧 → 更新 UV 偏移（Push Constant 或 UBO）
- 或者直接在 CPU 端更新图集子区域（`vkCmdCopyBufferToImage` 局部更新）
