#!/bin/bash
# VoxelCraft — quick runner (skips build).
#
# Usage:
#   ./run.sh                    Run normally.
#   ./run.sh --debug            Enable all runtime log categories.
#   ./run.sh --debug=entity,ui  Enable only the given categories.
#   You can also set VOXEL_DEBUG=entity,ui in the environment.
cd "$(dirname "$0")/build" || exit 1
export VK_ICD_FILENAMES=/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH=/opt/homebrew/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d
./VoxelEngine "$@"
