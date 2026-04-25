#!/usr/bin/env python3
"""Generate all tool + HUD icon PNGs from the same pixel data that was in texture_atlas.cpp.
Output dirs: assets/textures/items/ and assets/textures/hud/
Each PNG is 16x16 RGBA."""
import struct, zlib, os

def write_png(path, pixels, w, h):
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

T = 16  # tile size

def blit_tool(name, pattern, headRGB, handleRGB):
    pixels = [(0,0,0,0)] * (T*T)
    patH, patW = len(pattern), len(pattern[0])
    scale = max(1, T // patH)
    drawW, drawH = patW * scale, patH * scale
    ox = (T - drawW) // 2
    oy = (T - drawH) // 2
    for gy in range(patH):
        for gx in range(patW):
            v = pattern[gy][gx]
            if v == 0: continue
            r, g, b = handleRGB if v == 1 else headRGB
            for sy in range(scale):
                for sx in range(scale):
                    px, py = ox + gx*scale + sx, oy + gy*scale + sy
                    if 0 <= px < T and 0 <= py < T:
                        pixels[py*T+px] = (r, g, b, 255)
    return pixels

def blit_hud(name, pattern, fillRGB, outlineRGB):
    pixels = [(0,0,0,0)] * (T*T)
    patH, patW = len(pattern), len(pattern[0])
    scale = max(1, T // patH)
    drawW, drawH = patW * scale, patH * scale
    ox = (T - drawW) // 2
    oy = (T - drawH) // 2
    for gy in range(patH):
        for gx in range(patW):
            v = pattern[gy][gx]
            if v == 0: continue
            if v == 1: r,g,b = fillRGB
            elif v == 2: r,g,b = outlineRGB
            else: r,g,b = (50,40,35)
            for sy in range(scale):
                for sx in range(scale):
                    px, py = ox + gx*scale + sx, oy + gy*scale + sy
                    if 0 <= px < T and 0 <= py < T:
                        pixels[py*T+px] = (r, g, b, 255)
    return pixels

def blit_digit(digit):
    glyphs = [
        [0,1,1,1,0, 1,0,0,0,1, 1,0,0,1,1, 1,0,1,0,1, 1,1,0,0,1, 1,0,0,0,1, 0,1,1,1,0],
        [0,0,1,0,0, 0,1,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,1,1,1,0],
        [0,1,1,1,0, 1,0,0,0,1, 0,0,0,0,1, 0,0,0,1,0, 0,0,1,0,0, 0,1,0,0,0, 1,1,1,1,1],
        [1,1,1,1,0, 0,0,0,0,1, 0,0,0,0,1, 0,1,1,1,0, 0,0,0,0,1, 0,0,0,0,1, 1,1,1,1,0],
        [0,0,0,1,0, 0,0,1,1,0, 0,1,0,1,0, 1,0,0,1,0, 1,1,1,1,1, 0,0,0,1,0, 0,0,0,1,0],
        [1,1,1,1,1, 1,0,0,0,0, 1,1,1,1,0, 0,0,0,0,1, 0,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0],
        [0,0,1,1,0, 0,1,0,0,0, 1,0,0,0,0, 1,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0],
        [1,1,1,1,1, 0,0,0,0,1, 0,0,0,1,0, 0,0,1,0,0, 0,1,0,0,0, 0,1,0,0,0, 0,1,0,0,0],
        [0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0],
        [0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,1, 0,0,0,0,1, 0,0,0,1,0, 0,1,1,0,0],
    ]
    glyph = glyphs[digit]
    gW, gH = 5, 7
    pixels = [(0,0,0,0)] * (T*T)
    scale = max(1, T // 8)
    drawW, drawH = gW*scale, gH*scale
    ox = (T - drawW) // 2
    oy = (T - drawH) // 2
    for gy in range(gH):
        for gx in range(gW):
            if not glyph[gy*gW+gx]: continue
            for sy in range(scale):
                for sx in range(scale):
                    px, py = ox+gx*scale+sx, oy+gy*scale+sy
                    if 0 <= px < T and 0 <= py < T:
                        pixels[py*T+px] = (255,255,255,255)
    return pixels

BASE = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures')
ITEMS = os.path.join(BASE, 'items')
HUD = os.path.join(BASE, 'hud')

# Tool patterns (same as was in texture_atlas.cpp)
PICK = [[2,2,2,2,2,2,2],[2,2,0,0,0,2,2],[0,0,0,0,2,0,0],[0,0,0,0,2,0,0],[0,0,0,1,0,0,0],[0,0,0,1,0,0,0],[0,0,1,0,0,0,0],[0,0,1,0,0,0,0],[0,1,0,0,0,0,0],[0,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]
AXE  = [[0,0,0,0,2,2,0],[0,0,0,2,2,2,0],[0,0,0,2,2,2,2],[0,0,0,2,2,2,0],[0,0,0,0,2,0,0],[0,0,0,1,0,0,0],[0,0,1,0,0,0,0],[0,0,1,0,0,0,0],[0,1,0,0,0,0,0],[0,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]
SHOV = [[0,0,0,0,2,2,0],[0,0,0,2,2,2,0],[0,0,0,2,2,2,0],[0,0,0,0,2,0,0],[0,0,0,1,0,0,0],[0,0,0,1,0,0,0],[0,0,1,0,0,0,0],[0,0,1,0,0,0,0],[0,1,0,0,0,0,0],[0,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]
SWD  = [[0,0,0,0,0,0,2],[0,0,0,0,0,2,2],[0,0,0,0,2,2,0],[0,0,0,0,2,2,0],[0,0,0,2,2,0,0],[0,0,0,2,0,0,0],[0,0,2,0,0,0,0],[0,1,1,0,0,0,0],[1,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]
HOE  = [[0,0,0,2,2,2,0],[0,0,0,0,0,2,0],[0,0,0,0,2,0,0],[0,0,0,1,0,0,0],[0,0,0,1,0,0,0],[0,0,1,0,0,0,0],[0,0,1,0,0,0,0],[0,1,0,0,0,0,0],[0,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]

HANDLE = (110,75,40)
tools = [
    ("tool_wooden_pickaxe", PICK, (140,100,55)),
    ("tool_wooden_axe",     AXE,  (140,100,55)),
    ("tool_wooden_shovel",  SHOV, (140,100,55)),
    ("tool_wooden_sword",   SWD,  (140,100,55)),
    ("tool_wooden_hoe",     HOE,  (140,100,55)),
    ("tool_stone_pickaxe",  PICK, (140,140,140)),
    ("tool_stone_axe",      AXE,  (140,140,140)),
    ("tool_stone_shovel",   SHOV, (140,140,140)),
    ("tool_stone_sword",    SWD,  (140,140,140)),
    ("tool_stone_hoe",      HOE,  (140,140,140)),
]

for name, pat, head in tools:
    px = blit_tool(name, pat, head, HANDLE)
    write_png(os.path.join(ITEMS, name + '.png'), px, T, T)

# HUD icons
hud_icons = [
    ("hud_heart_full",
     [[0,2,2,0,0,0,2,2,0],[2,1,1,2,0,2,1,1,2],[2,1,1,1,2,1,1,1,2],[2,1,1,1,1,1,1,1,2],[0,2,1,1,1,1,1,2,0],[0,0,2,1,1,1,2,0,0],[0,0,0,2,1,2,0,0,0],[0,0,0,0,2,0,0,0,0],[0,0,0,0,0,0,0,0,0]],
     (220,30,30), (80,10,10)),
    ("hud_heart_half",
     [[0,2,2,0,0,0,2,2,0],[2,1,1,2,0,2,3,3,2],[2,1,1,1,2,3,3,3,2],[2,1,1,1,3,3,3,3,2],[0,2,1,1,3,3,3,2,0],[0,0,2,1,3,3,2,0,0],[0,0,0,2,3,2,0,0,0],[0,0,0,0,2,0,0,0,0],[0,0,0,0,0,0,0,0,0]],
     (220,30,30), (80,10,10)),
    ("hud_heart_empty",
     [[0,2,2,0,0,0,2,2,0],[2,0,0,2,0,2,0,0,2],[2,0,0,0,2,0,0,0,2],[2,0,0,0,0,0,0,0,2],[0,2,0,0,0,0,0,2,0],[0,0,2,0,0,0,2,0,0],[0,0,0,2,0,2,0,0,0],[0,0,0,0,2,0,0,0,0],[0,0,0,0,0,0,0,0,0]],
     (220,30,30), (80,10,10)),
    ("hud_drumstick_full",
     [[0,0,0,0,0,0,2,2,0],[0,0,0,0,0,2,1,1,2],[0,0,0,0,0,2,1,1,2],[0,0,0,0,2,1,1,2,0],[0,0,0,2,1,1,2,0,0],[0,0,2,1,2,2,0,0,0],[0,2,1,2,0,0,0,0,0],[0,2,2,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0]],
     (200,150,50), (90,60,20)),
    ("hud_drumstick_half",
     [[0,0,0,0,0,0,2,2,0],[0,0,0,0,0,2,1,1,2],[0,0,0,0,0,2,1,1,2],[0,0,0,0,2,1,1,2,0],[0,0,0,2,3,3,2,0,0],[0,0,2,3,2,2,0,0,0],[0,2,3,2,0,0,0,0,0],[0,2,2,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0]],
     (200,150,50), (90,60,20)),
    ("hud_drumstick_empty",
     [[0,0,0,0,0,0,2,2,0],[0,0,0,0,0,2,0,0,2],[0,0,0,0,0,2,0,0,2],[0,0,0,0,2,0,0,2,0],[0,0,0,2,0,0,2,0,0],[0,0,2,0,2,2,0,0,0],[0,2,0,2,0,0,0,0,0],[0,2,2,0,0,0,0,0,0],[0,0,0,0,0,0,0,0,0]],
     (200,150,50), (90,60,20)),
    ("item_apple",
     [[0,0,0,2,2,0,0,0,0],[0,0,0,0,2,0,0,0,0],[0,0,2,1,1,1,2,0,0],[0,2,1,1,1,1,1,2,0],[0,2,1,1,1,1,1,2,0],[0,2,1,1,1,1,1,2,0],[0,0,2,1,1,1,2,0,0],[0,0,0,2,2,2,0,0,0],[0,0,0,0,0,0,0,0,0]],
     (200,30,30), (40,120,40)),
]

for name, pat, fill, outline in hud_icons:
    px = blit_hud(name, pat, fill, outline)
    write_png(os.path.join(HUD, name + '.png'), px, T, T)

# Font digits
FONT = os.path.join(BASE, 'font')
for d in range(10):
    px = blit_digit(d)
    write_png(os.path.join(FONT, f'font_digit_{d}.png'), px, T, T)

print(f"Generated {len(tools)} tool + {len(hud_icons)} hud + 10 font PNGs")
