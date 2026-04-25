#!/usr/bin/env python3
"""Generate vegetation decoration block textures (16x16, transparent background)."""
import struct, zlib, os, random

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
BLOCKS = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures', 'blocks')

def make_plant(stem_color, flower_color, seed, has_flower=True, tall=False):
    rng = random.Random(seed)
    px = [(0,0,0,0)] * (T*T)
    # Stem: 1-2 pixels wide, from bottom to middle
    stem_h = rng.randint(8, 12) if tall else rng.randint(5, 9)
    sx = T // 2
    for y in range(T - 1, T - 1 - stem_h, -1):
        r, g, b = stem_color
        r += rng.randint(-15, 15); g += rng.randint(-15, 15)
        px[y*T+sx] = (max(0,min(255,r)), max(0,min(255,g)), max(0,min(255,b)), 255)
        if rng.random() < 0.4:
            px[y*T+sx-1] = (max(0,min(255,r-10)), max(0,min(255,g-10)), max(0,min(255,b)), 255)
    # Flower head or leaf top
    if has_flower:
        fy = T - 1 - stem_h
        for dy in range(-2, 2):
            for dx in range(-2, 2):
                if abs(dx) + abs(dy) <= 2 and rng.random() < 0.7:
                    nx, ny = sx + dx, fy + dy
                    if 0 <= nx < T and 0 <= ny < T:
                        r, g, b = flower_color
                        r += rng.randint(-20, 20); g += rng.randint(-20, 20)
                        px[ny*T+nx] = (max(0,min(255,r)), max(0,min(255,g)), max(0,min(255,b)), 255)
    return px

def make_grass(seed):
    rng = random.Random(seed)
    px = [(0,0,0,0)] * (T*T)
    for blade in range(5):
        bx = rng.randint(2, 13)
        bh = rng.randint(6, 12)
        for y in range(T - 1, T - 1 - bh, -1):
            r = 50 + rng.randint(-10, 10)
            g = 140 + rng.randint(-20, 20)
            b = 30 + rng.randint(-10, 10)
            bx2 = bx + rng.choice([-1, 0, 0, 1])
            if 0 <= bx2 < T:
                px[y*T+bx2] = (r, g, b, 255)
    return px

def make_mushroom(cap_color, seed):
    rng = random.Random(seed)
    px = [(0,0,0,0)] * (T*T)
    # Stem
    for y in range(9, 15):
        px[y*T+7] = (220, 210, 190, 255)
        px[y*T+8] = (210, 200, 180, 255)
    # Cap
    for y in range(6, 10):
        w = 4 - abs(y - 7)
        for x in range(7 - w, 9 + w):
            if 0 <= x < T:
                r, g, b = cap_color
                r += rng.randint(-15, 15); g += rng.randint(-15, 15)
                px[y*T+x] = (max(0,min(255,r)), max(0,min(255,g)), max(0,min(255,b)), 255)
    return px

textures = {
    'tall_grass':      make_grass(500),
    'poppy':           make_plant((40, 120, 30), (220, 30, 30), 501),
    'dandelion':       make_plant((50, 130, 35), (255, 230, 50), 502),
    'blue_orchid':     make_plant((40, 110, 40), (50, 120, 230), 503),
    'brown_mushroom':  make_mushroom((160, 120, 70), 504),
    'red_mushroom':    make_mushroom((200, 40, 40), 505),
    'dead_bush':       make_plant((140, 110, 60), (140, 110, 60), 506, has_flower=False),
}

for name, pixels in textures.items():
    write_png(os.path.join(BLOCKS, name + '.png'), pixels, T, T)

print(f"Generated {len(textures)} vegetation textures")
