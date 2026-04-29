#!/usr/bin/env python3
"""验证怪物贴图的UV区域"""
from PIL import Image

def check(img, name, uvX, uvY, sx, sy, sz):
    w, h = img.size
    total_w = 2 * (sz + sx)
    total_h = sz + sy
    opaque = 0; total = 0; oob = 0
    for dy in range(total_h):
        for dx in range(total_w):
            px, py = uvX + dx, uvY + dy
            if px >= w or py >= h: oob += 1; continue
            total += 1
            if img.getpixel((px, py))[3] > 0: opaque += 1
    pct = (opaque / total * 100) if total > 0 else 0
    status = "OK" if pct > 30 else "EMPTY"
    print(f"  {name}: UV({uvX},{uvY}) size({sx},{sy},{sz}) -> {total_w}x{total_h} | {opaque}/{total} ({pct:.0f}%) OOB={oob} [{status}]")

V = "/Users/yukirazhang/work/mycraft/assets/minecraft_vanilla/textures/entity"

# 僵尸 64x64 - MC标准人形模型
z = Image.open(f"{V}/zombie/zombie.png").convert("RGBA")
print(f"Zombie ({z.size[0]}x{z.size[1]}):")
check(z, "head(8,8,8)@(0,0)", 0, 0, 8, 8, 8)
check(z, "body(8,12,4)@(16,16)", 16, 16, 8, 12, 4)
check(z, "leg_L(4,12,4)@(0,16)", 0, 16, 4, 12, 4)
check(z, "leg_R(4,12,4)@(0,16)", 0, 16, 4, 12, 4)  # 原版左右腿共用UV
check(z, "arm_L(4,12,4)@(40,16)", 40, 16, 4, 12, 4)
check(z, "arm_R(4,12,4)@(32,48)", 32, 48, 4, 12, 4)  # 1.8+右臂独立UV

# 骷髅 64x32
s = Image.open(f"{V}/skeleton/skeleton.png").convert("RGBA")
print(f"\nSkeleton ({s.size[0]}x{s.size[1]}):")
check(s, "head(8,8,8)@(0,0)", 0, 0, 8, 8, 8)
check(s, "body(8,12,4)@(16,16)", 16, 16, 8, 12, 4)
check(s, "leg(2,12,2)@(0,16)", 0, 16, 2, 12, 2)
check(s, "arm(2,12,2)@(40,16)", 40, 16, 2, 12, 2)

# 蜘蛛 64x32
sp = Image.open(f"{V}/spider/spider.png").convert("RGBA")
print(f"\nSpider ({sp.size[0]}x{sp.size[1]}):")
check(sp, "head(8,8,8)@(32,4)", 32, 4, 8, 8, 8)
check(sp, "body(10,8,12)@(0,0)", 0, 0, 10, 8, 12)  # 蜘蛛身体 - 试试不同参数
# MC蜘蛛腿: 16x2x2 UV(18,0)
check(sp, "leg(2,12,2)@(18,0)", 18, 0, 2, 12, 2)

# 苦力怕 64x32
c = Image.open(f"{V}/creeper/creeper.png").convert("RGBA")
print(f"\nCreeper ({c.size[0]}x{c.size[1]}):")
check(c, "head(8,8,8)@(0,0)", 0, 0, 8, 8, 8)
check(c, "body(8,12,4)@(16,16)", 16, 16, 8, 12, 4)
check(c, "leg(4,6,4)@(0,16)", 0, 16, 4, 6, 4)
