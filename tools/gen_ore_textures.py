#!/usr/bin/env python3
"""Generate ore block textures and mineral item icon PNGs."""
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
BLOCKS = os.path.join(BASE, 'blocks')
ITEMS = os.path.join(BASE, 'items')

# --- Stone base colors (sample from existing stone texture look) ---
def stone_base():
    """Generate a 16x16 stone-like base."""
    pixels = []
    rng = random.Random(42)
    for y in range(T):
        for x in range(T):
            v = 120 + rng.randint(-15, 15)
            pixels.append((v, v, v, 255))
    return pixels

def add_ore_spots(pixels, color, seed, count=12, size=2):
    """Add colored mineral spots onto a stone base."""
    rng = random.Random(seed)
    for _ in range(count):
        cx, cy = rng.randint(1, T-2), rng.randint(1, T-2)
        for dy in range(-size//2, size//2+1):
            for dx in range(-size//2, size//2+1):
                if rng.random() < 0.6:
                    px, py = cx+dx, cy+dy
                    if 0 <= px < T and 0 <= py < T:
                        r, g, b = color
                        # Slight variation
                        rv = max(0, min(255, r + rng.randint(-20, 20)))
                        gv = max(0, min(255, g + rng.randint(-20, 20)))
                        bv = max(0, min(255, b + rng.randint(-20, 20)))
                        pixels[py * T + px] = (rv, gv, bv, 255)
    return pixels

# Ore textures: name → (spot color RGB, seed, spot_count, spot_size)
ores = {
    'coal_ore':     ((40, 40, 40),     100, 14, 2),
    'iron_ore':     ((200, 150, 120),  101, 10, 2),
    'gold_ore':     ((255, 220, 50),   102, 8,  2),
    'diamond_ore':  ((100, 230, 230),  103, 6,  2),
    'redstone_ore': ((200, 20, 20),    104, 12, 2),
    'lapis_ore':    ((30, 60, 180),    105, 8,  2),
    'emerald_ore':  ((30, 200, 60),    106, 5,  2),
    'copper_ore':   ((190, 120, 70),   107, 10, 2),
}

for name, (color, seed, count, size) in ores.items():
    px = stone_base()
    add_ore_spots(px, color, seed, count, size)
    write_png(os.path.join(BLOCKS, name + '.png'), px, T, T)

print(f"Generated {len(ores)} ore block textures")

# --- Mineral item icons (simple colored shapes on transparent bg) ---
def make_item_icon(color, shape='gem', seed=200):
    """Generate a 16x16 item icon."""
    pixels = [(0,0,0,0)] * (T*T)
    rng = random.Random(seed)
    r, g, b = color

    if shape == 'gem':
        # Diamond-like: centered rhombus
        pts = [(7,2),(11,7),(7,12),(3,7)]  # diamond shape
        for y in range(T):
            for x in range(T):
                # Simple point-in-polygon for convex quad
                inside = True
                for i in range(4):
                    x1,y1 = pts[i]
                    x2,y2 = pts[(i+1)%4]
                    if (x2-x1)*(y-y1) - (y2-y1)*(x-x1) > 0:
                        inside = False; break
                if inside:
                    rv = max(0, min(255, r + rng.randint(-15,15)))
                    gv = max(0, min(255, g + rng.randint(-15,15)))
                    bv = max(0, min(255, b + rng.randint(-15,15)))
                    pixels[y*T+x] = (rv, gv, bv, 255)
    elif shape == 'lump':
        # Raw ore: irregular blob
        for y in range(4, 12):
            for x in range(4, 12):
                dx, dy = x-8, y-8
                if dx*dx + dy*dy < 16 + rng.randint(-4,4):
                    rv = max(0, min(255, r + rng.randint(-20,20)))
                    gv = max(0, min(255, g + rng.randint(-20,20)))
                    bv = max(0, min(255, b + rng.randint(-20,20)))
                    pixels[y*T+x] = (rv, gv, bv, 255)
    elif shape == 'ingot':
        # Ingot: horizontal rectangle with highlight
        for y in range(5, 11):
            for x in range(3, 13):
                bright = 1.0 + (0.2 if y < 7 else -0.1)
                rv = max(0, min(255, int(r * bright)))
                gv = max(0, min(255, int(g * bright)))
                bv = max(0, min(255, int(b * bright)))
                pixels[y*T+x] = (rv, gv, bv, 255)
    elif shape == 'dust':
        # Redstone dust: scattered small dots
        for _ in range(30):
            x, y = rng.randint(3,12), rng.randint(3,12)
            rv = max(0, min(255, r + rng.randint(-30,30)))
            gv = max(0, min(255, g + rng.randint(-10,10)))
            bv = max(0, min(255, b + rng.randint(-10,10)))
            pixels[y*T+x] = (rv, gv, bv, 255)
            for nx,ny in [(x+1,y),(x,y+1),(x-1,y),(x,y-1)]:
                if 0<=nx<T and 0<=ny<T and rng.random()<0.4:
                    pixels[ny*T+nx] = (rv, gv, bv, 255)
    return pixels

items_icons = {
    'item_coal':        ((40, 40, 40),      'lump',  210),
    'item_raw_iron':    ((200, 150, 120),   'lump',  211),
    'item_raw_gold':    ((255, 220, 50),    'lump',  212),
    'item_raw_copper':  ((190, 120, 70),    'lump',  213),
    'item_iron_ingot':  ((210, 210, 210),   'ingot', 214),
    'item_gold_ingot':  ((255, 220, 50),    'ingot', 215),
    'item_copper_ingot':((200, 130, 80),    'ingot', 216),
    'item_diamond':     ((100, 230, 230),   'gem',   217),
    'item_emerald':     ((30, 200, 60),     'gem',   218),
    'item_lapis_lazuli':((30, 60, 180),     'gem',   219),
    'item_redstone':    ((200, 20, 20),     'dust',  220),
}

for name, (color, shape, seed) in items_icons.items():
    px = make_item_icon(color, shape, seed)
    write_png(os.path.join(ITEMS, name + '.png'), px, T, T)

print(f"Generated {len(items_icons)} mineral item icons")
