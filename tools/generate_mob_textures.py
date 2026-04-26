#!/usr/bin/env python3
"""
generate_mob_textures.py — 为 Mycraft Phase 5 生成所有生物皮肤纹理和新物品纹理。
所有纹理尽量贴近原版 MC 的像素风格。

用法: python3 tools/generate_mob_textures.py
输出: assets/textures/mobs/*.png, assets/textures/items/*.png, assets/textures/hud/*.png
"""

import os
import struct
import zlib

# ========== 极简 PNG 写入器（不依赖 PIL）==========

def write_png(filepath, width, height, pixels):
    """写入 RGBA PNG 文件。pixels 是 [r,g,b,a, ...] 的扁平列表。"""
    def chunk(chunk_type, data):
        c = chunk_type + data
        crc = struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
        return struct.pack('>I', len(data)) + c + crc

    raw = b''
    for y in range(height):
        raw += b'\x00'  # filter: None
        for x in range(width):
            idx = (y * width + x) * 4
            raw += bytes(pixels[idx:idx+4])

    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    with open(filepath, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))


def make_image(w, h, bg=(0, 0, 0, 0)):
    """创建空白 RGBA 图像"""
    return list(bg) * (w * h)


def set_pixel(img, w, h, x, y, color):
    """设置单个像素"""
    if 0 <= x < w and 0 <= y < h:
        idx = (y * w + x) * 4
        img[idx:idx+4] = list(color)


def fill_rect(img, w, h, x0, y0, rw, rh, color):
    """填充矩形区域"""
    for dy in range(rh):
        for dx in range(rw):
            set_pixel(img, w, h, x0 + dx, y0 + dy, color)


def get_pixel(img, w, x, y):
    idx = (y * w + x) * 4
    return tuple(img[idx:idx+4])


# ========== 颜色定义 ==========

# 猪
PIG_PINK = (234, 172, 158, 255)
PIG_PINK_DARK = (198, 138, 124, 255)
PIG_NOSE = (214, 152, 138, 255)
PIG_NOSE_DARK = (178, 118, 104, 255)

# 牛
COW_BROWN = (68, 44, 28, 255)
COW_WHITE = (216, 216, 216, 255)
COW_GRAY = (168, 168, 168, 255)
COW_DARK = (48, 28, 16, 255)

# 羊
SHEEP_WHITE = (228, 224, 218, 255)
SHEEP_GRAY = (188, 184, 178, 255)
SHEEP_FACE = (168, 148, 128, 255)
SHEEP_FACE_DARK = (128, 108, 88, 255)

# 鸡
CHICKEN_WHITE = (238, 234, 228, 255)
CHICKEN_RED = (204, 44, 28, 255)
CHICKEN_YELLOW = (228, 188, 48, 255)
CHICKEN_ORANGE = (208, 128, 28, 255)

# 僵尸
ZOMBIE_GREEN = (68, 108, 48, 255)
ZOMBIE_GREEN_DARK = (48, 78, 28, 255)
ZOMBIE_SHIRT = (48, 88, 108, 255)
ZOMBIE_SHIRT_DARK = (28, 68, 88, 255)
ZOMBIE_PANTS = (58, 38, 108, 255)
ZOMBIE_PANTS_DARK = (38, 18, 88, 255)

# 骷髅
SKEL_WHITE = (198, 198, 198, 255)
SKEL_GRAY = (148, 148, 148, 255)
SKEL_DARK = (88, 88, 88, 255)

# 蜘蛛
SPIDER_BROWN = (52, 40, 28, 255)
SPIDER_BROWN_DARK = (32, 24, 16, 255)
SPIDER_RED = (168, 28, 28, 255)

# 苦力怕
CREEPER_GREEN = (68, 128, 48, 255)
CREEPER_GREEN_DARK = (48, 98, 28, 255)
CREEPER_GREEN_LIGHT = (88, 148, 68, 255)

TRANSPARENT = (0, 0, 0, 0)


# ========== 生物纹理生成 ==========

