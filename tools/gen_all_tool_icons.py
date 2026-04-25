#!/usr/bin/env python3
"""Regenerate ALL tool icons with bigger heads, thicker handles, double-headed pickaxes."""
import struct, zlib, os

def write_png(path, pixels, w, h):
    def chunk(ct, data):
        c = ct + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    raw = b''
    for y in range(h):
        raw += b'\x00'
        for x in range(w):
            r, g, b, a = pixels[y * w + x]
            raw += struct.pack('BBBB', r, g, b, a)
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)
    with open(path, 'wb') as f:
        f.write(sig + chunk(b'IHDR', ihdr) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))

T = 16
ITEMS = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures', 'items')
HANDLE = (110, 75, 40)

# Bigger patterns: 9×14 grid, 0=transparent, 1=handle, 2=head
# Pickaxe: double-headed, wide T shape
PICK = [
    [0,2,2,2,2,2,2,2,0],
    [0,2,2,2,2,2,2,2,0],
    [2,2,0,0,2,0,0,2,2],
    [0,0,0,0,2,0,0,0,0],
    [0,0,0,1,1,0,0,0,0],
    [0,0,0,1,1,0,0,0,0],
    [0,0,1,1,0,0,0,0,0],
    [0,0,1,1,0,0,0,0,0],
    [0,1,1,0,0,0,0,0,0],
    [0,1,1,0,0,0,0,0,0],
    [1,1,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
]
# Axe: bigger chunky head
AXE = [
    [0,0,0,0,2,2,2,0,0],
    [0,0,0,2,2,2,2,0,0],
    [0,0,0,2,2,2,2,2,0],
    [0,0,0,2,2,2,2,2,0],
    [0,0,0,2,2,2,2,0,0],
    [0,0,0,0,2,2,0,0,0],
    [0,0,0,1,1,0,0,0,0],
    [0,0,1,1,0,0,0,0,0],
    [0,0,1,1,0,0,0,0,0],
    [0,1,1,0,0,0,0,0,0],
    [0,1,1,0,0,0,0,0,0],
    [1,1,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
]
# Shovel: bigger rounded head
SHOV = [
    [0,0,0,0,2,2,2,0,0],
    [0,0,0,2,2,2,2,0,0],
    [0,0,0,2,2,2,2,0,0],
    [0,0,0,2,2,2,2,0,0],
    [0,0,0,0,2,2,0,0,0],
    [0,0,0,1,1,0,0,0,0],
    [0,0,0,1,1,0,0,0,0],
    [0,0,1,1,0,0,0,0,0],
    [0,0,1,1,0,0,0,0,0],
    [0,1,1,0,0,0,0,0,0],
    [0,1,1,0,0,0,0,0,0],
    [1,1,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
]
# Sword: wider blade
SWD = [
    [0,0,0,0,0,0,0,2,0],
    [0,0,0,0,0,0,2,2,0],
    [0,0,0,0,0,2,2,2,0],
    [0,0,0,0,2,2,2,0,0],
    [0,0,0,0,2,2,0,0,0],
    [0,0,0,2,2,0,0,0,0],
    [0,0,0,2,0,0,0,0,0],
    [0,0,2,0,0,0,0,0,0],
    [0,1,1,1,0,0,0,0,0],
    [1,1,1,0,0,0,0,0,0],
    [1,1,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
]
# Hoe: bigger L head
HOE = [
    [0,0,0,2,2,2,2,0,0],
    [0,0,0,0,0,2,2,0,0],
    [0,0,0,0,2,2,0,0,0],
    [0,0,0,0,2,0,0,0,0],
    [0,0,0,1,1,0,0,0,0],
    [0,0,0,1,1,0,0,0,0],
    [0,0,1,1,0,0,0,0,0],
    [0,0,1,1,0,0,0,0,0],
    [0,1,1,0,0,0,0,0,0],
    [0,1,1,0,0,0,0,0,0],
    [1,1,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
    [0,0,0,0,0,0,0,0,0],
]

def blit(pattern, headRGB):
    pixels = [(0,0,0,0)] * (T*T)
    patH = len(pattern)
    patW = len(pattern[0])
    # No scaling — 9×14 fits into 16×16 with 1px margin
    ox = (T - patW) // 2
    oy = (T - patH) // 2
    for gy in range(patH):
        for gx in range(patW):
            v = pattern[gy][gx]
            if v == 0: continue
            r, g, b = HANDLE if v == 1 else headRGB
            px, py = ox + gx, oy + gy
            if 0 <= px < T and 0 <= py < T:
                pixels[py*T+px] = (r, g, b, 255)
    return pixels

materials = [
    ("wooden",  (140,100,55)),
    ("stone",   (140,140,140)),
    ("iron",    (210,210,210)),
    ("gold",    (255,220,50)),
    ("diamond", (100,230,230)),
]

tool_patterns = [
    ("pickaxe", PICK),
    ("axe",     AXE),
    ("shovel",  SHOV),
    ("sword",   SWD),
    ("hoe",     HOE),
]

count = 0
for mat_name, mat_color in materials:
    for tool_name, pattern in tool_patterns:
        name = f"tool_{mat_name}_{tool_name}"
        px = blit(pattern, mat_color)
        write_png(os.path.join(ITEMS, name + '.png'), px, T, T)
        count += 1

print(f"Generated {count} tool icons")
