#!/usr/bin/env python3
"""Generate iron/gold/diamond tool icons + furnace block textures."""
import struct, zlib, os, random

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

T = 16
BASE = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures')
ITEMS = os.path.join(BASE, 'items')
BLOCKS = os.path.join(BASE, 'blocks')
HANDLE = (110, 75, 40)

PICK = [[2,2,2,2,2,2,2],[2,2,0,0,0,2,2],[0,0,0,0,2,0,0],[0,0,0,0,2,0,0],[0,0,0,1,0,0,0],[0,0,0,1,0,0,0],[0,0,1,0,0,0,0],[0,0,1,0,0,0,0],[0,1,0,0,0,0,0],[0,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]
AXE  = [[0,0,0,0,2,2,0],[0,0,0,2,2,2,0],[0,0,0,2,2,2,2],[0,0,0,2,2,2,0],[0,0,0,0,2,0,0],[0,0,0,1,0,0,0],[0,0,1,0,0,0,0],[0,0,1,0,0,0,0],[0,1,0,0,0,0,0],[0,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]
SHOV = [[0,0,0,0,2,2,0],[0,0,0,2,2,2,0],[0,0,0,2,2,2,0],[0,0,0,0,2,0,0],[0,0,0,1,0,0,0],[0,0,0,1,0,0,0],[0,0,1,0,0,0,0],[0,0,1,0,0,0,0],[0,1,0,0,0,0,0],[0,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]
SWD  = [[0,0,0,0,0,0,2],[0,0,0,0,0,2,2],[0,0,0,0,2,2,0],[0,0,0,0,2,2,0],[0,0,0,2,2,0,0],[0,0,0,2,0,0,0],[0,0,2,0,0,0,0],[0,1,1,0,0,0,0],[1,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]
HOE  = [[0,0,0,2,2,2,0],[0,0,0,0,0,2,0],[0,0,0,0,2,0,0],[0,0,0,1,0,0,0],[0,0,0,1,0,0,0],[0,0,1,0,0,0,0],[0,0,1,0,0,0,0],[0,1,0,0,0,0,0],[0,1,0,0,0,0,0],[1,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0],[0,0,0,0,0,0,0]]

def blit_tool(pattern, headRGB):
    pixels = [(0,0,0,0)] * (T*T)
    patH, patW = len(pattern), len(pattern[0])
    scale = max(1, T // patH)
    ox = (T - patW * scale) // 2
    oy = (T - patH * scale) // 2
    for gy in range(patH):
        for gx in range(patW):
            v = pattern[gy][gx]
            if v == 0: continue
            r, g, b = HANDLE if v == 1 else headRGB
            for sy in range(scale):
                for sx in range(scale):
                    px, py = ox + gx*scale + sx, oy + gy*scale + sy
                    if 0 <= px < T and 0 <= py < T:
                        pixels[py*T+px] = (r, g, b, 255)
    return pixels

tools = [
    ("tool_iron_pickaxe",    PICK, (210,210,210)),
    ("tool_iron_axe",        AXE,  (210,210,210)),
    ("tool_iron_shovel",     SHOV, (210,210,210)),
    ("tool_iron_sword",      SWD,  (210,210,210)),
    ("tool_iron_hoe",        HOE,  (210,210,210)),
    ("tool_gold_pickaxe",    PICK, (255,220,50)),
    ("tool_gold_axe",        AXE,  (255,220,50)),
    ("tool_gold_shovel",     SHOV, (255,220,50)),
    ("tool_gold_sword",      SWD,  (255,220,50)),
    ("tool_gold_hoe",        HOE,  (255,220,50)),
    ("tool_diamond_pickaxe", PICK, (100,230,230)),
    ("tool_diamond_axe",     AXE,  (100,230,230)),
    ("tool_diamond_shovel",  SHOV, (100,230,230)),
    ("tool_diamond_sword",   SWD,  (100,230,230)),
    ("tool_diamond_hoe",     HOE,  (100,230,230)),
]

for name, pat, head in tools:
    px = blit_tool(pat, head)
    write_png(os.path.join(ITEMS, name + '.png'), px, T, T)
print(f"Generated {len(tools)} tool icons")

# Furnace textures
rng = random.Random(300)
def cobble_base():
    px = []
    for y in range(T):
        for x in range(T):
            v = 110 + rng.randint(-15, 15)
            px.append((v, v, v, 255))
    return px

# furnace_front: cobblestone with dark opening in center
front = cobble_base()
for y in range(5, 13):
    for x in range(4, 12):
        if y < 7:
            front[y*T+x] = (60, 60, 60, 255)
        else:
            front[y*T+x] = (30, 20, 15, 255)
write_png(os.path.join(BLOCKS, 'furnace_front.png'), front, T, T)

# furnace_side: plain cobblestone-like
side = cobble_base()
write_png(os.path.join(BLOCKS, 'furnace_side.png'), side, T, T)

# furnace_top: slightly different cobblestone
top = cobble_base()
write_png(os.path.join(BLOCKS, 'furnace_top.png'), top, T, T)

print("Generated furnace textures")
