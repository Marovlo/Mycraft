#!/usr/bin/env python3
"""验证mob纹理UV布局的一致性、无重叠、无越界"""

import struct
import os
import sys

def read_png_size(filepath):
    """读取PNG文件的宽高"""
    with open(filepath, 'rb') as f:
        f.read(8 + 4 + 4)  # signature + length + IHDR tag
        w = struct.unpack('>I', f.read(4))[0]
        h = struct.unpack('>I', f.read(4))[0]
    return w, h

def check_mob(name, tex_w, tex_h, parts):
    """检查一个mob的所有部件UV布局"""
    ok = True
    rects = []
    for part_name, uvX, uvY, sx, sy, sz in parts:
        ew = 2 * sz + 2 * sx
        eh = sz + sy
        xe = uvX + ew
        ye = uvY + eh
        rects.append((part_name, uvX, uvY, xe, ye))
        print(f"  {part_name}: UV({uvX},{uvY}) size({sx},{sy},{sz}) -> 展开{ew}x{eh} 占用x=[{uvX},{xe}) y=[{uvY},{ye})")
        if xe > tex_w or ye > tex_h:
            print(f"    *** 越界! {xe}>{tex_w} or {ye}>{tex_h}")
            ok = False

    # 检查重叠
    for i in range(len(rects)):
        for j in range(i + 1, len(rects)):
            n1, x1, y1, x1e, y1e = rects[i]
            n2, x2, y2, x2e, y2e = rects[j]
            if x1 < x2e and x1e > x2 and y1 < y2e and y1e > y2:
                print(f"    *** 重叠! {n1} 和 {n2}")
                ok = False
            else:
                print(f"    无重叠: {n1} 和 {n2}")
    return ok

def main():
    base = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures', 'mobs')
    all_ok = True

    # C++中的模型定义（当前代码）
    mobs = {
        'pig': {
            'file': 'pig.png',
            'texSize': (64, 64),
            'parts': [
                ('头部', 0, 0, 8, 8, 8),
                ('身体', 0, 16, 8, 8, 10),
                ('腿',   0, 34, 4, 6, 4),
            ]
        },
        'cow': {
            'file': 'cow.png',
            'texSize': (64, 64),
            'parts': [
                ('头部', 0, 0, 8, 8, 8),
                ('身体', 0, 16, 10, 10, 12),
                ('腿',   0, 38, 4, 12, 4),
            ]
        },
        'sheep': {
            'file': 'sheep.png',
            'texSize': (64, 64),
            'parts': [
                ('头部', 0, 0, 6, 6, 6),
                ('身体', 0, 12, 8, 8, 10),
                ('腿',   0, 30, 4, 6, 4),
            ]
        },
        'chicken': {
            'file': 'chicken.png',
            'texSize': (64, 32),
            'parts': [
                ('头部', 0, 0, 4, 6, 3),
                ('身体', 0, 9, 6, 6, 6),
                ('腿',   26, 0, 3, 5, 3),
            ]
        },
    }

    for name, info in mobs.items():
        tex_w, tex_h = info['texSize']
        print(f"=== {name} (期望 {tex_w}x{tex_h}) ===")

        # 检查纹理文件尺寸
        filepath = os.path.join(base, info['file'])
        if os.path.exists(filepath):
            actual_w, actual_h = read_png_size(filepath)
            if actual_w != tex_w or actual_h != tex_h:
                print(f"  *** 纹理尺寸不匹配! 文件={actual_w}x{actual_h} 期望={tex_w}x{tex_h}")
                all_ok = False
            else:
                print(f"  纹理文件尺寸正确: {actual_w}x{actual_h}")
        else:
            print(f"  *** 纹理文件不存在: {filepath}")
            all_ok = False

        # 检查UV布局
        if not check_mob(name, tex_w, tex_h, info['parts']):
            all_ok = False
        print()

    if all_ok:
        print("✅ 全部验证通过!")
    else:
        print("❌ 存在问题!")
    return 0 if all_ok else 1

if __name__ == '__main__':
    sys.exit(main())
