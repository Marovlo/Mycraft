#!/usr/bin/env python3
"""验证新UV布局是否有越界或重叠"""

def check_cuboid_uv(name, part, uvX, uvY, sx, sy, sz, texW, texH):
    """检查一个部件的UV展开是否越界"""
    total_w = 2 * sz + 2 * sx
    total_h = sz + sy
    end_x = uvX + total_w
    end_y = uvY + total_h
    ok = True
    if end_x > texW:
        print(f"  ❌ {name} {part}: UV宽度越界! x=[{uvX},{end_x}) > texW={texW}")
        ok = False
    if end_y > texH:
        print(f"  ❌ {name} {part}: UV高度越界! y=[{uvY},{end_y}) > texH={texH}")
        ok = False
    if ok:
        print(f"  ✅ {name} {part}: UV区域 x=[{uvX},{end_x}) y=[{uvY},{end_y}) 展开{total_w}x{total_h} OK")
    return (uvX, uvY, end_x, end_y)

def check_overlap(name, regions):
    """检查多个区域是否重叠"""
    for i in range(len(regions)):
        for j in range(i+1, len(regions)):
            r1_name, (x1, y1, x2, y2) = regions[i]
            r2_name, (x3, y3, x4, y4) = regions[j]
            if x1 < x4 and x3 < x2 and y1 < y4 and y3 < y2:
                print(f"  ⚠️  {name}: {r1_name} [{x1},{y1},{x2},{y2}] 与 {r2_name} [{x3},{y3},{x4},{y4}] 重叠!")
            else:
                print(f"  ✅ {name}: {r1_name} 与 {r2_name} 无重叠")

print("=== 猪 (64x64) ===")
pig_regions = []
pig_regions.append(("头部", check_cuboid_uv("猪", "头部", 0, 0, 6, 6, 6, 64, 64)))
pig_regions.append(("身体", check_cuboid_uv("猪", "身体", 0, 12, 8, 8, 14, 64, 64)))
pig_regions.append(("腿部", check_cuboid_uv("猪", "腿部", 0, 34, 4, 6, 4, 64, 64)))
check_overlap("猪", pig_regions)

print("\n=== 牛 (64x64) ===")
cow_regions = []
cow_regions.append(("头部", check_cuboid_uv("牛", "头部", 0, 0, 8, 8, 8, 64, 64)))
cow_regions.append(("身体", check_cuboid_uv("牛", "身体", 0, 16, 10, 10, 16, 64, 64)))
cow_regions.append(("腿部", check_cuboid_uv("牛", "腿部", 0, 42, 4, 12, 4, 64, 64)))
check_overlap("牛", cow_regions)

print("\n=== 羊 (64x64) ===")
sheep_regions = []
sheep_regions.append(("头部", check_cuboid_uv("羊", "头部", 0, 0, 6, 6, 6, 64, 64)))
sheep_regions.append(("身体", check_cuboid_uv("羊", "身体", 0, 12, 8, 8, 14, 64, 64)))
sheep_regions.append(("腿部", check_cuboid_uv("羊", "腿部", 0, 34, 4, 6, 4, 64, 64)))
check_overlap("羊", sheep_regions)

print("\n=== 鸡 (64x32, 未修改) ===")
chicken_regions = []
chicken_regions.append(("头部", check_cuboid_uv("鸡", "头部", 0, 0, 4, 6, 3, 64, 32)))
chicken_regions.append(("身体", check_cuboid_uv("鸡", "身体", 0, 9, 6, 6, 6, 64, 32)))
chicken_regions.append(("腿部", check_cuboid_uv("鸡", "腿部", 26, 0, 3, 5, 3, 64, 32)))
check_overlap("鸡", chicken_regions)
