#!/usr/bin/env bash
# ============================================================================
# VoxelCraft — One-click dependency installer
# Supports: macOS (Homebrew), Ubuntu/Debian (apt), Fedora (dnf), Arch (pacman)
# ============================================================================
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; exit 1; }

# ---------- Detect OS ----------
detect_os() {
    case "$(uname -s)" in
        Darwin)  OS="macos" ;;
        Linux)
            if   [ -f /etc/os-release ]; then
                . /etc/os-release
                case "$ID" in
                    ubuntu|debian|linuxmint|pop) OS="debian" ;;
                    fedora|rhel|centos|rocky|alma) OS="fedora" ;;
                    arch|manjaro|endeavouros) OS="arch" ;;
                    *) OS="linux-unknown" ;;
                esac
            else
                OS="linux-unknown"
            fi
            ;;
        MINGW*|MSYS*|CYGWIN*)
            OS="windows"
            ;;
        *) fail "Unsupported OS: $(uname -s)" ;;
    esac
    info "Detected OS: $OS"
}

# ---------- Check if a command exists ----------
has() { command -v "$1" &>/dev/null; }

# ---------- macOS ----------
install_macos() {
    if ! has brew; then
        info "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    ok "Homebrew ready"

    info "Installing dependencies via Homebrew..."
    brew install cmake git

    # Vulkan SDK
    if ! has glslc; then
        info "Installing Vulkan SDK..."
        brew install vulkan-sdk
    fi
    ok "Vulkan SDK ready"

    # Optional: validation layers for debugging
    brew install vulkan-validationlayers 2>/dev/null || true
}

# ---------- Debian/Ubuntu ----------
install_debian() {
    info "Updating package list..."
    sudo apt-get update -qq

    info "Installing build tools..."
    sudo apt-get install -y --no-install-recommends \
        build-essential cmake git pkg-config

    info "Installing Vulkan SDK and dev libraries..."
    # Add LunarG repo if vulkan-sdk not available
    if ! apt-cache show vulkan-sdk &>/dev/null; then
        info "Adding LunarG Vulkan SDK repository..."
        sudo apt-get install -y wget gnupg
        wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc >/dev/null
        CODENAME=$(lsb_release -cs 2>/dev/null || echo "jammy")
        sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan.list \
            "https://packages.lunarg.com/vulkan/lunarg-vulkan-${CODENAME}.list" 2>/dev/null || \
        echo "deb https://packages.lunarg.com/vulkan/ ${CODENAME} main" | sudo tee /etc/apt/sources.list.d/lunarg-vulkan.list
        sudo apt-get update -qq
    fi
    sudo apt-get install -y vulkan-sdk

    info "Installing GLFW dependencies..."
    sudo apt-get install -y --no-install-recommends \
        libwayland-dev libxkbcommon-dev xorg-dev \
        libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

    ok "All dependencies installed"
}

# ---------- Fedora ----------
install_fedora() {
    info "Installing build tools..."
    sudo dnf install -y gcc gcc-c++ cmake git

    info "Installing Vulkan SDK..."
    sudo dnf install -y vulkan-devel vulkan-tools vulkan-validation-layers-devel \
        glslc glslang

    info "Installing GLFW dependencies..."
    sudo dnf install -y wayland-devel libxkbcommon-devel \
        libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel

    ok "All dependencies installed"
}

# ---------- Arch ----------
install_arch() {
    info "Installing dependencies via pacman..."
    sudo pacman -S --needed --noconfirm \
        base-devel cmake git \
        vulkan-devel vulkan-tools vulkan-validation-layers \
        shaderc glslang \
        wayland libxkbcommon \
        libx11 libxrandr libxinerama libxcursor libxi

    ok "All dependencies installed"
}

# ---------- Windows (MSYS2/MinGW) ----------
install_windows() {
    warn "Windows detected."
    echo ""
    echo "Please install the following manually:"
    echo "  1. Visual Studio 2019+ or MinGW-w64 (C++17 support)"
    echo "  2. CMake 3.20+          — https://cmake.org/download/"
    echo "  3. Vulkan SDK 1.2+      — https://vulkan.lunarg.com/sdk/home"
    echo "  4. Git                   — https://git-scm.com/download/win"
    echo ""
    echo "After installing, run from Developer Command Prompt:"
    echo "  mkdir build && cd build"
    echo "  cmake .. -G \"Visual Studio 17 2022\""
    echo "  cmake --build . --config Release"
    echo ""
    echo "Or use the build.sh script with MSYS2/Git Bash."
}

# ---------- Verify ----------
verify() {
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
        ok "All prerequisites satisfied! Run ./build.sh to compile and launch."
    else
        warn "$fail_count tool(s) missing. Please install them and retry."
    fi
}

# ---------- Main ----------
main() {
    echo "============================================"
    echo "  VoxelCraft — Dependency Setup"
    echo "============================================"
    echo ""

    detect_os

    case "$OS" in
        macos)         install_macos ;;
        debian)        install_debian ;;
        fedora)        install_fedora ;;
        arch)          install_arch ;;
        windows)       install_windows; exit 0 ;;
        linux-unknown) fail "Unrecognized Linux distro. Install manually: cmake, git, Vulkan SDK (glslc)." ;;
    esac

    verify
}

main "$@"
