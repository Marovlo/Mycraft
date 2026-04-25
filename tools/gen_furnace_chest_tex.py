#!/usr/bin/env python3
"""Generate improved furnace + chest block textures (16x16 pixel art, MC style)."""
import struct, zlib, os

def write_png(path, pixels, w, h):
    """Write a minimal RGBA PNG file."""
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    raw = b''
    for y in range(h):
        raw += b'\x00'
        for x in range(w):
            r, g, b, a = pixels[y * w + x]
            raw += struct.pack('BBBB', r, g, b, a)
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)
    out = sig + chunk(b'IHDR', ihdr) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(out)

T = 16
BASE = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures')
BLOCKS = os.path.join(BASE, 'blocks')

# ============================================================
# Furnace textures — MC style cobblestone with prominent front opening
# ============================================================

# MC furnace colors
STONE_BASE   = (136, 136, 136, 255)  # 基础石头色
STONE_LIGHT  = (155, 155, 155, 255)  # 浅石头
STONE_DARK   = (115, 115, 115, 255)  # 深石头
STONE_DARKER = (100, 100, 100, 255)  # 更深
STONE_EDGE   = (85, 85, 85, 255)    # 边缘/裂缝

# 炉口颜色
FURNACE_HOLE_DARK   = (55, 55, 55, 255)     # 炉口深处
FURNACE_HOLE_MED    = (70, 70, 70, 255)      # 炉口中间
FURNACE_GRATE_DARK  = (45, 45, 45, 255)      # 格栅暗色
FURNACE_GRATE_LIGHT = (80, 80, 80, 255)      # 格栅亮色
FURNACE_FIRE_DARK   = (140, 50, 10, 255)     # 火焰暗色（底部暗示）
FURNACE_FIRE_MED    = (180, 80, 20, 255)     # 火焰中色

def make_stone_base():
    """生成MC风格的石砖底纹（类似cobblestone但更规整）"""
    px = [STONE_BASE] * (T * T)
    
    # 石砖纹理模式 - 模拟MC的furnace侧面/顶部
    # 用不规则的石块拼接效果
    import random
    rng = random.Random(42)
    
    # 先填充基础色带一些变化
    for y in range(T):
        for x in range(T):
            v = rng.randint(0, 10)
            if v < 3:
                px[y * T + x] = STONE_LIGHT
            elif v < 5:
                px[y * T + x] = STONE_DARK
            else:
                px[y * T + x] = STONE_BASE
    
    # 画石砖的接缝线（水平线）
    for x in range(T):
        px[0 * T + x] = STONE_EDGE    # 顶边
        px[4 * T + x] = STONE_EDGE    # 中间线1
        px[8 * T + x] = STONE_EDGE    # 中间线2
        px[12 * T + x] = STONE_EDGE   # 中间线3
        px[15 * T + x] = STONE_EDGE   # 底边
    
    # 画石砖的接缝线（竖直线，交错排列模拟砖墙）
    for y in range(T):
        px[y * T + 0] = STONE_EDGE    # 左边
        px[y * T + 15] = STONE_EDGE   # 右边
        
        if 0 <= y <= 4:
            px[y * T + 8] = STONE_EDGE
        elif 4 <= y <= 8:
            px[y * T + 4] = STONE_EDGE
            px[y * T + 12] = STONE_EDGE
        elif 8 <= y <= 12:
            px[y * T + 8] = STONE_EDGE
        elif 12 <= y <= 15:
            px[y * T + 4] = STONE_EDGE
            px[y * T + 12] = STONE_EDGE
    
    return px

# --- furnace_side: 石砖纹理 ---
side = make_stone_base()
write_png(os.path.join(BLOCKS, 'furnace_side.png'), side, T, T)

# --- furnace_top: 石砖纹理（稍微不同的种子） ---
top = [STONE_BASE] * (T * T)
import random
rng = random.Random(123)
for y in range(T):
    for x in range(T):
        v = rng.randint(0, 10)
        if v < 3:
            top[y * T + x] = STONE_LIGHT
        elif v < 5:
            top[y * T + x] = STONE_DARK
        else:
            top[y * T + x] = STONE_BASE

