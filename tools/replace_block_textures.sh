#!/bin/bash
# 方块贴图替换脚本：将原版MC方块贴图复制到我们的项目中
# 处理文件名映射差异

VANILLA_DIR="/Users/yukirazhang/work/mycraft/assets/minecraft_vanilla/textures/block"
OUR_DIR="/Users/yukirazhang/work/mycraft/assets/textures/blocks"
LOG="/tmp/block_texture_replace.log"

echo "=== 方块贴图替换开始 ===" > "$LOG"

# 直接同名复制的文件（原版和我们命名一致）
DIRECT_COPY=(
    "bedrock"
    "coal_ore"
    "cobblestone"
    "dandelion"
    "dead_bush"
    "diamond_ore"
    "dirt"
    "emerald_ore"
    "gold_ore"
    "gravel"
    "iron_ore"
    "lapis_ore"
    "oak_leaves"
    "oak_planks"
    "poppy"
    "red_mushroom"
    "redstone_ore"
    "sand"
    "sandstone"
    "snow"
    "stone"
    "white_wool"
    "cactus_side"
    "cactus_top"
    "crafting_table_side"
    "crafting_table_top"
    "copper_ore"
    "furnace_front"
    "furnace_side"
    "furnace_top"
    "oak_log_top"
    "spruce_log_top"
    "spruce_leaves"
    "brown_mushroom"
    "blue_orchid"
    "water_still"
    "torch"
)

for name in "${DIRECT_COPY[@]}"; do
    if [ -f "$VANILLA_DIR/${name}.png" ]; then
        cp "$VANILLA_DIR/${name}.png" "$OUR_DIR/${name}.png"
        echo "[OK] 直接复制: ${name}.png" >> "$LOG"
    else
        echo "[MISS] 原版不存在: ${name}.png" >> "$LOG"
    fi
done

# 需要重命名的文件映射 (原版名 -> 我们的名)
# 原版 grass_block_side.png -> 我们的 grass_side.png
cp "$VANILLA_DIR/grass_block_side.png" "$OUR_DIR/grass_side.png" && echo "[OK] grass_block_side -> grass_side" >> "$LOG"

# 原版 grass_block_top.png -> 我们的 grass_top.png
cp "$VANILLA_DIR/grass_block_top.png" "$OUR_DIR/grass_top.png" && echo "[OK] grass_block_top -> grass_top" >> "$LOG"

# 原版 oak_log.png (侧面) -> 我们的 oak_log_side.png
cp "$VANILLA_DIR/oak_log.png" "$OUR_DIR/oak_log_side.png" && echo "[OK] oak_log -> oak_log_side" >> "$LOG"

# 原版 spruce_log.png (侧面) -> 我们的 spruce_log_side.png
cp "$VANILLA_DIR/spruce_log.png" "$OUR_DIR/spruce_log_side.png" && echo "[OK] spruce_log -> spruce_log_side" >> "$LOG"

# 原版 short_grass.png -> 我们的 tall_grass.png (MC 1.20+ 改名了)
cp "$VANILLA_DIR/short_grass.png" "$OUR_DIR/tall_grass.png" && echo "[OK] short_grass -> tall_grass" >> "$LOG"

# 原版 furnace_front_on.png -> 我们的 furnace_front_on.png
cp "$VANILLA_DIR/furnace_front_on.png" "$OUR_DIR/furnace_front_on.png" && echo "[OK] furnace_front_on" >> "$LOG"

# 箱子贴图 - 原版MC的箱子是entity贴图(一张大展开图)，不是block贴图
# 我们的项目用的是简单的6面贴图，原版没有对应的block贴图
# 保留我们自己的箱子贴图
echo "[SKIP] chest_front/side/top - 原版箱子是entity贴图，保留我们的" >> "$LOG"

# destroy_stage 破坏动画贴图
for i in $(seq 0 9); do
    if [ -f "$VANILLA_DIR/destroy_stage_${i}.png" ]; then
        cp "$VANILLA_DIR/destroy_stage_${i}.png" "$OUR_DIR/destroy_stage_${i}.png"
        echo "[OK] destroy_stage_${i}" >> "$LOG"
    fi
done

echo "=== 方块贴图替换完成 ===" >> "$LOG"
cat "$LOG"