def draw_cuboid_uv(img, w, h, uvX, uvY, sx, sy, sz, top_c, bot_c, front_c, back_c, left_c, right_c):
    """按MC标准长方体展开布局绘制一个部件的6个面纹理。
    UV展开布局（从uvX,uvY开始）：
      第一行(y=uvY):        [空sz宽] [顶面sx*sz] [底面sx*sz]
      第二行(y=uvY+sz):     [左面sz*sy] [正面sx*sy] [右面sz*sy] [背面sx*sy]
    总宽 = 2*sz + 2*sx, 总高 = sz + sy
    """
    # 顶面: (uvX+sz, uvY) size (sx, sz)
    fill_rect(img, w, h, uvX + sz, uvY, sx, sz, top_c)
    # 底面: (uvX+sz+sx, uvY) size (sx, sz)
    fill_rect(img, w, h, uvX + sz + sx, uvY, sx, sz, bot_c)
    # 正面(-Z): (uvX+sz, uvY+sz) size (sx, sy)
    fill_rect(img, w, h, uvX + sz, uvY + sz, sx, sy, front_c)
    # 左面(-X): (uvX, uvY+sz) size (sz, sy)
    fill_rect(img, w, h, uvX, uvY + sz, sz, sy, left_c)
    # 右面(+X): (uvX+sz+sx, uvY+sz) size (sz, sy)
    fill_rect(img, w, h, uvX + sz + sx, uvY + sz, sz, sy, right_c)
    # 背面(+Z): (uvX+2*sz+sx, uvY+sz) size (sx, sy)
    fill_rect(img, w, h, uvX + 2 * sz + sx, uvY + sz, sx, sy, back_c)


def generate_pig_texture(base_dir):
    """猪皮肤 64x64 — 严格匹配C++模型UV布局
    C++: 头部 size={8,8,8} UV(0,0), 身体 size={8,8,10} UV(0,16), 腿 size={4,6,4} UV(0,34)
    """
    w, h = 64, 64
    img = make_image(w, h)

    # 头部 (sx=8, sy=8, sz=8) UV(0,0) -> 展开32x16 占用x=[0,32) y=[0,16)
    draw_cuboid_uv(img, w, h, 0, 0, 8, 8, 8,
                   PIG_PINK, PIG_PINK_DARK, PIG_PINK, PIG_PINK_DARK, PIG_PINK_DARK, PIG_PINK_DARK)
    # 头部细节：鼻子、眼睛（在正面上）
    # 正面位于 (uvX+sz, uvY+sz) = (8, 8), 大小 8x8
    fill_rect(img, w, h, 10, 12, 4, 3, PIG_NOSE)
    fill_rect(img, w, h, 11, 13, 2, 1, PIG_NOSE_DARK)
    set_pixel(img, w, h, 10, 11, (28, 28, 28, 255))
    set_pixel(img, w, h, 13, 11, (28, 28, 28, 255))

    # 身体 (sx=8, sy=8, sz=10) UV(0,16) -> 展开36x18 占用x=[0,36) y=[16,34)
    draw_cuboid_uv(img, w, h, 0, 16, 8, 8, 10,
                   PIG_PINK, PIG_PINK_DARK, PIG_PINK, PIG_PINK_DARK, PIG_PINK_DARK, PIG_PINK_DARK)

    # 腿 (sx=4, sy=6, sz=4) UV(0,34) -> 展开16x10 占用x=[0,16) y=[34,44)
    draw_cuboid_uv(img, w, h, 0, 34, 4, 6, 4,
                   PIG_PINK, PIG_PINK_DARK, PIG_PINK, PIG_PINK_DARK, PIG_PINK_DARK, PIG_PINK_DARK)

    write_png(os.path.join(base_dir, "mobs", "pig.png"), w, h, img)


def generate_cow_texture(base_dir):
    """牛皮肤 64x64 — 严格匹配C++模型UV布局
    C++: 头部 size={8,8,8} UV(0,0), 身体 size={10,10,12} UV(0,16), 腿 size={4,12,4} UV(0,38)
    """
    w, h = 64, 64
    img = make_image(w, h)

    # 头部 (sx=8, sy=8, sz=8) UV(0,0) -> 展开32x16 占用x=[0,32) y=[0,16)
    draw_cuboid_uv(img, w, h, 0, 0, 8, 8, 8,
                   COW_WHITE, COW_GRAY, COW_WHITE, COW_DARK, COW_BROWN, COW_BROWN)
    # 头部细节：斑块、眼睛、鼻子（在正面上，正面位于(8,8) 大小8x8）
    fill_rect(img, w, h, 9, 9, 2, 2, COW_BROWN)
    fill_rect(img, w, h, 13, 9, 2, 2, COW_BROWN)
    set_pixel(img, w, h, 10, 11, (28, 28, 28, 255))
    set_pixel(img, w, h, 13, 11, (28, 28, 28, 255))
    fill_rect(img, w, h, 10, 13, 4, 2, COW_GRAY)

    # 身体 (sx=10, sy=10, sz=12) UV(0,16) -> 展开44x22 占用x=[0,44) y=[16,38)
    draw_cuboid_uv(img, w, h, 0, 16, 10, 10, 12,
                   COW_WHITE, COW_GRAY, COW_WHITE, COW_DARK, COW_BROWN, COW_BROWN)
    # 身体斑块（在正面上，正面位于(0+12,16+12)=(12,28) 大小10x10）
    fill_rect(img, w, h, 13, 30, 4, 4, COW_BROWN)
    fill_rect(img, w, h, 18, 33, 3, 3, COW_BROWN)

    # 腿 (sx=4, sy=12, sz=4) UV(0,38) -> 展开16x16 占用x=[0,16) y=[38,54)
    draw_cuboid_uv(img, w, h, 0, 38, 4, 12, 4,
                   COW_WHITE, COW_GRAY, COW_WHITE, COW_DARK, COW_BROWN, COW_BROWN)

    write_png(os.path.join(base_dir, "mobs", "cow.png"), w, h, img)


