#!/bin/bash
# 物品/工具贴图替换脚本：将原版MC物品贴图复制到我们的项目中
# 我们的文件名有 item_ 和 tool_ 前缀，原版没有

VANILLA_DIR="/Users/yukirazhang/work/mycraft/assets/minecraft_vanilla/textures/item"
OUR_DIR="/Users/yukirazhang/work/mycraft/assets/textures/items"
LOG="/tmp/item_texture_replace.log"

echo "=== 物品贴图替换开始 ===" > "$LOG"

# 矿物/材料: 原版名 -> 我们的 item_名
ITEMS=(
    "coal:item_coal"
    "raw_iron:item_raw_iron"
    "raw_gold:item_raw_gold"
    "raw_copper:item_raw_copper"
    "diamond:item_diamond"
    "emerald:item_emerald"
    "lapis_lazuli:item_lapis_lazuli"
    "redstone:item_redstone"
    "iron_ingot:item_iron_ingot"
    "gold_ingot:item_gold_ingot"
    "copper_ingot:item_copper_ingot"
    "stick:item_stick"
)

for entry in "${ITEMS[@]}"; do
    vanilla="${entry%%:*}"
    ours="${entry##*:}"
    if [ -f "$VANILLA_DIR/${vanilla}.png" ]; then
        cp "$VANILLA_DIR/${vanilla}.png" "$OUR_DIR/${ours}.png"
        echo "[OK] ${vanilla} -> ${ours}" >> "$LOG"
    else
        echo "[MISS] ${vanilla}.png not found" >> "$LOG"
    fi
done

# 食物: 原版 porkchop -> 我们的 item_raw_porkchop
FOODS=(
    "porkchop:item_raw_porkchop"
    "cooked_porkchop:item_cooked_porkchop"
    "beef:item_raw_beef"
    "cooked_beef:item_cooked_beef"
    "chicken:item_raw_chicken"
    "cooked_chicken:item_cooked_chicken"
)

for entry in "${FOODS[@]}"; do
    vanilla="${entry%%:*}"
    ours="${entry##*:}"
    if [ -f "$VANILLA_DIR/${vanilla}.png" ]; then
        cp "$VANILLA_DIR/${vanilla}.png" "$OUR_DIR/${ours}.png"
        echo "[OK] ${vanilla} -> ${ours}" >> "$LOG"
    else
        echo "[MISS] ${vanilla}.png not found" >> "$LOG"
    fi
done

# 生物掉落物
DROPS=(
    "leather:item_leather"
    "feather:item_feather"
    "bone:item_bone"
    "arrow:item_arrow"
    "bow:item_bow"
    "gunpowder:item_gunpowder"
    "string:item_string"
    "spider_eye:item_spider_eye"
)

for entry in "${DROPS[@]}"; do
    vanilla="${entry%%:*}"
    ours="${entry##*:}"
    if [ -f "$VANILLA_DIR/${vanilla}.png" ]; then
        cp "$VANILLA_DIR/${vanilla}.png" "$OUR_DIR/${ours}.png"
        echo "[OK] ${vanilla} -> ${ours}" >> "$LOG"
    else
        echo "[MISS] ${vanilla}.png not found" >> "$LOG"
    fi
done

# 工具: 原版 wooden_pickaxe -> 我们的 tool_wooden_pickaxe
# 注意: 原版金工具叫 golden_xxx，我们叫 tool_gold_xxx
for mat in wooden stone iron diamond; do
    for tool in pickaxe axe shovel sword hoe; do
        if [ -f "$VANILLA_DIR/${mat}_${tool}.png" ]; then
            cp "$VANILLA_DIR/${mat}_${tool}.png" "$OUR_DIR/tool_${mat}_${tool}.png"
            echo "[OK] ${mat}_${tool} -> tool_${mat}_${tool}" >> "$LOG"
        else
            echo "[MISS] ${mat}_${tool}.png not found" >> "$LOG"
        fi
    done
done

# 金工具特殊处理: 原版 golden_xxx -> 我们的 tool_gold_xxx
for tool in pickaxe axe shovel sword hoe; do
    if [ -f "$VANILLA_DIR/golden_${tool}.png" ]; then
        cp "$VANILLA_DIR/golden_${tool}.png" "$OUR_DIR/tool_gold_${tool}.png"
        echo "[OK] golden_${tool} -> tool_gold_${tool}" >> "$LOG"
    else
        echo "[MISS] golden_${tool}.png not found" >> "$LOG"
    fi
done

echo "=== 物品贴图替换完成 ===" >> "$LOG"
cat "$LOG"