# 顶部也画接缝
for x in range(T):
    top[0 * T + x] = STONE_EDGE
    top[5 * T + x] = STONE_EDGE
    top[10 * T + x] = STONE_EDGE
    top[15 * T + x] = STONE_EDGE
for y in range(T):
    top[y * T + 0] = STONE_EDGE
    top[y * T + 5] = STONE_EDGE
    top[y * T + 10] = STONE_EDGE
    top[y * T + 15] = STONE_EDGE

write_png(os.path.join(BLOCKS, 'furnace_top.png'), top, T, T)

# --- furnace_front: 石砖底 + 明显的炉口 ---
front = make_stone_base()

# 炉口区域：x=3..12, y=3..13（占据正面中央大部分区域）
# 上半部分(y=3..7): 格栅/进料口 — 深灰色带格栅条纹
for y in range(3, 8):
    for x in range(3, 13):
        if y == 3:  # 顶边框
            front[y * T + x] = STONE_DARKER
        elif x == 3 or x == 12:  # 左右边框
            front[y * T + x] = STONE_DARKER
        elif y % 2 == 0:  # 格栅横条
            front[y * T + x] = FURNACE_GRATE_LIGHT
        else:  # 格栅间隙
            front[y * T + x] = FURNACE_GRATE_DARK

# 下半部分(y=8..13): 火焰口 — 更深的黑色，底部有橙红色暗示火焰
for y in range(8, 14):
    for x in range(3, 13):
        if y == 8:  # 分隔线
            front[y * T + x] = STONE_DARKER
        elif x == 3 or x == 12:  # 左右边框
            front[y * T + x] = STONE_DARKER
        elif y == 13:  # 底边框
            front[y * T + x] = STONE_DARKER
        else:
            # 内部：上方深黑，下方有火焰暗示
            if y <= 10:
                front[y * T + x] = FURNACE_HOLE_DARK
            elif y == 11:
                # 火焰暗示 — 交替橙红和深色
                if (x + y) % 2 == 0:
                    front[y * T + x] = FURNACE_FIRE_DARK
                else:
                    front[y * T + x] = FURNACE_HOLE_DARK
            else:  # y == 12
                if (x + y) % 3 == 0:
                    front[y * T + x] = FURNACE_FIRE_MED
                elif (x + y) % 3 == 1:
                    front[y * T + x] = FURNACE_FIRE_DARK
                else:
                    front[y * T + x] = FURNACE_HOLE_MED

write_png(os.path.join(BLOCKS, 'furnace_front.png'), front, T, T)

# --- furnace_front_on: 燃烧中的熔炉正面（火焰更亮） ---
front_on = list(front)  # 复制基础

FIRE_BRIGHT  = (255, 170, 50, 255)
FIRE_ORANGE  = (230, 120, 30, 255)
FIRE_RED     = (200, 70, 15, 255)
FIRE_YELLOW  = (255, 220, 80, 255)

# 替换下半部分的火焰区域为明亮的火焰
for y in range(9, 13):
    for x in range(4, 12):
        if y == 9:
            front_on[y * T + x] = FIRE_RED
        elif y == 10:
            if (x + y) % 2 == 0:
                front_on[y * T + x] = FIRE_ORANGE
            else:
                front_on[y * T + x] = FIRE_RED
        elif y == 11:
            if (x + y) % 3 == 0:
                front_on[y * T + x] = FIRE_BRIGHT
            elif (x + y) % 3 == 1:
                front_on[y * T + x] = FIRE_ORANGE
            else:
                front_on[y * T + x] = FIRE_YELLOW
        else:  # y == 12
            if (x + y) % 2 == 0:
                front_on[y * T + x] = FIRE_YELLOW
            else:
                front_on[y * T + x] = FIRE_BRIGHT

