# VoxelCraft

一个用 C++ 和 Vulkan 从零实现的类 Minecraft 体素游戏引擎。

## 依赖

- **Vulkan SDK** (1.2+) — 需要提前安装
- CMake 3.20+
- C++17 编译器

其他依赖（GLFW、GLM、VMA、vk-bootstrap、stb、FastNoiseLite）通过 CMake FetchContent 自动下载。

### macOS
```bash
# 安装 Vulkan SDK
# 下载地址: https://vulkan.lunarg.com/sdk/home
# 或通过 homebrew:
brew install vulkan-sdk
```

## 构建

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 运行

```bash
./VoxelEngine
```

## 操作

| 按键 | 功能 |
|------|------|
| W/A/S/D | 移动 |
| 鼠标 | 视角 |
| 空格 | 跳跃 |
| 左键 | 破坏方块 |
| 右键 | 放置方块 |
| 1-7 | 选择方块类型 |
| ESC | 释放/锁定鼠标 |

## 方块类型

1. 草方块
2. 泥土
3. 石头
4. 沙子
5. 木头
6. 树叶
7. 水
