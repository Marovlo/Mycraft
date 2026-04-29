from PIL import Image

# 原版经典牛
vanilla = Image.open("assets/textures/mobs/cow_vanilla/temperate_cow.png").convert("RGBA")
# 我们的牛
ours = Image.open("assets/textures/mobs/cow.png").convert("RGBA")

print(f"原版尺寸: {vanilla.size}")
print(f"我们尺寸: {ours.size}")

print("\n=== 原版牛: 非透明区域分析 ===")
for y in range(64):
    non_transparent = []
    for x in range(64):
        r, g, b, a = vanilla.getpixel((x, y))
        if a > 0:
            non_transparent.append(x)
    if non_transparent:
        print(f"  y={y:2d}: x=[{min(non_transparent):2d}..{max(non_transparent):2d}] ({len(non_transparent)} pixels)")

print("\n=== 我们的牛: 非透明区域分析 ===")
for y in range(64):
    non_transparent = []
    for x in range(64):
        r, g, b, a = ours.getpixel((x, y))
        if a > 0:
            non_transparent.append(x)
    if non_transparent:
        print(f"  y={y:2d}: x=[{min(non_transparent):2d}..{max(non_transparent):2d}] ({len(non_transparent)} pixels)")

# 可视化：用字符画展示非透明区域
print("\n=== 原版牛: 像素占用图 (# = 有像素, . = 透明) ===")
for y in range(64):
    row = ""
    for x in range(64):
        r, g, b, a = vanilla.getpixel((x, y))
        if a > 0:
            row += "#"
        else:
            row += "."
    print(f"y{y:02d} {row}")

print("\n=== 我们的牛: 像素占用图 (# = 有像素, . = 透明) ===")
for y in range(64):
    row = ""
    for x in range(64):
        r, g, b, a = ours.getpixel((x, y))
        if a > 0:
            row += "#"
        else:
            row += "."
    print(f"y{y:02d} {row}")
