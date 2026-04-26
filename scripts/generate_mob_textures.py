#!/usr/bin/env python3
"""
生成 Mycraft 生物纹理 — MC 标准 UV 布局
每种生物的纹理按照 MC 原版的长方体展开方式排列。

MC 标准长方体 UV 展开（从 uvX, uvY 开始）：
  顶面: (uvX+depth, uvY) size (width, depth)
  底面: (uvX+depth+width, uvY) size (width, depth)
  正面: (uvX+depth, uvY+depth) size (width, height)
  右面: (uvX, uvY+depth) size (depth, height)
  背面: (uvX+depth+width+depth, uvY+depth) size (width, height)
  左面: (uvX+depth+width, uvY+depth) size (depth, height)
"""

from PIL import Image, ImageDraw
import random
import os

OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "assets", "textures", "mobs")
os.makedirs(OUTPUT_DIR, exist_ok=True)

random.seed(42)

def fill_rect(img, x, y, w, h, color):
    """填充矩形区域"""
    draw = ImageDraw.Draw(img)
    draw.rectangle([x, y, x + w - 1, y + h - 1], fill=color)

def fill_rect_noise(img, x, y, w, h, base_color, noise=15):
    """填充带噪声的矩形区域，模拟MC像素风格"""
    for py in range(y, y + h):
        for px in range(x, x + w):
            if 0 <= px < img.width and 0 <= py < img.height:
                r = max(0, min(255, base_color[0] + random.randint(-noise, noise)))
                g = max(0, min(255, base_color[1] + random.randint(-noise, noise)))
                b = max(0, min(255, base_color[2] + random.randint(-noise, noise)))
                img.putpixel((px, py), (r, g, b, 255))

def fill_cuboid_uv(img, uvX, uvY, width, height, depth, color, noise=10):
    """按MC标准展开方式填充一个长方体的所有面UV区域"""
    # 顶面
    fill_rect_noise(img, uvX + depth, uvY, width, depth, 
                    lighten(color, 20), noise)
    # 底面
    fill_rect_noise(img, uvX + depth + width, uvY, width, depth,
                    darken(color, 20), noise)
    # 正面 (front)
    fill_rect_noise(img, uvX + depth, uvY + depth, width, height, color, noise)
    # 右面
    fill_rect_noise(img, uvX, uvY + depth, depth, height,
                    darken(color, 10), noise)
    # 背面
    fill_rect_noise(img, uvX + depth + width + depth, uvY + depth, width, height,
                    darken(color, 5), noise)
    # 左面
    fill_rect_noise(img, uvX + depth + width, uvY + depth, depth, height,
                    darken(color, 10), noise)

def lighten(color, amount):
    return tuple(min(255, c + amount) for c in color)

def darken(color, amount):
    return tuple(max(0, c - amount) for c in color)

def add_face_feature(img, x, y, w, h, color):
    """在指定位置添加面部特征（眼睛、鼻子等）"""
    fill_rect(img, x, y, w, h, color + (255,))

# ========== 猪 (Pig) ==========
# 64x32 纹理，整体粉色
# 头部: 8x8x8 at (0,0), 身体: 8x16x10 at (28,0), 腿: 4x6x4 at (0,16)
def generate_pig():
    img = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    pink = (234, 163, 150)
    dark_pink = (190, 130, 120)
    nose_pink = (210, 145, 132)
    
    # 头部 8x8x8 at UV(0,0)
    fill_cuboid_uv(img, 0, 0, 8, 8, 8, pink, 8)
    # 鼻子（在正面中央）— 正面位于 (8, 8) size 8x8
    # 鼻子 4x3 在正面中下部
    fill_rect_noise(img, 10, 13, 4, 3, nose_pink, 5)
    # 鼻孔
    fill_rect(img, 11, 14, 1, 1, dark_pink + (255,))
    fill_rect(img, 13, 14, 1, 1, dark_pink + (255,))
    # 眼睛
    fill_rect(img, 10, 11, 2, 2, (255, 255, 255, 255))
    fill_rect(img, 14, 11, 2, 2, (255, 255, 255, 255))
    fill_rect(img, 11, 11, 1, 1, (40, 20, 20, 255))
    fill_rect(img, 15, 11, 1, 1, (40, 20, 20, 255))
    
    # 身体 10x8x16 at UV(28,0) — width=10, height=8, depth=16
    # 注意：MC猪身体 width=8, height=8, depth=16 但我们用10宽
    fill_cuboid_uv(img, 28, 0, 8, 8, 6, pink, 8)
    
    # 腿 4x6x4 at UV(0,16)
    fill_cuboid_uv(img, 0, 16, 4, 6, 4, pink, 8)
    
    img.save(os.path.join(OUTPUT_DIR, "pig.png"))
    print("Generated pig.png")

