#!/usr/bin/env bash
# ============================================================================
# Mycraft — 统一管理脚本
# 将依赖安装、编译、运行整合为一个脚本，通过子命令区分功能。
#
# 用法:
#   ./mycraft.sh setup              一键安装所有依赖（首次运行）
#   ./mycraft.sh build              编译 (Release)
#   ./mycraft.sh build --debug      编译 (Debug)
#   ./mycraft.sh build --clean      清理后重新编译
#   ./mycraft.sh build --jobs N     指定并行编译线程数
#   ./mycraft.sh run                运行游戏
#   ./mycraft.sh run --vlog         运行并启用全部调试日志
#   ./mycraft.sh run --vlog=entity  运行并启用指定类别日志
#   ./mycraft.sh start              编译 (Release) 并运行（默认行为）
#   ./mycraft.sh start --debug      编译 (Debug) 并运行
#   ./mycraft.sh server             编译并运行专用服务器
#   ./mycraft.sh server --build     仅编译服务器
#
# 无子命令时等同于 start（编译并运行）。
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; exit 1; }
fail_no_exit() { echo -e "${RED}[FAIL]${NC}  $*"; }

# ============================================================================
# 子命令: setup — 安装依赖
# ============================================================================
cmd_setup() {
    echo "============================================"
    echo "  Mycraft — Dependency Setup"
    echo "============================================"
    echo ""

    has() { command -v "$1" &>/dev/null; }

    case "$(uname -s)" in
        Darwin)
            # 检查 Xcode Command Line Tools（全新 Mac 必须先安装）
            if ! xcode-select -p &>/dev/null; then
                info "Installing Xcode Command Line Tools (required for compilation)..."
                xcode-select --install
                echo ""
                warn "Please complete the Xcode CLT installation dialog, then re-run this script."
                exit 0
            fi
            ok "Xcode Command Line Tools ready"

            if ! has brew; then
                info "Installing Homebrew..."
                /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
                # Homebrew 安装后需要设置 PATH
                if [ -f /opt/homebrew/bin/brew ]; then
                    eval "$(/opt/homebrew/bin/brew shellenv)"
                elif [ -f /usr/local/bin/brew ]; then
                    eval "$(/usr/local/bin/brew shellenv)"
                fi
            fi
            ok "Homebrew ready"
            info "Installing dependencies via Homebrew..."
            brew install cmake git
            if ! has glslc; then
                info "Installing Vulkan SDK..."
                brew install vulkan-sdk
            fi
            ok "Vulkan SDK ready"
            brew install vulkan-validationlayers 2>/dev/null || true
            ;;
        Linux)
            if [ -f /etc/os-release ]; then
                . /etc/os-release
                case "$ID" in
                    ubuntu|debian|linuxmint|pop)
                        info "Updating package list..."
                        sudo apt-get update -qq
                        sudo apt-get install -y --no-install-recommends \
                            build-essential cmake git pkg-config
                        if ! apt-cache show vulkan-sdk &>/dev/null; then
                            info "Adding LunarG Vulkan SDK repository..."
                            sudo apt-get install -y wget gnupg
                            wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc >/dev/null
                            CODENAME=$(lsb_release -cs 2>/dev/null || echo "jammy")
                            echo "deb https://packages.lunarg.com/vulkan/ ${CODENAME} main" | sudo tee /etc/apt/sources.list.d/lunarg-vulkan.list
                            sudo apt-get update -qq
                        fi
                        sudo apt-get install -y vulkan-sdk
                        sudo apt-get install -y --no-install-recommends \
                            libwayland-dev libxkbcommon-dev xorg-dev \
                            libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
                        ;;
                    fedora|rhel|centos|rocky|alma)
                        sudo dnf install -y gcc gcc-c++ cmake git
                        sudo dnf install -y vulkan-devel vulkan-tools vulkan-validation-layers-devel glslc glslang
                        sudo dnf install -y wayland-devel libxkbcommon-devel \
                            libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
                        ;;
                    arch|manjaro|endeavouros)
                        sudo pacman -S --needed --noconfirm \
                            base-devel cmake git vulkan-devel vulkan-tools vulkan-validation-layers \
                            shaderc glslang wayland libxkbcommon libx11 libxrandr libxinerama libxcursor libxi
                        ;;
                    *) fail "Unrecognized Linux distro: $ID" ;;
                esac
            else
                fail "Cannot detect Linux distribution"
            fi
            ;;
        MINGW*|MSYS*|CYGWIN*)
            warn "Windows detected. Please install manually:"
            echo "  1. Visual Studio 2019+ or MinGW-w64"
            echo "  2. CMake 3.20+  — https://cmake.org/download/"
            echo "  3. Vulkan SDK   — https://vulkan.lunarg.com/sdk/home"
            echo "  4. Git          — https://git-scm.com/download/win"
            exit 0
            ;;
        *) fail "Unsupported OS: $(uname -s)" ;;
    esac

    echo ""
    info "Verifying installation..."
    local ok_count=0 fail_count=0
    for cmd in cmake git glslc; do
        if has "$cmd"; then
            ok "$cmd: $(command -v $cmd)"
            ((ok_count++))
        else
            warn "$cmd: NOT FOUND"
            ((fail_count++))
        fi
    done
    echo ""
    if [ "$fail_count" -eq 0 ]; then
        ok "All prerequisites satisfied! Run './mycraft.sh' to compile and launch."
    else
        warn "$fail_count tool(s) missing."
    fi
}

