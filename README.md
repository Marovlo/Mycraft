# VoxelCraft

一个用 C++17 和 Vulkan 从零实现的类 Minecraft 体素游戏引擎。

## 快速开始

```bash
# 1. 一键安装所有依赖（首次运行）
./setup.sh

# 2. 一键编译并运行
./build.sh
```

就这么简单！

## 脚本说明

### `setup.sh` — 一键安装依赖

自动检测操作系统并安装所有前置依赖：

| 平台 | 包管理器 | 安装内容 |
|------|---------|---------|
| **macOS** | Homebrew | cmake, git, vulkan-sdk, validation-layers |
| **Ubuntu/Debian** | apt | build-essential, cmake, vulkan-sdk, X11/Wayland 开发库 |
| **Fedora/RHEL** | dnf | gcc, cmake, vulkan-devel, glslc, X11/Wayland 开发库 |
| **Arch/Manjaro** | pacman | base-devel, cmake, vulkan-devel, shaderc |
| **Windows** | 手动 | 打印安装指引（Visual Studio + Vulkan SDK） |

### `build.sh` — 一键编译运行

```bash
./build.sh              # 编译 (Release) 并运行
./build.sh --debug      # 编译 (Debug) 并运行
./build.sh --build      # 仅编译，不运行
./build.sh --run        # 仅运行（跳过编译）
./build.sh --clean      # 清理 build 目录后重新编译
./build.sh --jobs 8     # 指定并行编译线程数
```

## 手动构建

如果不想用脚本，也可以手动操作：

### 依赖

- **Vulkan SDK** (1.2+) — [下载地址](https://vulkan.lunarg.com/sdk/home)
- CMake 3.20+
- C++17 编译器 (GCC 8+, Clang 7+, MSVC 2019+)

其他依赖（GLFW、GLM、VMA、vk-bootstrap、stb、FastNoiseLite）通过 CMake FetchContent 自动下载。

### 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### 运行

```bash
# Linux
./build/VoxelEngine

# macOS（需要设置 MoltenVK 环境变量）
./build.sh --run
```

## 操作

| 按键 | 功能 |
|------|------|
| W/A/S/D | 移动 |
| 鼠标 | 视角 |
| 空格 | 跳跃 |
| Left Ctrl | 冲刺 |
| 左键 | 破坏方块 |
| 右键 | 放置方块 |
| 1-9 | 选择方块类型 |
| ESC | 释放/锁定鼠标 |

## 方块类型

1. 草方块
2. 泥土
3. 石头
4. 沙子
5. 木头
6. 树叶
7. 水

## 技术栈

- **C++17** + **Vulkan 1.2**
- macOS 通过 MoltenVK 运行
- GLFW (窗口/输入) · GLM (数学) · VMA (GPU 内存) · vk-bootstrap (Vulkan 初始化)
- FastNoiseLite (地形噪声) · stb_image (纹理加载)