def generate_sheep_texture(base_dir):
    """羊皮肤 64x64 — 严格匹配C++模型UV布局
    C++: 头部 size={6,6,6} UV(0,0), 身体 size={8,8,10} UV(0,12), 腿 size={4,6,4} UV(0,30)
    """
    w, h = 64, 64
    img = make_image(w, h)

    # 头部 (sx=6, sy=6, sz=6) UV(0,0) -> 展开24x12 占用x=[0,24) y=[0,12)
    draw_cuboid_uv(img, w, h, 0, 0, 6, 6, 6,
                   SHEEP_FACE, SHEEP_FACE_DARK, SHEEP_FACE, SHEEP_FACE_DARK, SHEEP_FACE_DARK, SHEEP_FACE_DARK)
    # 头部细节：眼睛（正面位于(6,6) 大小6x6）
    set_pixel(img, w, h, 7, 8, (28, 28, 28, 255))
    set_pixel(img, w, h, 10, 8, (28, 28, 28, 255))

    # 身体 (sx=8, sy=8, sz=10) UV(0,12) -> 展开36x18 占用x=[0,36) y=[12,30)
    draw_cuboid_uv(img, w, h, 0, 12, 8, 8, 10,
                   SHEEP_WHITE, SHEEP_GRAY, SHEEP_WHITE, SHEEP_GRAY, SHEEP_GRAY, SHEEP_GRAY)

    # 腿 (sx=4, sy=6, sz=4) UV(0,30) -> 展开16x10 占用x=[0,16) y=[30,40)
    draw_cuboid_uv(img, w, h, 0, 30, 4, 6, 4,
                   SHEEP_FACE, SHEEP_FACE_DARK, SHEEP_FACE, SHEEP_FACE_DARK, SHEEP_FACE_DARK, SHEEP_FACE_DARK)

    write_png(os.path.join(base_dir, "mobs", "sheep.png"), w, h, img)


def generate_chicken_texture(base_dir):
    """鸡皮肤 64x32 — 严格匹配C++模型UV布局
    C++: 头部 size={4,6,3} UV(0,0), 身体 size={6,6,6} UV(0,9), 腿 size={3,5,3} UV(26,0)
    """
    w, h = 64, 32
    img = make_image(w, h)

    # 头部 (sx=4, sy=6, sz=3) UV(0,0)
    # 展开宽=2*3+2*4=14, 展开高=3+6=9
    draw_cuboid_uv(img, w, h, 0, 0, 4, 6, 3,
                   CHICKEN_WHITE, CHICKEN_WHITE, CHICKEN_WHITE, CHICKEN_WHITE, CHICKEN_WHITE, CHICKEN_WHITE)
    # 头部细节：眼睛（正面位于(3,3) 大小4x6）
    set_pixel(img, w, h, 3, 4, (28, 28, 28, 255))
    set_pixel(img, w, h, 6, 4, (28, 28, 28, 255))
    # 喙
    fill_rect(img, w, h, 4, 6, 2, 1, CHICKEN_YELLOW)
    # 鸡冠（在正面顶部）
    fill_rect(img, w, h, 4, 3, 2, 1, CHICKEN_RED)
    # 肉垂
    set_pixel(img, w, h, 5, 8, CHICKEN_RED)

    # 身体 (sx=6, sy=6, sz=6) UV(0,9)
    # 展开宽=2*6+2*6=24, 展开高=6+6=12
    draw_cuboid_uv(img, w, h, 0, 9, 6, 6, 6,
                   CHICKEN_WHITE, CHICKEN_WHITE, CHICKEN_WHITE, CHICKEN_WHITE, CHICKEN_WHITE, CHICKEN_WHITE)

    # 腿 (sx=3, sy=5, sz=3) UV(26,0)
    # 展开宽=2*3+2*3=12, 展开高=3+5=8
    draw_cuboid_uv(img, w, h, 26, 0, 3, 5, 3,
                   CHICKEN_ORANGE, CHICKEN_ORANGE, CHICKEN_ORANGE, CHICKEN_ORANGE, CHICKEN_ORANGE, CHICKEN_ORANGE)

    write_png(os.path.join(base_dir, "mobs", "chicken.png"), w, h, img)


