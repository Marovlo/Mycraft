# Mycraft

> **A Minecraft clone built from scratch with C++17 and Vulkan.**
>
> 用 C++17 + Vulkan 从零实现的 Minecraft 克隆，目标是尽可能还原原版 MC 的游玩体验、视觉效果和游戏规则。

<!-- GitHub Description: A Minecraft clone built from scratch with C++17 & Vulkan. Features terrain generation, survival mechanics, crafting, furnace smelting, chest storage, multi-threaded chunk loading, and a custom save system. -->

## ✨ 已实现功能

### 🌍 世界生成
- 4 种群系（平原、森林、沙漠、雪地）+ 平滑过渡
- 多层洞穴系统（噪声驱动）
- 树木、花草、蘑菇、仙人掌等植被
- 矿石分布（煤、铁、金、钻石、红石）
- 海平面水体

### ⛏️ 核心玩法
- 方块破坏 & 放置（35 种方块）
- 60 种物品（方块、工具、食物、材料）
- 工具耐久度 & 挖掘速度加成
- 生命值、饥饿值、饱食度系统
- 摔落伤害、虚空伤害、溺水
- 死亡界面 & 重生

### 🔨 合成与熔炼
- 2×2 背包合成 + 3×3 工作台合成（32 种配方）
- 熔炉系统（6 种熔炼配方，燃料消耗，朝向纹理）
- 箱子存储（27 格，Shift 快速转移，R 键整理）

### 💾 存档与持久化
- 区域文件系统（.mca，32×32 区块打包，4KB 扇区对齐）
- 调色板压缩（128KB → ~24KB，压缩率 80%+）
- 自动保存（每 5 分钟）+ 增量脏区块保存
- 玩家数据、箱子、熔炉、掉落物实体全部持久化

### ⚡ 性能
- 多线程区块生成 & mesh 构建（线程池，N-1 核）
- 20 TPS 游戏 tick + 不限帧渲染
- 区块状态机（Empty → Ready，异步流水线）
- 缓存友好的数据结构

### 🎨 渲染
- Vulkan 1.2 渲染管线（macOS 通过 MoltenVK）
- 16×16 纹理图集（自动打包 142 张纹理）
- 水面半透明 + 水下雾效
- 方块选择高亮 + 破坏裂纹动画（10 阶段）
- 掉落物实体（旋转 + 悬浮动画）
- HUD（生命值、饥饿值、氧气条、快捷栏、十字准星）

## 🚀 快速开始

```bash
# 1. 一键安装所有依赖（首次运行）
./setup.sh

# 2. 一键编译并运行
./build.sh
```

### 脚本选项

```bash
./build.sh              # 编译 (Release) 并运行
./build.sh --debug      # 编译 (Debug) 并运行
./build.sh --build      # 仅编译，不运行
./build.sh --run        # 仅运行（跳过编译）
./build.sh --clean      # 清理后重新编译
./build.sh --jobs 8     # 指定并行编译线程数
```

### 手动构建

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./Mycraft
```

### 依赖

- **Vulkan SDK** 1.2+ — [下载](https://vulkan.lunarg.com/sdk/home)
- CMake 3.20+
- C++17 编译器 (GCC 8+, Clang 7+, MSVC 2019+)

其他依赖（GLFW、GLM、VMA、vk-bootstrap、stb、FastNoiseLite）通过 CMake FetchContent 自动下载。

## 🎮 操作

| 按键 | 功能 |
|------|------|
| **W/A/S/D** | 移动 |
| **鼠标** | 视角 |
| **空格** | 跳跃 |
| **Left Ctrl** | 冲刺 |
| **左键** | 破坏方块 / 攻击 |
| **右键** | 放置方块 / 使用物品 / 进食 |
| **Q** | 丢弃手持物品 |
| **E** | 打开/关闭背包 |
| **1-9** | 选择快捷栏槽位 |
| **滚轮** | 切换快捷栏槽位 |
| **R** | 整理背包/箱子 |
| **Shift+点击** | 快速转移物品 |
| **ESC** | 释放鼠标 / 关闭界面 |
| **F2** | 截图 |
| **F3** | FPS 显示 |
| **F4** | 调试信息 |
| **F5** | 传送到地表 |
| **F7** | 传送到出生点 |

## 🏗️ 技术栈

- **C++17** + **Vulkan 1.2** (MoltenVK on macOS)
- **GLFW** — 窗口 & 输入
- **GLM** — 数学库
- **VMA** — Vulkan 内存分配
- **vk-bootstrap** — Vulkan 初始化
- **FastNoiseLite** — 地形噪声生成
- **stb_image** — 纹理加载

## 📊 项目规模

| 指标 | 数值 |
|------|------|
| 源文件 (.h + .cpp) | 81 |
| 代码行数 | ~13,100 |
| 方块种类 | 35 |
| 物品种类 | 60 |
| 合成配方 | 32 |
| 纹理资源 | 142 |

## 📋 开发路线

- [x] **Phase 1** — 引擎基础（Vulkan 管线、区块系统、纹理图集、20TPS Tick）
- [x] **Phase 2** — 核心生存玩法（物品/背包/工作台/熔炉、生命值/饥饿、挖掘/掉落物）
- [x] **Phase 3** — 世界生成与环境（矿石/工具、洞穴/植被、光照、4 群系、水面透明）
- [x] **Phase 4** — 存档与持久化（区域文件、调色板压缩、自动保存、多线程区块）
- [ ] **Phase 5** — 生物与 AI（被动/敌对生物、寻路、战斗、昼夜循环）

## 📄 License

MIT