write_png(os.path.join(BLOCKS, 'furnace_front_on.png'), front_on, T, T)

print("Generated furnace textures: furnace_front.png, furnace_front_on.png, furnace_side.png, furnace_top.png")

# ============================================================
# Chest textures — MC style wooden chest with metal latch on front
# ============================================================

# MC chest colors
CHEST_WOOD      = (160, 120, 60, 255)   # 主体木色
CHEST_WOOD_LT   = (180, 140, 75, 255)   # 浅木色
CHEST_WOOD_DK   = (130, 95, 45, 255)    # 深木色
CHEST_EDGE      = (100, 70, 35, 255)    # 边框/接缝
CHEST_PLANK_LN  = (140, 105, 50, 255)   # 木板纹理线

# 锁扣颜色
LATCH_GOLD      = (200, 180, 80, 255)   # 金色锁扣
LATCH_GOLD_LT   = (230, 210, 110, 255)  # 锁扣高光
LATCH_GOLD_DK   = (160, 140, 55, 255)   # 锁扣暗部
LATCH_KEYHOLE   = (80, 60, 30, 255)     # 锁孔

def make_chest_wood_base(seed=200):
    """生成MC风格的箱子木纹底"""
    px = [CHEST_WOOD] * (T * T)
    rng = random.Random(seed)
    
    for y in range(T):
        for x in range(T):
            v = rng.randint(0, 12)
            if v < 3:
                px[y * T + x] = CHEST_WOOD_LT
            elif v < 5:
                px[y * T + x] = CHEST_WOOD_DK
            else:
                px[y * T + x] = CHEST_WOOD
    
    # 边框
    for x in range(T):
        px[0 * T + x] = CHEST_EDGE
        px[15 * T + x] = CHEST_EDGE
    for y in range(T):
        px[y * T + 0] = CHEST_EDGE
        px[y * T + 15] = CHEST_EDGE
    
    # 水平木板线（模拟木板拼接）
    for x in range(1, 15):
        px[5 * T + x] = CHEST_PLANK_LN
        px[10 * T + x] = CHEST_PLANK_LN
    
    return px

# --- chest_side: 木纹 ---
chest_side = make_chest_wood_base(200)
write_png(os.path.join(BLOCKS, 'chest_side.png'), chest_side, T, T)

# --- chest_top: 木纹（不同种子） ---
chest_top = make_chest_wood_base(300)
# 顶部加一个边框凹槽效果
for x in range(1, 15):
    chest_top[1 * T + x] = CHEST_WOOD_LT
    chest_top[14 * T + x] = CHEST_WOOD_DK
for y in range(1, 15):
    chest_top[y * T + 1] = CHEST_WOOD_LT
    chest_top[y * T + 14] = CHEST_WOOD_DK

write_png(os.path.join(BLOCKS, 'chest_top.png'), chest_top, T, T)

# --- chest_front: 木纹 + 金色锁扣 ---
chest_front = make_chest_wood_base(250)

# 锁扣位置：中央偏上，x=6..9, y=5..10
# 锁扣底座（方形金属片）
for y in range(5, 10):
    for x in range(6, 10):
        if y == 5 or y == 9 or x == 6 or x == 9:
            chest_front[y * T + x] = LATCH_GOLD_DK  # 边框
        else:
            chest_front[y * T + x] = LATCH_GOLD      # 主体

# 锁扣高光
chest_front[6 * T + 7] = LATCH_GOLD_LT
chest_front[6 * T + 8] = LATCH_GOLD_LT

# 锁孔（中央小黑点）
chest_front[7 * T + 7] = LATCH_KEYHOLE
chest_front[7 * T + 8] = LATCH_KEYHOLE
chest_front[8 * T + 7] = LATCH_KEYHOLE
chest_front[8 * T + 8] = LATCH_KEYHOLE

write_png(os.path.join(BLOCKS, 'chest_front.png'), chest_front, T, T)