def generate_zombie_texture(base_dir):
    """僵尸皮肤 64x64 — 类人型布局"""
    w, h = 64, 64
    img = make_image(w, h)

    # 头部 (8x8x8) — 绿色皮肤
    fill_rect(img, w, h, 8, 0, 8, 8, ZOMBIE_GREEN)       # 头顶
    fill_rect(img, w, h, 16, 0, 8, 8, ZOMBIE_GREEN_DARK)  # 头底
    fill_rect(img, w, h, 8, 8, 8, 8, ZOMBIE_GREEN)        # 头正面
    fill_rect(img, w, h, 0, 8, 8, 8, ZOMBIE_GREEN_DARK)   # 头左
    fill_rect(img, w, h, 16, 8, 8, 8, ZOMBIE_GREEN_DARK)  # 头右
    fill_rect(img, w, h, 24, 8, 8, 8, ZOMBIE_GREEN_DARK)  # 头后
    # 眼睛 — 黑色
    set_pixel(img, w, h, 10, 12, (28, 28, 28, 255))
    set_pixel(img, w, h, 13, 12, (28, 28, 28, 255))
    # 嘴
    fill_rect(img, w, h, 10, 14, 4, 1, ZOMBIE_GREEN_DARK)

    # 身体 (8x12x4) — 青色衬衫
    fill_rect(img, w, h, 20, 16, 8, 4, ZOMBIE_SHIRT)      # 身体顶
    fill_rect(img, w, h, 28, 16, 8, 4, ZOMBIE_SHIRT_DARK) # 身体底
    fill_rect(img, w, h, 20, 20, 8, 12, ZOMBIE_SHIRT)     # 身体正面
    fill_rect(img, w, h, 16, 20, 4, 12, ZOMBIE_SHIRT_DARK) # 身体左
    fill_rect(img, w, h, 28, 20, 4, 12, ZOMBIE_SHIRT_DARK) # 身体右
    fill_rect(img, w, h, 32, 20, 8, 12, ZOMBIE_SHIRT_DARK) # 身体后
    # 裤子区域（身体下半部分）
    fill_rect(img, w, h, 20, 26, 8, 6, ZOMBIE_PANTS)
    fill_rect(img, w, h, 16, 26, 4, 6, ZOMBIE_PANTS_DARK)
    fill_rect(img, w, h, 28, 26, 4, 6, ZOMBIE_PANTS_DARK)
    fill_rect(img, w, h, 32, 26, 8, 6, ZOMBIE_PANTS_DARK)

    # 右臂 (4x12x4) — 绿色
    fill_rect(img, w, h, 40, 16, 4, 4, ZOMBIE_GREEN)
    fill_rect(img, w, h, 44, 16, 4, 4, ZOMBIE_GREEN_DARK)
    fill_rect(img, w, h, 40, 20, 4, 12, ZOMBIE_GREEN)
    fill_rect(img, w, h, 44, 20, 4, 12, ZOMBIE_GREEN_DARK)
    fill_rect(img, w, h, 48, 20, 4, 12, ZOMBIE_GREEN_DARK)
    fill_rect(img, w, h, 36, 20, 4, 12, ZOMBIE_GREEN_DARK)

    # 左臂 (4x12x4) — 绿色 (at 32,48 in 64x64 layout)
    fill_rect(img, w, h, 32, 48, 16, 16, ZOMBIE_GREEN)
    fill_rect(img, w, h, 36, 52, 4, 12, ZOMBIE_GREEN_DARK)

    # 右腿 (4x12x4)
    fill_rect(img, w, h, 0, 16, 4, 4, ZOMBIE_PANTS)
    fill_rect(img, w, h, 4, 16, 4, 4, ZOMBIE_PANTS_DARK)
    fill_rect(img, w, h, 0, 20, 4, 12, ZOMBIE_PANTS)
    fill_rect(img, w, h, 4, 20, 4, 12, ZOMBIE_PANTS_DARK)
    fill_rect(img, w, h, 8, 20, 4, 12, ZOMBIE_PANTS_DARK)
    fill_rect(img, w, h, 12, 20, 4, 12, ZOMBIE_PANTS_DARK)

    # 左腿 (at 16,48)
    fill_rect(img, w, h, 16, 48, 16, 16, ZOMBIE_PANTS)
    fill_rect(img, w, h, 20, 52, 4, 12, ZOMBIE_PANTS_DARK)

    write_png(os.path.join(base_dir, "mobs", "zombie.png"), w, h, img)