# ========== 牛 (Cow) ==========
# 64x32 纹理，褐色+白色斑点
def generate_cow():
    img = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    brown = (68, 46, 30)
    white = (216, 210, 200)
    dark_brown = (50, 34, 22)
    
    # 头部 8x8x8 at UV(0,0)
    fill_cuboid_uv(img, 0, 0, 8, 8, 8, brown, 10)
    # 白色嘴部区域（正面下半部分）
    fill_rect_noise(img, 10, 13, 4, 3, white, 5)
    # 鼻孔
    fill_rect(img, 11, 14, 1, 1, dark_brown + (255,))
    fill_rect(img, 13, 14, 1, 1, dark_brown + (255,))
    # 眼睛
    fill_rect(img, 10, 10, 2, 2, (255, 255, 255, 255))
    fill_rect(img, 14, 10, 2, 2, (255, 255, 255, 255))
    fill_rect(img, 11, 10, 1, 1, (30, 15, 10, 255))
    fill_rect(img, 15, 10, 1, 1, (30, 15, 10, 255))
    
    # 身体 at UV(18,4) — width=12, height=10, depth=6
    fill_cuboid_uv(img, 18, 0, 10, 10, 6, brown, 10)
    # 添加白色斑点
    for _ in range(12):
        sx = random.randint(24, 40)
        sy = random.randint(6, 20)
        sw = random.randint(2, 4)
        sh = random.randint(2, 3)
        fill_rect_noise(img, sx, sy, sw, sh, white, 8)
    
    # 腿 4x12x4 at UV(0,16)
    fill_cuboid_uv(img, 0, 16, 4, 12, 4, brown, 8)
    # 白色蹄子
    fill_rect_noise(img, 4, 26, 4, 2, white, 5)
    
    img.save(os.path.join(OUTPUT_DIR, "cow.png"))
    print("Generated cow.png")

# ========== 羊 (Sheep) ==========
# 64x32 纹理，白色毛+灰色面部
def generate_sheep():
    img = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    wool_white = (228, 225, 218)
    face_gray = (150, 140, 130)
    dark_gray = (100, 90, 80)
    
    # 头部 6x6x6 at UV(0,0)
    fill_cuboid_uv(img, 0, 0, 6, 6, 6, face_gray, 10)
    # 眼睛
    fill_rect(img, 8, 8, 1, 2, (255, 255, 255, 255))
    fill_rect(img, 11, 8, 1, 2, (255, 255, 255, 255))
    fill_rect(img, 8, 8, 1, 1, (30, 20, 15, 255))
    fill_rect(img, 11, 8, 1, 1, (30, 20, 15, 255))
    # 嘴
    fill_rect_noise(img, 9, 11, 2, 1, dark_gray, 5)
    
    # 身体 at UV(28,0) — 毛茸茸的白色
    fill_cuboid_uv(img, 28, 0, 8, 8, 6, wool_white, 12)
    
    # 腿 4x6x4 at UV(0,16)
    fill_cuboid_uv(img, 0, 16, 4, 6, 4, face_gray, 8)
    
    img.save(os.path.join(OUTPUT_DIR, "sheep.png"))
    print("Generated sheep.png")

# ========== 鸡 (Chicken) ==========
# 64x32 纹理，白色身体+红色鸡冠
def generate_chicken():
    img = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    white = (230, 225, 220)
    red = (200, 50, 40)
    yellow = (210, 180, 50)
    dark = (60, 50, 40)
    
    # 头部 4x6x3 at UV(0,0)
    fill_cuboid_uv(img, 0, 0, 4, 6, 3, white, 8)
    # 鸡冠（头顶红色）
    fill_rect_noise(img, 4, 0, 2, 2, red, 5)
    # 眼睛
    fill_rect(img, 4, 4, 1, 1, dark + (255,))
    fill_rect(img, 6, 4, 1, 1, dark + (255,))
    # 喙
    fill_rect_noise(img, 5, 6, 2, 1, yellow, 3)
    # 肉垂
    fill_rect_noise(img, 5, 7, 1, 1, red, 3)
    
    # 身体 at UV(0,9) — width=6, height=8, depth=6
    fill_cuboid_uv(img, 0, 9, 6, 8, 6, white, 10)
    
    # 腿 at UV(26,0) — 细腿 2x5x2
    fill_cuboid_uv(img, 26, 0, 3, 5, 3, yellow, 5)
    
    img.save(os.path.join(OUTPUT_DIR, "chicken.png"))
    print("Generated chicken.png")

# ========== 僵尸 (Zombie) ==========
# 64x64 纹理，绿色皮肤+蓝色衣服
def generate_zombie():
    img = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    skin_green = (100, 140, 80)
    shirt_cyan = (60, 130, 130)
    pants_purple = (70, 55, 100)
    dark_green = (70, 100, 55)
    
    # 头部 8x8x8 at UV(0,0)
    fill_cuboid_uv(img, 0, 0, 8, 8, 8, skin_green, 10)
    # 眼睛 — 僵尸黑色空洞眼
    fill_rect(img, 10, 11, 2, 2, (20, 20, 20, 255))
    fill_rect(img, 14, 11, 2, 2, (20, 20, 20, 255))
    # 嘴
    fill_rect(img, 11, 14, 4, 1, dark_green + (255,))
    
    # 身体 8x12x4 at UV(16,16)
    fill_cuboid_uv(img, 16, 16, 8, 12, 4, shirt_cyan, 8)
    
    # 右腿 4x12x4 at UV(0,16)
    fill_cuboid_uv(img, 0, 16, 4, 12, 4, pants_purple, 8)
    
    # 左腿 4x12x4 at UV(0,32) (64x64纹理)
    fill_cuboid_uv(img, 0, 32, 4, 12, 4, pants_purple, 8)
    
    # 右臂 4x12x4 at UV(40,16)
    fill_cuboid_uv(img, 40, 16, 4, 12, 4, skin_green, 8)
    
    # 左臂 4x12x4 at UV(32,48)
    fill_cuboid_uv(img, 32, 48, 4, 12, 4, skin_green, 8)
    
    img.save(os.path.join(OUTPUT_DIR, "zombie.png"))
    print("Generated zombie.png")

