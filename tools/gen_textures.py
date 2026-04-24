#!/usr/bin/env python3
"""Generate 16x16 Minecraft-style block textures as PNG files."""
import struct, zlib, os, random

def write_png(path, pixels, w, h):
    """Write RGBA pixels to PNG (no dependency needed)."""
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    
    raw = b''
    for y in range(h):
        raw += b'\x00'  # filter: none
        for x in range(w):
            i = (y * w + x) * 4
            raw += bytes(pixels[i:i+4])
    
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))

def fill(pixels, w, r, g, b, a=255, noise=10):
    """Fill with color + random noise."""
    for i in range(w * w):
        n = random.randint(-noise, noise)
        pixels[i*4+0] = max(0, min(255, r + n))
        pixels[i*4+1] = max(0, min(255, g + n))
        pixels[i*4+2] = max(0, min(255, b + n))
        pixels[i*4+3] = a

def gen_grass_top(s=16):
    px = [0]*(s*s*4)
    for i in range(s*s):
        n = random.randint(-15, 15)
        g = random.choice([140, 150, 160, 130]) + n
        px[i*4+0] = max(0, min(255, 90 + n//2))
        px[i*4+1] = max(0, min(255, g))
        px[i*4+2] = max(0, min(255, 40 + n//3))
        px[i*4+3] = 255
    return px

def gen_grass_side(s=16):
    px = [0]*(s*s*4)
    for y in range(s):
        for x in range(s):
            i = (y*s+x)*4
            n = random.randint(-8, 8)
            if y < 4:  # top part green
                px[i+0] = max(0, min(255, 90 + n))
                px[i+1] = max(0, min(255, 145 + n))
                px[i+2] = max(0, min(255, 40 + n))
            else:  # dirt part
                px[i+0] = max(0, min(255, 134 + n))
                px[i+1] = max(0, min(255, 96 + n))
                px[i+2] = max(0, min(255, 67 + n))
            px[i+3] = 255
    return px

def gen_dirt(s=16):
    px = [0]*(s*s*4)
    for i in range(s*s):
        n = random.randint(-12, 12)
        px[i*4+0] = max(0, min(255, 134 + n))
        px[i*4+1] = max(0, min(255, 96 + n))
        px[i*4+2] = max(0, min(255, 67 + n))
        px[i*4+3] = 255
    return px

def gen_stone(s=16):
    px = [0]*(s*s*4)
    for i in range(s*s):
        n = random.randint(-20, 20)
        v = random.choice([125, 130, 120, 128]) + n
        px[i*4+0] = max(0, min(255, v))
        px[i*4+1] = max(0, min(255, v))
        px[i*4+2] = max(0, min(255, v))
        px[i*4+3] = 255
    return px

def gen_sand(s=16):
    px = [0]*(s*s*4)
    for i in range(s*s):
        n = random.randint(-10, 10)
        px[i*4+0] = max(0, min(255, 219 + n))
        px[i*4+1] = max(0, min(255, 211 + n))
        px[i*4+2] = max(0, min(255, 160 + n))
        px[i*4+3] = 255
    return px

def gen_oak_log_side(s=16):
    px = [0]*(s*s*4)
    for y in range(s):
        for x in range(s):
            i = (y*s+x)*4
            n = random.randint(-8, 8)
            # Vertical bark lines
            if x % 4 == 0:
                px[i+0] = max(0, min(255, 80 + n))
                px[i+1] = max(0, min(255, 56 + n))
                px[i+2] = max(0, min(255, 30 + n))
            else:
                px[i+0] = max(0, min(255, 109 + n))
                px[i+1] = max(0, min(255, 85 + n))
                px[i+2] = max(0, min(255, 51 + n))
            px[i+3] = 255
    return px

def gen_oak_log_top(s=16):
    px = [0]*(s*s*4)
    cx, cy = s//2, s//2
    for y in range(s):
        for x in range(s):
            i = (y*s+x)*4
            n = random.randint(-8, 8)
            d = ((x-cx)**2 + (y-cy)**2)**0.5
            if d < 3:  # inner ring
                px[i+0] = max(0, min(255, 160 + n))
                px[i+1] = max(0, min(255, 130 + n))
                px[i+2] = max(0, min(255, 70 + n))
            elif d < 6:
                px[i+0] = max(0, min(255, 140 + n))
                px[i+1] = max(0, min(255, 110 + n))
                px[i+2] = max(0, min(255, 60 + n))
            else:  # bark
                px[i+0] = max(0, min(255, 109 + n))
                px[i+1] = max(0, min(255, 85 + n))
                px[i+2] = max(0, min(255, 51 + n))
            px[i+3] = 255
    return px

def gen_leaves(s=16):
    px = [0]*(s*s*4)
    for i in range(s*s):
        n = random.randint(-20, 20)
        if random.random() < 0.15:  # gaps
            px[i*4+0] = 30 + random.randint(0,20)
            px[i*4+1] = 80 + random.randint(0,20)
            px[i*4+2] = 20 + random.randint(0,10)
            px[i*4+3] = 180
        else:
            px[i*4+0] = max(0, min(255, 55 + n))
            px[i*4+1] = max(0, min(255, 120 + n))
            px[i*4+2] = max(0, min(255, 35 + n))
            px[i*4+3] = 255
    return px

def gen_water(s=16):
    px = [0]*(s*s*4)
    for i in range(s*s):
        n = random.randint(-10, 10)
        px[i*4+0] = max(0, min(255, 30 + n))
        px[i*4+1] = max(0, min(255, 60 + n))
        px[i*4+2] = max(0, min(255, 170 + n))
        px[i*4+3] = 160  # semi-transparent
    return px

def gen_solid(s, r, g, b, noise=15, a=255):
    px = [0]*(s*s*4)
    fill(px, s, r, g, b, a, noise)
    return px

def gen_cobblestone(s=16):
    px = [0]*(s*s*4)
    for y in range(s):
        for x in range(s):
            i = (y*s+x)*4
            n = random.randint(-15, 15)
            # Irregular stone pattern
            v = 120 + ((x*7 + y*13) % 30) - 15 + n
            px[i+0] = max(0, min(255, v))
            px[i+1] = max(0, min(255, v - 3))
            px[i+2] = max(0, min(255, v - 3))
            px[i+3] = 255
    return px

def gen_bedrock(s=16):
    px = [0]*(s*s*4)
    for i in range(s*s):
        n = random.randint(-25, 25)
        v = random.choice([50, 55, 60, 45, 70]) + n
        px[i*4+0] = max(0, min(255, v))
        px[i*4+1] = max(0, min(255, v))
        px[i*4+2] = max(0, min(255, v))
        px[i*4+3] = 255
    return px

def gen_gravel(s=16):
    px = [0]*(s*s*4)
    for i in range(s*s):
        n = random.randint(-15, 15)
        v = random.choice([120, 130, 110, 140]) + n
        px[i*4+0] = max(0, min(255, v))
        px[i*4+1] = max(0, min(255, v - 5))
        px[i*4+2] = max(0, min(255, v - 8))
        px[i*4+3] = 255
    return px

def gen_oak_planks(s=16):
    px = [0]*(s*s*4)
    for y in range(s):
        for x in range(s):
            i = (y*s+x)*4
            n = random.randint(-8, 8)
            # Horizontal plank lines
            if y % 4 == 0:
                px[i+0] = max(0, min(255, 130 + n))
                px[i+1] = max(0, min(255, 100 + n))
                px[i+2] = max(0, min(255, 55 + n))
            else:
                px[i+0] = max(0, min(255, 162 + n))
                px[i+1] = max(0, min(255, 130 + n))
                px[i+2] = max(0, min(255, 78 + n))
            px[i+3] = 255
    return px

if __name__ == '__main__':
    random.seed(42)
    out = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures', 'blocks')
    os.makedirs(out, exist_ok=True)
    
    textures = {
        'grass_top': gen_grass_top,
        'grass_side': gen_grass_side,
        'dirt': gen_dirt,
        'stone': gen_stone,
        'sand': gen_sand,
        'oak_log_side': gen_oak_log_side,
        'oak_log_top': gen_oak_log_top,
        'oak_leaves': gen_leaves,
        'water_still': gen_water,
        'cobblestone': gen_cobblestone,
        'oak_planks': gen_oak_planks,
        'bedrock': gen_bedrock,
        'gravel': gen_gravel,
    }
    
    for name, gen_fn in textures.items():
        px = gen_fn()
        path = os.path.join(out, f'{name}.png')
        write_png(path, px, 16, 16)
        print(f'  Generated {path}')
    
    print(f'\nDone: {len(textures)} textures generated.')