def generate_skeleton_texture(base_dir):
    """骷髅皮肤 64x32"""
    w, h = 64, 32
    img = make_image(w, h)

    # 头部 — 白灰色
    fill_rect(img, w, h, 8, 0, 8, 8, SKEL_WHITE)
    fill_rect(img, w, h, 16, 0, 8, 8, SKEL_GRAY)
    fill_rect(img, w, h, 8, 8, 8, 8, SKEL_WHITE)
    fill_rect(img, w, h, 0, 8, 8, 8, SKEL_GRAY)
    fill_rect(img, w, h, 16, 8, 8, 8, SKEL_GRAY)
    fill_rect(img, w, h, 24, 8, 8, 8, SKEL_DARK)
    # 眼睛 — 黑色空洞
    fill_rect(img, w, h, 10, 11, 2, 2, (8, 8, 8, 255))
    fill_rect(img, w, h, 13, 11, 2, 2, (8, 8, 8, 255))
    # 鼻子
    set_pixel(img, w, h, 11, 13, SKEL_DARK)
    set_pixel(img, w, h, 12, 13, SKEL_DARK)
    # 嘴
    fill_rect(img, w, h, 9, 14, 6, 1, SKEL_DARK)

    # 身体 — 窄骨架
    fill_rect(img, w, h, 20, 16, 8, 16, SKEL_WHITE)
    fill_rect(img, w, h, 28, 16, 8, 16, SKEL_GRAY)
    fill_rect(img, w, h, 16, 20, 4, 12, SKEL_GRAY)
    fill_rect(img, w, h, 36, 16, 4, 16, SKEL_DARK)

    # 臂
    fill_rect(img, w, h, 40, 16, 8, 16, SKEL_WHITE)
    fill_rect(img, w, h, 48, 16, 8, 16, SKEL_GRAY)

    # 腿
    fill_rect(img, w, h, 0, 16, 8, 16, SKEL_WHITE)
    fill_rect(img, w, h, 8, 16, 8, 16, SKEL_GRAY)

    write_png(os.path.join(base_dir, "mobs", "skeleton.png"), w, h, img)


def generate_spider_texture(base_dir):
    """蜘蛛皮肤 64x32"""
    w, h = 64, 32
    img = make_image(w, h)

    # 头部 — 深棕色
    fill_rect(img, w, h, 0, 0, 16, 16, SPIDER_BROWN)
    fill_rect(img, w, h, 2, 2, 12, 12, SPIDER_BROWN_DARK)
    # 红色眼睛 (8只)
    for ex in [3, 5, 9, 11]:
        for ey in [5, 7]:
            set_pixel(img, w, h, ex, ey, SPIDER_RED)

    # 身体 — 大腹部
    fill_rect(img, w, h, 16, 0, 16, 16, SPIDER_BROWN)
    fill_rect(img, w, h, 18, 2, 12, 12, SPIDER_BROWN_DARK)

    # 腿 (8条)
    fill_rect(img, w, h, 32, 0, 16, 8, SPIDER_BROWN)
    fill_rect(img, w, h, 32, 8, 16, 8, SPIDER_BROWN_DARK)
    fill_rect(img, w, h, 48, 0, 16, 8, SPIDER_BROWN)
    fill_rect(img, w, h, 48, 8, 16, 8, SPIDER_BROWN_DARK)

    # 下半部分腿
    fill_rect(img, w, h, 0, 16, 16, 16, SPIDER_BROWN)
    fill_rect(img, w, h, 16, 16, 16, 16, SPIDER_BROWN_DARK)
    fill_rect(img, w, h, 32, 16, 16, 16, SPIDER_BROWN)
    fill_rect(img, w, h, 48, 16, 16, 16, SPIDER_BROWN_DARK)

    write_png(os.path.join(base_dir, "mobs", "spider.png"), w, h, img)


def generate_creeper_texture(base_dir):
    """苦力怕皮肤 64x32"""
    w, h = 64, 32
    img = make_image(w, h)

    # 头部 — 绿色像素噪点
    import random
    random.seed(42)  # 确定性
    for y in range(16):
        for x in range(32):
            c = random.choice([CREEPER_GREEN, CREEPER_GREEN_DARK, CREEPER_GREEN_LIGHT])
            set_pixel(img, w, h, x, y, c)

    # 脸部 — 标志性苦力怕脸
    # 眼睛 (2x2 黑色方块)
    fill_rect(img, w, h, 10, 10, 2, 2, (8, 8, 8, 255))
    fill_rect(img, w, h, 14, 10, 2, 2, (8, 8, 8, 255))
    # 嘴 — 倒T形
    fill_rect(img, w, h, 11, 13, 4, 1, (8, 8, 8, 255))
    fill_rect(img, w, h, 12, 14, 2, 2, (8, 8, 8, 255))

    # 身体 — 绿色噪点
    for y in range(16, 32):
        for x in range(64):
            c = random.choice([CREEPER_GREEN, CREEPER_GREEN_DARK, CREEPER_GREEN_LIGHT])
            set_pixel(img, w, h, x, y, c)

    write_png(os.path.join(base_dir, "mobs", "creeper.png"), w, h, img)