# ========== 骷髅 (Skeleton) ==========
# 64x32 纹理，灰白色骨骼
def generate_skeleton():
    img = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    bone = (200, 200, 195)
    dark_bone = (160, 155, 150)
    
    # 头部 8x8x8 at UV(0,0)
    fill_cuboid_uv(img, 0, 0, 8, 8, 8, bone, 8)
    # 眼睛 — 黑色空洞
    fill_rect(img, 10, 11, 2, 2, (10, 10, 10, 255))
    fill_rect(img, 14, 11, 2, 2, (10, 10, 10, 255))
    # 鼻子
    fill_rect(img, 12, 13, 2, 1, (30, 30, 30, 255))
    # 嘴
    fill_rect(img, 10, 14, 6, 1, dark_bone + (255,))
    
    # 身体 8x12x4 at UV(16,16)
    fill_cuboid_uv(img, 16, 16, 8, 12, 4, bone, 8)
    
    # 腿 2x12x2 at UV(0,16)
    fill_cuboid_uv(img, 0, 16, 2, 12, 2, bone, 6)
    
    # 臂 2x12x2 at UV(40,16)
    fill_cuboid_uv(img, 40, 16, 2, 12, 2, bone, 6)
    
    img.save(os.path.join(OUTPUT_DIR, "skeleton.png"))
    print("Generated skeleton.png")

# ========== 蜘蛛 (Spider) ==========
# 64x32 纹理，深棕色+红眼
def generate_spider():
    img = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    dark_brown = (55, 40, 30)
    red_eye = (180, 20, 20)
    
    # 头部 8x8x8 at UV(32,4)
    fill_cuboid_uv(img, 32, 4, 8, 8, 8, dark_brown, 8)
    # 红色眼睛（8只，4对）
    fill_rect(img, 42, 13, 2, 2, red_eye + (255,))
    fill_rect(img, 46, 13, 2, 2, red_eye + (255,))
    fill_rect(img, 41, 15, 1, 1, red_eye + (255,))
    fill_rect(img, 48, 15, 1, 1, red_eye + (255,))
    
    # 身体 10x8x12 at UV(0,0)
    fill_cuboid_uv(img, 0, 0, 10, 8, 6, dark_brown, 10)
    
    # 腿 2x4x2 at UV(0,16) — 蜘蛛有8条腿但简化为4对
    fill_cuboid_uv(img, 0, 16, 2, 6, 2, dark_brown, 6)
    
    img.save(os.path.join(OUTPUT_DIR, "spider.png"))
    print("Generated spider.png")

# ========== 苦力怕 (Creeper) ==========
# 64x32 纹理，绿色斑驳
def generate_creeper():
    img = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    green = (80, 140, 60)
    dark_green = (50, 100, 40)
    
    # 头部 8x8x8 at UV(0,0)
    fill_cuboid_uv(img, 0, 0, 8, 8, 8, green, 15)
    # 苦力怕标志性的脸 — 黑色像素
    # 眼睛
    fill_rect(img, 10, 10, 2, 2, (10, 10, 10, 255))
    fill_rect(img, 14, 10, 2, 2, (10, 10, 10, 255))
    # 嘴（倒T形）
    fill_rect(img, 12, 12, 2, 1, (10, 10, 10, 255))
    fill_rect(img, 11, 13, 4, 2, (10, 10, 10, 255))
    fill_rect(img, 11, 15, 1, 1, (10, 10, 10, 255))
    fill_rect(img, 14, 15, 1, 1, (10, 10, 10, 255))
    
    # 身体 8x12x4 at UV(16,16)
    fill_cuboid_uv(img, 16, 16, 8, 12, 4, green, 15)
    # 添加深色斑驳
    for _ in range(8):
        sx = random.randint(20, 30)
        sy = random.randint(20, 28)
        fill_rect_noise(img, sx, sy, 2, 2, dark_green, 5)
    
    # 腿 4x6x4 at UV(0,16)
    fill_cuboid_uv(img, 0, 16, 4, 6, 4, green, 12)
    
    img.save(os.path.join(OUTPUT_DIR, "creeper.png"))
    print("Generated creeper.png")

if __name__ == "__main__":
    generate_pig()
    generate_cow()
    generate_sheep()
    generate_chicken()
    generate_zombie()
    generate_skeleton()
    generate_spider()
    generate_creeper()
    print("All mob textures generated!")
