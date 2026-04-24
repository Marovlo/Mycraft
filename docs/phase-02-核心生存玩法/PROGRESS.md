# 第二阶段进度总结

> 截至 2026-04-24 23:52，供下一个对话窗口继承。

---

## 已完成的 Batch

### Batch 0：2D UI 渲染管线 ✅
- 创建了独立的 Vulkan UI 管线：无深度测试、alpha blend、push constant 传屏幕尺寸
- `UIVertex` 格式：`{vec2 pos, vec2 uv, vec4 color}` — 像素坐标
- `UIRenderer` 类：drawRect / drawTexturedRect / drawCrosshair + flush
- 使用持久化动态 buffer（VMA_MEMORY_USAGE_CPU_TO_GPU），每帧只做 memcpy，零 GPU 阻塞

### Batch 1：物品系统 ✅
- `ItemId`(uint16_t)、`ItemStack`(id+count+durability)、`ItemProperties`、`ItemRegistry` 单例
- 22 种物品注册：10 方块物品 + 1 木棍 + 5 木工具 + 5 石工具 + None
- ItemId 和 BlockId 不是 1:1 的，映射关系在 ItemProperties::blockId 中定义

### Batch 2：背包 + 快捷栏 — 部分完成 ⚠️
- `Inventory` 类：36 格（9 快捷栏 + 27 主背包），addItem / consumeHeldItem
- 快捷栏 HUD：9 格深色背景 + 白色细边框选中高亮
- 数字键 1-9 和滚轮切换快捷栏
- 右键放置从快捷栏取对应方块的 blockId
- **待修复**：快捷栏内的 3D 方块图标未正常显示（见下方"当前阻塞问题"）

---

## 当前阻塞问题：快捷栏 3D 方块图标

### 问题描述
MC 原版在快捷栏中用 3D 等轴投影实时渲染方块模型。我们尝试了三种方案，都有问题：

### 方案历史

**方案 1（已废弃）：CPU 像素采样预渲染**
- `ItemIconAtlas` 类在启动时用 CPU 逐像素渲染等轴方块到图集
- 问题：UV 映射 bug 导致图标和实际方块不一致
- 文件已删除

**方案 2（已废弃）：3D 管线 + UBO 切换**
- `BlockModelRenderer` 用小视口 + 正交投影 UBO 渲染方块 mesh
- 问题：Vulkan 持久化映射 UBO 是共享的，修改 UBO 后 GPU 执行所有 draw call 时都看到最后写入的值，导致**世界变白屏**
- 根因：单 UBO 不支持同一 command buffer 中不同 draw call 使用不同矩阵

**方案 3（当前代码中，但已禁用）：UI 管线 + CPU 变换**
- 在 CPU 端用等轴投影矩阵变换方块顶点到屏幕坐标，生成 UIVertex
- 通过 UI 管线绘制（不影响 3D UBO）
- 问题：仍然白屏（可能是 uploadUIMesh 每帧调用 vkQueueWaitIdle，或 descriptor set 绑定未恢复）
- **当前状态：render3DIcons() 已禁用（空函数体），世界正常渲染**

### 正确解决方向
需要以下之一：
1. **第二个 UBO**：为图标渲染创建独立的 descriptor set + UBO，互不干扰
2. **动态 UBO 偏移**：单个大 UBO buffer 中存多个 UBO 数据，用动态偏移绑定
3. **修复方案 3**：排查 UI 管线方案的白屏原因（很可能是 uploadUIMesh 的 vkQueueWaitIdle 或 descriptor set 切换问题）

**推荐方案**：方案 3 最有潜力（不需要改引擎），需要：
- 把每帧 uploadUIMesh 改为复用动态 buffer（和 UIRenderer 一样）
- 确保切换 UI 管线后 descriptor set 正确绑定
- 用截图功能验证

---

## 已修复的 Bug 汇总

| Bug | 原因 | 修复 |
|-----|------|------|
| VMA cleanup 崩溃 | 空指针未检查 | destroyMesh/destroyTexture 加空检查 |
| 鼠标停不下来 | delta 未在消费后清零 | postUpdate() 机制 |
| 纹理拉伸成线 | UV 未归一化到 [0,1] | 除以 tileCount |
| PosZ/NegZ 面透明 | 绕序错误 | 叉积验证修正 |
| 区块边界透视 | 邻居未加载时错误剔除面 | 等邻居就绪 + 标记脏 |
| ESC 不响应 | input.update() 在 handleInput 前执行 | 调换顺序 |
| 移动卡顿 | 位置只在 20TPS tick 更新 | glm::mix 渲染插值 |
| 摄像机穿墙 | 碰撞缓冲 0.001 太小 | 增大到 0.04 + near plane 0.05 |
| 点击/数字键不灵 | 单次事件在 tick 中处理被漏掉 | 移到帧级处理 |
| 性能下降 | uploadUIMesh 每帧 vkQueueWaitIdle | 改为持久化动态 buffer |