# ========== 物品纹理生成 ==========

def generate_item_texture(base_dir, name, pixel_data):
    """生成 16x16 物品纹理"""
    w, h = 16, 16
    img = make_image(w, h)
    for y, row in enumerate(pixel_data):
        for x, color in enumerate(row):
            if color is not None:
                set_pixel(img, w, h, x, y, color)
    write_png(os.path.join(base_dir, "items", f"item_{name}.png"), w, h, img)


# 颜色简写
_ = None  # 透明
K = (28, 28, 28, 255)       # 黑色
W = (255, 255, 255, 255)    # 白色
R = (200, 48, 48, 255)      # 红色
Br = (148, 88, 48, 255)     # 棕色
Bd = (108, 58, 28, 255)     # 深棕
Bl = (178, 108, 68, 255)    # 浅棕
Pk = (238, 168, 148, 255)   # 粉色（生肉）
Pd = (198, 128, 108, 255)   # 深粉
Ck = (168, 108, 48, 255)    # 熟肉色
Cd = (138, 78, 28, 255)     # 深熟肉
Gy = (168, 168, 168, 255)   # 灰色
Gd = (128, 128, 128, 255)   # 深灰
Wh = (228, 218, 198, 255)   # 米白
Wd = (198, 188, 168, 255)   # 深米白
Gn = (88, 128, 48, 255)     # 绿色
Gnd = (58, 98, 28, 255)     # 深绿
Ye = (228, 188, 48, 255)    # 黄色
Or = (208, 128, 28, 255)    # 橙色
Pu = (108, 28, 128, 255)    # 紫色


def generate_raw_porkchop(base_dir):
    """生猪排"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Pk,Pk,Pk,_,_,_,_,_,_,_],
        [_,_,_,_,_,Pk,Pk,Pd,Pk,Pk,_,_,_,_,_,_],
        [_,_,_,_,Pk,Pk,Pd,Pd,Pk,Pk,Pk,_,_,_,_,_],
        [_,_,_,Pk,Pk,Pd,R,Pd,Pd,Pk,Pk,_,_,_,_,_],
        [_,_,_,Pk,Pd,R,R,R,Pd,Pk,Pk,Pk,_,_,_,_],
        [_,_,_,Pk,Pd,R,R,Pd,Pd,Pk,Pk,Pk,_,_,_,_],
        [_,_,_,_,Pk,Pd,Pd,Pd,Pk,Pk,Pk,_,_,_,_,_],
        [_,_,_,_,Pk,Pk,Pk,Pk,Pk,Pk,_,_,_,_,_,_],
        [_,_,_,_,_,Pk,Pk,Pk,Pk,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Wh,Wh,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "raw_porkchop", d)


def generate_cooked_porkchop(base_dir):
    """熟猪排"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Ck,Ck,Ck,_,_,_,_,_,_,_],
        [_,_,_,_,_,Ck,Ck,Cd,Ck,Ck,_,_,_,_,_,_],
        [_,_,_,_,Ck,Ck,Cd,Cd,Ck,Ck,Ck,_,_,_,_,_],
        [_,_,_,Ck,Ck,Cd,Bd,Cd,Cd,Ck,Ck,_,_,_,_,_],
        [_,_,_,Ck,Cd,Bd,Bd,Bd,Cd,Ck,Ck,Ck,_,_,_,_],
        [_,_,_,Ck,Cd,Bd,Bd,Cd,Cd,Ck,Ck,Ck,_,_,_,_],
        [_,_,_,_,Ck,Cd,Cd,Cd,Ck,Ck,Ck,_,_,_,_,_],
        [_,_,_,_,Ck,Ck,Ck,Ck,Ck,Ck,_,_,_,_,_,_],
        [_,_,_,_,_,Ck,Ck,Ck,Ck,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Wh,Wh,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "cooked_porkchop", d)


