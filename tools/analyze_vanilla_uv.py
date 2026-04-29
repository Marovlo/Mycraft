#!/usr/bin/env python3
"""分析原版MC动物贴图的UV布局，确定每个部件的精确UV参数"""
from PIL import Image
import sys

def analyze_uv_region(img, name, uvX, uvY, sx, sy, sz):
    """检查一个部件的UV展开区域是否有不透明像素"""
    w, h = img.size
    # MC标准展开：
    # 顶面: (uvX+sz, uvY) to (uvX+sz+sx, uvY+sz)
    # 底面: (uvX+sz+sx, uvY) to (uvX+sz+sx+sx, uvY+sz)
    # 左面: (uvX, uvY+sz) to (uvX+sz, uvY+sz+sy)
    # 正面: (uvX+sz, uvY+sz) to (uvX+sz+sx, uvY+sz+sy)
    # 右面: (uvX+sz+sx, uvY+sz) to (uvX+sz+sx+sz, uvY+sz+sy)
    # 背面: (uvX+sz+sx+sz, uvY+sz) to (uvX+sz+sx+sz+sx, uvY+sz+sy)
    
    total_w = 2 * (sz + sx)
    total_h = sz + sy
    
    opaque = 0
    total = 0
    oob = 0
    
    for dy in range(total_h):
        for dx in range(total_w):
            px = uvX + dx
            py = uvY + dy
            if px >= w or py >= h:
                oob += 1
                continue
            total += 1
            r, g, b, a = img.getpixel((px, py))
            if a > 0:
                opaque += 1
    
    pct = (opaque / total * 100) if total > 0 else 0
    status = "OK" if pct > 50 else "EMPTY"
    print(f"  {name}: UV({uvX},{uvY}) size({sx},{sy},{sz}) -> region {total_w}x{total_h} | opaque={opaque}/{total} ({pct:.0f}%) OOB={oob} [{status}]")
    return pct > 50

# 原版MC标准UV参数（来自MC Wiki和Java源码）
animals = {
    "pig": {
        "file": "/Users/yukirazhang/work/mycraft/assets/minecraft_vanilla/textures/entity/pig/temperate_pig.png",
        "parts": [
            # name, uvX, uvY, sx(宽), sy(高), sz(深/长)
            ("head_8x8x8", 0, 0, 8, 8, 8),
            ("body_10x16x8", 28, 8, 10, 16, 8),  # MC: body width=10, length=16, height=8
            ("leg_4x4x6", 0, 16, 4, 6, 4),
            # 鼻子
            ("snout_4x3x1", 16, 16, 4, 3, 1),
        ]
    },
    "cow": {
        "file": "/Users/yukirazhang/work/mycraft/assets/minecraft_vanilla/textures/entity/cow/temperate_cow.png",
        "parts": [
            ("head_8x8x8", 0, 0, 8, 8, 8),
            ("body_12x18x10", 18, 4, 12, 18, 10),  # MC cow body
            ("leg_4x4x12", 0, 16, 4, 12, 4),
            # 角
            ("horn", 22, 0, 1, 3, 1),
        ]
    },
    "sheep": {
        "file": "/Users/yukirazhang/work/mycraft/assets/minecraft_vanilla/textures/entity/sheep/sheep.png",
        "parts": [
            ("head_6x6x8", 0, 0, 6, 6, 8),  # MC sheep head: 6 wide, 6 tall, 8 deep
            ("body_6x9x6", 28, 8, 6, 9, 6),  # MC sheep body
            ("leg_4x4x12", 0, 16, 4, 12, 4),
        ]
    },
    "chicken": {
        "file": "/Users/yukirazhang/work/mycraft/assets/minecraft_vanilla/textures/entity/chicken/temperate_chicken.png",
        "parts": [
            ("head_4x6x3", 0, 0, 4, 6, 3),
            ("body_6x8x6", 0, 9, 6, 8, 6),  # MC chicken body
            ("leg_3x5x3", 26, 0, 3, 5, 3),
            # 翅膀
            ("wing_6x4x1", 24, 13, 6, 4, 1),
            # 嘴
            ("beak_4x2x2", 14, 0, 4, 2, 2),
        ]
    }
}

print("=== 原版MC动物贴图UV分析 ===\n")

for animal_name, info in animals.items():
    try:
        img = Image.open(info["file"]).convert("RGBA")
        print(f"{animal_name} ({img.size[0]}x{img.size[1]}):")
        for part in info["parts"]:
            analyze_uv_region(img, *part)
        print()
    except Exception as e:
        print(f"{animal_name}: ERROR - {e}\n")

# 现在尝试MC原版的精确参数
print("\n=== 验证MC原版精确UV参数 ===\n")

# 猪 - 根据MC Java源码 (ModelPig extends ModelQuadruped)
# head: 8x8x8, UV(0,0)
# body: 8x16x10 (height=8, width=10, length=16), UV(28,8)  
# 注意MC中body的参数顺序是 (width, height, length) 但UV展开用的是 (sx=width, sy=height, sz=length)
# 实际上MC Java中 body 的 addBox 参数是 (-5, -10, -7, 10, 16, 8) UV(28,8)
# 这意味着 sx=10, sy=16, sz=8 -> UV展开宽度 = 2*(8+10) = 36, 高度 = 8+16 = 24

pig_img = Image.open(animals["pig"]["file"]).convert("RGBA")
print("猪 - MC Java精确参数:")
analyze_uv_region(pig_img, "head(8,8,8)@(0,0)", 0, 0, 8, 8, 8)
analyze_uv_region(pig_img, "body(10,16,8)@(28,8)", 28, 8, 10, 16, 8)
analyze_uv_region(pig_img, "leg(4,6,4)@(0,16)", 0, 16, 4, 6, 4)

cow_img = Image.open(animals["cow"]["file"]).convert("RGBA")
print("\n牛 - MC Java精确参数:")
analyze_uv_region(cow_img, "head(8,8,8)@(0,0)", 0, 0, 8, 8, 8)
# MC cow body: addBox(-6, -10, -7, 12, 18, 10, ...) UV(18,4)
analyze_uv_region(cow_img, "body(12,18,10)@(18,4)", 18, 4, 12, 18, 10)
analyze_uv_region(cow_img, "leg(4,12,4)@(0,16)", 0, 16, 4, 12, 4)

sheep_img = Image.open(animals["sheep"]["file"]).convert("RGBA")
print("\n羊 - MC Java精确参数:")
analyze_uv_region(sheep_img, "head(6,6,8)@(0,0)", 0, 0, 6, 6, 8)
# MC sheep body: addBox(-4, -10, -7, 8, 16, 6, ...) UV(28,8)  -> sx=8, sy=16, sz=6
analyze_uv_region(sheep_img, "body(8,16,6)@(28,8)", 28, 8, 8, 16, 6)
analyze_uv_region(sheep_img, "leg(4,12,4)@(0,16)", 0, 16, 4, 12, 4)

chicken_img = Image.open(animals["chicken"]["file"]).convert("RGBA")
print("\n鸡 - MC Java精确参数:")
analyze_uv_region(chicken_img, "head(4,6,3)@(0,0)", 0, 0, 4, 6, 3)
# MC chicken body: addBox(-3, -4, -3, 6, 8, 6, ...) UV(0,9)
analyze_uv_region(chicken_img, "body(6,8,6)@(0,9)", 0, 9, 6, 8, 6)
analyze_uv_region(chicken_img, "leg(3,5,3)@(26,0)", 26, 0, 3, 5, 3)