---

## 当前代码架构

```
src/
├── main.cpp                    # 入口 + STB 实现
├── core/
│   ├── common.h                # 常量、坐标工具、Direction
│   ├── block.h/cpp             # BlockId、BlockProperties、BlockRegistry
│   ├── item.h/cpp              # ItemId、ItemStack、ItemRegistry
│   ├── input.h/cpp             # GLFW 输入（键盘/鼠标/滚轮）
│   └── tick_clock.h            # 20 TPS tick 时钟
├── engine/
│   └── vulkan_engine.h/cpp     # Vulkan 管线（3D + UI）、截图、UBO
├── game/
│   └── game.h/cpp              # 主循环、输入处理、区块管理
├── player/
│   ├── player.h/cpp            # 玩家状态、视角、射线检测
│   ├── physics.h/cpp           # AABB 碰撞（逐轴解算）
│   └── inventory.h/cpp         # 36 格背包
├── renderer/
│   ├── mesh_builder.h/cpp      # 区块网格构建（面剔除）
│   ├── texture_atlas.h/cpp     # PNG 纹理图集
│   ├── ui_renderer.h/cpp       # 2D UI 渲染（动态 buffer）
│   └── block_model.h/cpp       # 方块图标渲染（当前禁用）
├── world/
│   ├── chunk.h/cpp             # 16×256×16 区块
│   ├── world.h/cpp             # 区块哈希表管理
│   └── terrain_generator.h/cpp # 多层噪声地形 + 树木
├── ui/
│   └── hud.h/cpp               # 快捷栏 + 准星
└── shaders/
    ├── basic.vert/frag         # 3D 方块（含雾效）
    └── ui.vert/frag            # 2D UI overlay
```

**关键设计决策**：
- 输入分两层：帧级（ESC、鼠标视角、点击、数字键）+ tick 级（WASD 移动、跳跃）
- 渲染插值：prevPlayerPos_ + partialTick 做平滑
- 纹理名称解析：BlockFaceTextureNames(string) → resolveTextures → BlockFaceTextures(uint16_t)

---

## 调试工具

| 工具 | 触发 | 输出位置 |
|------|------|---------|
| 截图 | F2 或代码调用 `engine_.requestScreenshot()` | `debug_output/screenshot_<tick>.png` |
| 状态日志 | F4 | stdout |
| 自动截图 | 代码中在特定 tick 触发（测试后删除） | `debug_output/auto_debug.png` |

**AI 调试流程**：修改渲染代码后 → 在代码中加自动截图（如 tick=100 时）→ 运行 → read_file 读取截图 → 验证效果 → 移除自动截图代码

---

## 待继续的 Batch

| Batch | 内容 | 状态 |
|-------|------|------|
| Batch 2 | 快捷栏 3D 图标 | ⚠️ 需修复 |
| Batch 3 | 方块交互增强（挖掘进度+掉落物） | 未开始 |
| Batch 4 | 工具与武器系统 | 未开始 |
| Batch 5 | 制作系统 | 未开始 |
| Batch 6 | 生命值 + 自然伤害 | 未开始 |
| Batch 7 | 饱食度 + 食物 | 未开始 |

---

## 关键提醒

1. **目标是复刻 MC**，每个功能尽量按 MC 原版逻辑实现，不写临时方案
2. **性能优先**，但正确性更优先
3. **存档系统先行设计**（第四阶段），每个新系统同步定义序列化格式
4. **调试输出统一放 debug_output/**，不要污染项目目录
5. **每次运行后 pkill -f VoxelEngine** 确保进程关闭
6. **帧级 vs tick 级输入**：单次事件（点击、按键切换）在帧级处理；持续状态（移动）在 tick 级处理；攻击/放置后续改为 tick 级 + clickCount 排队
7. **单 UBO 限制**：当前引擎只有一个 UBO，不能在同一 command buffer 中用不同 MVP 矩阵。这是快捷栏图标的阻塞原因