def generate_raw_beef(base_dir):
    """生牛排"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,R,R,R,R,_,_,_,_,_,_,_],
        [_,_,_,_,R,R,Pd,R,R,R,_,_,_,_,_,_],
        [_,_,_,R,R,Pd,Pk,Pd,R,R,R,_,_,_,_,_],
        [_,_,_,R,Pd,Pk,Pk,Pk,Pd,R,R,_,_,_,_,_],
        [_,_,_,R,R,Pd,Pk,Pd,R,R,R,_,_,_,_,_],
        [_,_,_,_,R,R,Pd,R,R,R,_,_,_,_,_,_],
        [_,_,_,_,_,R,R,R,R,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,R,R,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "raw_beef", d)


def generate_cooked_beef(base_dir):
    """熟牛排"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,Bd,Bd,Bd,Bd,_,_,_,_,_,_,_],
        [_,_,_,_,Bd,Bd,Cd,Bd,Bd,Bd,_,_,_,_,_,_],
        [_,_,_,Bd,Bd,Cd,Ck,Cd,Bd,Bd,Bd,_,_,_,_,_],
        [_,_,_,Bd,Cd,Ck,Ck,Ck,Cd,Bd,Bd,_,_,_,_,_],
        [_,_,_,Bd,Bd,Cd,Ck,Cd,Bd,Bd,Bd,_,_,_,_,_],
        [_,_,_,_,Bd,Bd,Cd,Bd,Bd,Bd,_,_,_,_,_,_],
        [_,_,_,_,_,Bd,Bd,Bd,Bd,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Bd,Bd,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "cooked_beef", d)


def generate_raw_chicken(base_dir):
    """生鸡肉"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Pk,Pk,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,Pk,Pk,Pk,Pk,_,_,_,_,_,_,_],
        [_,_,_,_,Pk,Pk,Pd,Pk,Pk,_,_,_,_,_,_,_],
        [_,_,_,_,Pk,Pd,Pd,Pd,Pk,_,_,_,_,_,_,_],
        [_,_,_,_,Pk,Pk,Pd,Pk,Pk,_,_,_,_,_,_,_],
        [_,_,_,_,_,Pk,Pk,Pk,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Pk,Pk,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Wh,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "raw_chicken", d)


def generate_cooked_chicken(base_dir):
    """熟鸡肉"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Ck,Ck,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,Ck,Ck,Ck,Ck,_,_,_,_,_,_,_],
        [_,_,_,_,Ck,Ck,Cd,Ck,Ck,_,_,_,_,_,_,_],
        [_,_,_,_,Ck,Cd,Cd,Cd,Ck,_,_,_,_,_,_,_],
        [_,_,_,_,Ck,Ck,Cd,Ck,Ck,_,_,_,_,_,_,_],
        [_,_,_,_,_,Ck,Ck,Ck,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Ck,Ck,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Wh,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "cooked_chicken", d)


def generate_leather(base_dir):
    """皮革"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,Br,Br,Br,Br,Br,_,_,_,_,_,_],
        [_,_,_,_,Br,Bl,Bl,Br,Bl,Br,Br,_,_,_,_,_],
        [_,_,_,Br,Bl,Br,Bl,Bl,Br,Bl,Br,_,_,_,_,_],
        [_,_,_,Br,Br,Bl,Br,Br,Bl,Br,Br,_,_,_,_,_],
        [_,_,_,Br,Bl,Br,Bl,Bl,Br,Bl,Br,_,_,_,_,_],
        [_,_,_,Br,Br,Bl,Br,Br,Bl,Br,Br,_,_,_,_,_],
        [_,_,_,_,Br,Br,Bl,Br,Br,Br,_,_,_,_,_,_],
        [_,_,_,_,_,Br,Br,Br,Br,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Bd,Bd,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "leather", d)


def generate_feather(base_dir):
    """羽毛"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,W,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,W,W,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,W,Gy,W,_,_,_,_],
        [_,_,_,_,_,_,_,_,W,Gy,W,_,_,_,_,_],
        [_,_,_,_,_,_,_,W,Gy,W,_,_,_,_,_,_],
        [_,_,_,_,_,_,W,Gy,W,_,_,_,_,_,_,_],
        [_,_,_,_,_,W,Gy,W,_,_,_,_,_,_,_,_],
        [_,_,_,_,W,Gy,W,_,_,_,_,_,_,_,_,_],
        [_,_,_,W,Gy,W,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,W,W,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,K,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,K,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,K,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "feather", d)


def generate_bone(base_dir):
    """骨头"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,Wh,Wd,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,Wh,Wd,Wh,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,Wd,Wh,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,Wh,Wd,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,Wh,Wd,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Wh,Wd,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,Wh,Wd,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,Wh,Wd,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,Wd,Wh,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,Wh,Wd,Wh,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,Wh,Wd,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "bone", d)


def generate_arrow(base_dir):
    """箭"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,K,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,K,Gy,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,K,Gy,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,K,Gy,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,Br,K,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,Br,Bd,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,Br,Bd,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,Br,Bd,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,Br,Bd,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,Br,Bd,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,W,Gy,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,W,Gy,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,W,_,W,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "arrow", d)