# ============================================================================
# 辅助函数
# ============================================================================
detect_jobs() {
    local jobs="${1:-}"
    if [ -n "$jobs" ]; then echo "$jobs"; return; fi
    case "$(uname -s)" in
        Darwin)  sysctl -n hw.ncpu 2>/dev/null || echo 4 ;;
        Linux)   nproc 2>/dev/null || echo 4 ;;
        MINGW*|MSYS*|CYGWIN*) echo "${NUMBER_OF_PROCESSORS:-4}" ;;
        *)       echo 4 ;;
    esac
}

setup_vulkan_env() {
    case "$(uname -s)" in
        Darwin)
            if [ -f /opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json ]; then
                export VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json
            elif [ -f /usr/local/etc/vulkan/icd.d/MoltenVK_icd.json ]; then
                export VK_ICD_FILENAMES=/usr/local/etc/vulkan/icd.d/MoltenVK_icd.json
            fi
            if [ -d /opt/homebrew/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d ]; then
                export VK_LAYER_PATH=/opt/homebrew/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d
            elif [ -d /usr/local/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d ]; then
                export VK_LAYER_PATH=/usr/local/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d
            fi
            ;;
    esac
}

preflight() {
    # 确保 Homebrew 路径在 PATH 中（macOS 上 bash 脚本可能没有加载 shell profile）
    if [ -d /opt/homebrew/bin ] && [[ ":$PATH:" != *":/opt/homebrew/bin:"* ]]; then
        export PATH="/opt/homebrew/bin:$PATH"
    elif [ -d /usr/local/bin ] && [[ ":$PATH:" != *":/usr/local/bin:"* ]]; then
        export PATH="/usr/local/bin:$PATH"
    fi

    local missing=()
    command -v cmake &>/dev/null || missing+=("cmake")
    command -v git   &>/dev/null || missing+=("git")
    if [ ${#missing[@]} -gt 0 ]; then
        fail "Missing tools: ${missing[*]}. Run './mycraft.sh setup' first."
    fi
}

# ============================================================================
# 子命令: build — 编译
# ============================================================================
cmd_build() {
    local build_type="Release"
    local do_clean=false
    local jobs_override=""
    local target="Mycraft"

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --debug)  build_type="Debug"; shift ;;
            --clean)  do_clean=true; shift ;;
            --jobs)   jobs_override="$2"; shift 2 ;;
            --server) target="MycraftServer"; shift ;;
            *) shift ;;
        esac
    done

    preflight

    local jobs
    jobs=$(detect_jobs "$jobs_override")

    if $do_clean && [ -d "$BUILD_DIR" ]; then
        info "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # --- CMake 配置阶段 ---
    info "Configuring (${build_type}, target: ${target})..."
    local cmake_log="${BUILD_DIR}/cmake_configure.log"
    if cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE="$build_type" > "$cmake_log" 2>&1; then
        # 成功：只显示关键摘要
        grep -E "^-- (Configuring done|Generating done|Build files|Found Vulkan|Using glslc)" "$cmake_log" | while read -r line; do
            echo "  $line"
        done
        ok "Configuration complete"
    else
        # 失败：显示完整错误
        echo ""
        fail_no_exit "CMake configuration failed! Full log:"
        echo "────────────────────────────────────────"
        cat "$cmake_log"
        echo "────────────────────────────────────────"
        echo ""
        # 检测常见错误并给出建议
        if grep -q "FetchContent" "$cmake_log" && grep -qi "error\|fatal\|failed" "$cmake_log"; then
            warn "Looks like a dependency download failed. Possible fixes:"
            echo "  1. Check your internet connection"
            echo "  2. If behind a proxy: git config --global http.proxy http://host:port"
            echo "  3. If in China: git config --global url.\"https://ghproxy.com/https://github.com\".insteadOf \"https://github.com\""
            echo "  4. Retry: ./mycraft.sh build --clean"
        elif grep -q "glslc not found" "$cmake_log"; then
            warn "Vulkan SDK not installed. Run: ./mycraft.sh setup"
        elif grep -q "Could NOT find Vulkan" "$cmake_log"; then
            warn "Vulkan SDK not found. Run: ./mycraft.sh setup"
        fi
        exit 1
    fi

    # --- 编译阶段 ---
    echo ""
    info "Building ${target} with ${jobs} parallel jobs..."
    local start_time=$SECONDS
    local build_log="${BUILD_DIR}/cmake_build.log"
    if cmake --build . --target "$target" -j "$jobs" > "$build_log" 2>&1; then
        # 成功：显示进度摘要
        local total_targets
        total_targets=$(grep -c "^\[" "$build_log" 2>/dev/null || echo "0")
        ok "Build complete — ${total_targets} steps in $((SECONDS - start_time))s"
    else
        # 失败：显示最后的错误
        echo ""
        fail_no_exit "Build failed! Errors:"
        echo "────────────────────────────────────────"
        grep -A2 -E "error:|FAILED:" "$build_log" | tail -30
        echo "────────────────────────────────────────"
        echo ""
        info "Full build log: ${build_log}"
        exit 1
    fi
}