print("Generated chest textures: chest_front.png, chest_side.png, chest_top.png")

# ============================================================
# HUD textures for furnace GUI
# ============================================================
HUD = os.path.join(BASE, 'hud')

# --- furnace_flame.png: 14x14 fire icon for fuel progress ---
# MC的熔炉火焰图标，从下往上填充表示燃料剩余
FLAME_W, FLAME_H = 14, 14
flame = [(0, 0, 0, 0)] * (FLAME_W * FLAME_H)

# 火焰形状（从底部宽到顶部尖）
flame_shape = [
    "......##......",  # y=0 (top)
    ".....####.....",
    ".....####.....",
    "....######....",
    "....######....",
    "...########...",
    "...########...",
    "..##########..",
    "..##########..",
    ".############.",
    ".############.",
    "##############",
    "##############",
    "##############",  # y=13 (bottom)
]

FLAME_OUTER = (220, 130, 20, 255)
FLAME_INNER = (255, 200, 50, 255)
FLAME_TIP   = (255, 240, 100, 255)

for y in range(FLAME_H):
    row = flame_shape[y]
    for x in range(FLAME_W):
        if row[x] == '#':
            # 颜色渐变：顶部亮黄，中间橙色，底部深橙
            if y < 4:
                flame[y * FLAME_W + x] = FLAME_TIP
            elif y < 8:
                flame[y * FLAME_W + x] = FLAME_INNER
            else:
                flame[y * FLAME_W + x] = FLAME_OUTER

write_png(os.path.join(HUD, 'furnace_flame.png'), flame, FLAME_W, FLAME_H)

# --- furnace_flame_bg.png: 同样形状但灰色（未燃烧状态） ---
flame_bg = [(0, 0, 0, 0)] * (FLAME_W * FLAME_H)
FLAME_BG_COL = (80, 80, 80, 255)
for y in range(FLAME_H):
    row = flame_shape[y]
    for x in range(FLAME_W):
        if row[x] == '#':
            flame_bg[y * FLAME_W + x] = FLAME_BG_COL

write_png(os.path.join(HUD, 'furnace_flame_bg.png'), flame_bg, FLAME_W, FLAME_H)

# --- furnace_arrow.png: 22x16 progress arrow (white, filled from left to right) ---
ARROW_W, ARROW_H = 22, 16
arrow = [(0, 0, 0, 0)] * (ARROW_W * ARROW_H)

# 箭头形状：左边是矩形主体，右边是三角形箭头
ARROW_COL = (255, 255, 255, 255)
for y in range(ARROW_H):
    for x in range(ARROW_W):
        # 矩形主体部分 x=0..14, y=4..11
        if x <= 14 and 4 <= y <= 11:
            arrow[y * ARROW_W + x] = ARROW_COL
        # 三角形箭头部分 x=15..21
        elif x > 14:
            # 箭头从 x=15 开始，中心 y=7.5
            half_h = (ARROW_W - 1 - x) * 0.8 + 0.5
            center = 7.5
            if center - half_h <= y <= center + half_h:
                arrow[y * ARROW_W + x] = ARROW_COL

write_png(os.path.join(HUD, 'furnace_arrow.png'), arrow, ARROW_W, ARROW_H)

# --- furnace_arrow_bg.png: 同样形状但灰色（未完成状态） ---
arrow_bg = [(0, 0, 0, 0)] * (ARROW_W * ARROW_H)
ARROW_BG_COL = (80, 80, 80, 180)
for y in range(ARROW_H):
    for x in range(ARROW_W):
        if arrow[y * ARROW_W + x][3] > 0:
            arrow_bg[y * ARROW_W + x] = ARROW_BG_COL

write_png(os.path.join(HUD, 'furnace_arrow_bg.png'), arrow_bg, ARROW_W, ARROW_H)

print("Generated furnace HUD textures: furnace_flame.png, furnace_flame_bg.png, furnace_arrow.png, furnace_arrow_bg.png")