def generate_bow(base_dir):
    """弓"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,Br,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,Br,Bd,W,_,_,_,_],
        [_,_,_,_,_,_,_,_,Br,Bd,_,W,_,_,_,_],
        [_,_,_,_,_,_,_,Br,Bd,_,_,W,_,_,_,_],
        [_,_,_,_,_,_,Br,Bd,_,_,_,W,_,_,_,_],
        [_,_,_,_,_,_,Br,Bd,_,_,_,W,_,_,_,_],
        [_,_,_,_,_,_,Br,Bd,_,_,W,_,_,_,_,_],
        [_,_,_,_,_,_,_,Br,Bd,_,W,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,Br,Bd,W,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,Br,W,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,Br,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "bow", d)


def generate_gunpowder(base_dir):
    """火药"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,Gd,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,Gd,Gy,Gd,Gy,_,_,_,_,_,_,_],
        [_,_,_,_,Gy,Gd,Gy,Gd,Gy,Gd,_,_,_,_,_,_],
        [_,_,_,_,Gd,Gy,K,Gy,K,Gy,_,_,_,_,_,_],
        [_,_,_,Gd,Gy,K,Gd,K,Gd,K,Gy,_,_,_,_,_],
        [_,_,_,_,Gd,Gy,K,Gy,K,Gy,_,_,_,_,_,_],
        [_,_,_,_,Gy,Gd,Gy,Gd,Gy,Gd,_,_,_,_,_,_],
        [_,_,_,_,_,Gd,Gy,Gd,Gy,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,Gd,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "gunpowder", d)


def generate_string(base_dir):
    """蜘蛛丝/线"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,W,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,W,Gy,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,W,Gy,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,W,Gy,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,W,Gy,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,W,Gy,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,W,Gy,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,W,Gy,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,W,Gy,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,W,Gy,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,Gy,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "string", d)


def generate_spider_eye(base_dir):
    """蜘蛛眼"""
    d = [
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,R,R,R,R,_,_,_,_,_,_],
        [_,_,_,_,_,R,Pu,R,R,Pu,R,_,_,_,_,_],
        [_,_,_,_,R,Pu,K,Pu,Pu,K,Pu,R,_,_,_,_],
        [_,_,_,_,R,R,Pu,R,R,Pu,R,R,_,_,_,_],
        [_,_,_,_,_,R,R,R,R,R,R,_,_,_,_,_],
        [_,_,_,_,_,_,R,R,R,R,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
        [_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_],
    ]
    generate_item_texture(base_dir, "spider_eye", d)


def generate_wool_block(base_dir):
    """羊毛方块纹理 16x16"""
    w, h = 16, 16
    img = make_image(w, h)
    import random
    random.seed(99)
    for y in range(h):
        for x in range(w):
            c = random.choice([SHEEP_WHITE, SHEEP_GRAY,
                              (238, 234, 228, 255), (218, 214, 208, 255)])
            set_pixel(img, w, h, x, y, c)
    write_png(os.path.join(base_dir, "blocks", "white_wool.png"), w, h, img)


# ========== 主函数 ==========

def main():
    base_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                            "assets", "textures")

    print("生成生物纹理...")
    os.makedirs(os.path.join(base_dir, "mobs"), exist_ok=True)
    generate_pig_texture(base_dir)
    generate_cow_texture(base_dir)
    generate_sheep_texture(base_dir)
    generate_chicken_texture(base_dir)
    generate_zombie_texture(base_dir)
    generate_skeleton_texture(base_dir)
    generate_spider_texture(base_dir)
    generate_creeper_texture(base_dir)

    print("生成物品纹理...")
    generate_raw_porkchop(base_dir)
    generate_cooked_porkchop(base_dir)
    generate_raw_beef(base_dir)
    generate_cooked_beef(base_dir)
    generate_raw_chicken(base_dir)
    generate_cooked_chicken(base_dir)
    generate_leather(base_dir)
    generate_feather(base_dir)
    generate_bone(base_dir)
    generate_arrow(base_dir)
    generate_bow(base_dir)
    generate_gunpowder(base_dir)
    generate_string(base_dir)
    generate_spider_eye(base_dir)

    print("生成方块纹理...")
    generate_wool_block(base_dir)

    print("✅ 所有纹理生成完成！")
    print(f"   生物纹理: {base_dir}/mobs/")
    print(f"   物品纹理: {base_dir}/items/")
    print(f"   方块纹理: {base_dir}/blocks/")


if __name__ == "__main__":
    main()