# ============================================================================
# 子命令: run — 运行
# ============================================================================
cmd_run() {
    local exe="${BUILD_DIR}/Mycraft"
    local extra_args=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --vlog)    extra_args+=("--debug"); shift ;;
            --vlog=*)  extra_args+=("--debug=${1#--vlog=}"); shift ;;
            --server)  exe="${BUILD_DIR}/MycraftServer"; shift ;;
            *) extra_args+=("$1"); shift ;;
        esac
    done

    if [ ! -f "$exe" ]; then
        fail "Executable not found at ${exe}. Build first with: ./mycraft.sh build"
    fi

    setup_vulkan_env

    echo ""
    ok "Launching $(basename "$exe")..."
    if [ ${#extra_args[@]} -gt 0 ]; then
        info "Runtime args: ${extra_args[*]}"
        echo "────────────────────────────────────────"
        cd "$BUILD_DIR"
        exec "$(basename "$exe")" "${extra_args[@]}"
    else
        echo "────────────────────────────────────────"
        cd "$BUILD_DIR"
        exec "./$( basename "$exe")"
    fi
}

# ============================================================================
# 子命令: start — 编译并运行（默认）
# ============================================================================
cmd_start() {
    local build_type="Release"
    local do_clean=false
    local jobs_override=""
    local run_args=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --debug)   build_type="Debug"; shift ;;
            --clean)   do_clean=true; shift ;;
            --jobs)    jobs_override="$2"; shift 2 ;;
            --vlog)    run_args+=("--debug"); shift ;;
            --vlog=*)  run_args+=("--debug=${1#--vlog=}"); shift ;;
            *) run_args+=("$1"); shift ;;
        esac
    done

    # 编译
    local build_args=()
    [ "$build_type" = "Debug" ] && build_args+=("--debug")
    $do_clean && build_args+=("--clean")
    [ -n "$jobs_override" ] && build_args+=("--jobs" "$jobs_override")
    cmd_build "${build_args[@]}"

    # 运行
    cmd_run "${run_args[@]}"
}

# ============================================================================
# 子命令: server — 编译并运行专用服务器
# ============================================================================
cmd_server() {
    local build_only=false
    local build_args=("--server")
    local run_args=("--server")

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build)  build_only=true; shift ;;
            --debug)  build_args+=("--debug"); shift ;;
            --clean)  build_args+=("--clean"); shift ;;
            --jobs)   build_args+=("--jobs" "$2"); shift 2 ;;
            *) run_args+=("$1"); shift ;;
        esac
    done

    cmd_build "${build_args[@]}"

    if ! $build_only; then
        cmd_run "${run_args[@]}"
    fi
}

# ============================================================================
# 主入口
# ============================================================================
main() {
    echo "============================================"
    echo "  Mycraft — Project Manager"
    echo "============================================"
    echo ""

    local subcmd="${1:-start}"

    case "$subcmd" in
        setup)   shift; cmd_setup "$@" ;;
        build)   shift; cmd_build "$@" ;;
        run)     shift; cmd_run "$@" ;;
        start)   shift 2>/dev/null || true; cmd_start "$@" ;;
        server)  shift; cmd_server "$@" ;;
        -h|--help|help)
            echo "Usage: ./mycraft.sh <command> [options]"
            echo ""
            echo "Commands:"
            echo "  setup              Install all dependencies (first time)"
            echo "  build [options]    Compile the project"
            echo "  run [options]      Run the game (must be built first)"
            echo "  start [options]    Build and run (default)"
            echo "  server [options]   Build and run dedicated server"
            echo "  help               Show this help"
            echo ""
            echo "Build options:"
            echo "  --debug            Debug build (symbols, no optimization)"
            echo "  --clean            Clean build directory first"
            echo "  --jobs N           Parallel compilation threads"
            echo ""
            echo "Run options:"
            echo "  --vlog             Enable all debug logging"
            echo "  --vlog=CATS        Enable specific log categories"
            echo ""
            echo "Server options:"
            echo "  --build            Build server only, don't run"
            exit 0
            ;;
        *)
            # 无子命令时当作 start 的参数
            cmd_start "$@"
            ;;
    esac
}

main "$@"
