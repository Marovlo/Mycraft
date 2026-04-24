#!/usr/bin/env bash
# ============================================================================
# VoxelCraft — One-click build & run
# Usage:
#   ./build.sh              Build (Release) and run
#   ./build.sh --debug      Build (Debug) and run
#   ./build.sh --build      Build only, don't run
#   ./build.sh --run        Run only (skip build)
#   ./build.sh --clean      Clean build directory and rebuild
#   ./build.sh --jobs N     Use N parallel jobs (default: auto)
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="Release"
DO_BUILD=true
DO_RUN=true
DO_CLEAN=false
JOBS=""

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; exit 1; }

# ---------- Parse args ----------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)  BUILD_TYPE="Debug"; shift ;;
        --build)  DO_RUN=false; shift ;;
        --run)    DO_BUILD=false; shift ;;
        --clean)  DO_CLEAN=true; shift ;;
        --jobs)   JOBS="$2"; shift 2 ;;
        -h|--help)
            head -n 10 "$0" | grep "^#" | sed 's/^# *//'
            exit 0
            ;;
        *) warn "Unknown option: $1"; shift ;;
    esac
done

# ---------- Detect parallel jobs ----------
detect_jobs() {
    if [ -n "$JOBS" ]; then
        echo "$JOBS"
        return
    fi
    case "$(uname -s)" in
        Darwin)  sysctl -n hw.ncpu 2>/dev/null || echo 4 ;;
        Linux)   nproc 2>/dev/null || echo 4 ;;
        MINGW*|MSYS*|CYGWIN*) echo "${NUMBER_OF_PROCESSORS:-4}" ;;
        *)       echo 4 ;;
    esac
}

# ---------- Setup Vulkan env (macOS) ----------
setup_vulkan_env() {
    case "$(uname -s)" in
        Darwin)
            # MoltenVK ICD
            if [ -f /opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json ]; then
                export VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json
            elif [ -f /usr/local/etc/vulkan/icd.d/MoltenVK_icd.json ]; then
                export VK_ICD_FILENAMES=/usr/local/etc/vulkan/icd.d/MoltenVK_icd.json
            fi
            # Validation layers
            if [ -d /opt/homebrew/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d ]; then
                export VK_LAYER_PATH=/opt/homebrew/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d
            elif [ -d /usr/local/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d ]; then
                export VK_LAYER_PATH=/usr/local/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d
            fi
            ;;
    esac
}

# ---------- Preflight checks ----------
preflight() {
    local missing=()
    command -v cmake &>/dev/null || missing+=("cmake")
    command -v git   &>/dev/null || missing+=("git")

    if [ ${#missing[@]} -gt 0 ]; then
        fail "Missing tools: ${missing[*]}. Run ./setup.sh first."
    fi
}

# ---------- Build ----------
build() {
    local jobs
    jobs=$(detect_jobs)

    if $DO_CLEAN && [ -d "$BUILD_DIR" ]; then
        info "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    info "Configuring (${BUILD_TYPE})..."
    cmake "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" 2>&1 | tail -5

    echo ""
    info "Building with ${jobs} parallel jobs..."
    local start_time=$SECONDS
    cmake --build . -j "$jobs" 2>&1 | tail -10

    local elapsed=$((SECONDS - start_time))
    ok "Build complete in ${elapsed}s"
}

# ---------- Run ----------
run() {
    local exe="${BUILD_DIR}/VoxelEngine"

    if [ ! -f "$exe" ]; then
        fail "Executable not found at ${exe}. Build first with: ./build.sh --build"
    fi

    setup_vulkan_env

    echo ""
    ok "Launching VoxelCraft..."
    echo "────────────────────────────────────────"
    cd "$BUILD_DIR"
    exec ./VoxelEngine "$@"
}

# ---------- Main ----------
main() {
    echo "============================================"
    echo "  VoxelCraft — Build & Run"
    echo "============================================"
    echo ""

    preflight

    if $DO_BUILD; then
        build
    fi

    if $DO_RUN; then
        run
    fi
}

main "$@"
