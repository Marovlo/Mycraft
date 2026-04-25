#!/usr/bin/env python3
"""Generate crafting_table_top.png and crafting_table_side.png (16x16 pixel art)."""
import struct, zlib, os

def write_png(path, pixels, w, h):
    """Write a minimal RGBA PNG file."""
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    raw = b''
    for y in range(h):
        raw += b'\x00'  # filter none
        for x in range(w):
            r, g, b, a = pixels[y * w + x]
            raw += struct.pack('BBBB', r, g, b, a)
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)  # 8bit RGBA
    out = sig + chunk(b'IHDR', ihdr) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(out)

# Colors
PLANK       = (180, 140, 90, 255)   # oak planks base
PLANK_DARK  = (150, 115, 70, 255)   # darker plank shade
GRID_LINE   = (100, 75, 50, 255)    # dark grid lines on top
GRID_LIGHT  = (200, 160, 110, 255)  # lighter area
SIDE_DARK   = (130, 100, 65, 255)   # side darker band
SIDE_TOOL   = (90, 70, 50, 255)     # side tool marking

# --- Top texture: oak plank base with 3x3 grid lines ---
top = [PLANK] * 256
# Fill with subtle plank pattern
for y in range(16):
    for x in range(16):
        if (x + y) % 4 == 0:
            top[y * 16 + x] = PLANK_DARK
        elif (x * 3 + y) % 7 == 0:
            top[y * 16 + x] = GRID_LIGHT

# Draw grid lines (3x3 crafting grid appearance)
for i in range(16):
    # Vertical lines at x=0, 5, 10, 15
    for lx in [0, 5, 10, 15]:
        top[i * 16 + lx] = GRID_LINE
    # Horizontal lines at y=0, 5, 10, 15
    for ly in [0, 5, 10, 15]:
        top[ly * 16 + i] = GRID_LINE

# Lighter inner cells
for cy in range(3):
    for cx in range(3):
        sx = cx * 5 + 2
        sy = cy * 5 + 2
        for dy in range(-1, 2):
            for dx in range(-1, 2):
                px, py = sx + dx, sy + dy
                if 0 <= px < 16 and 0 <= py < 16:
                    top[py * 16 + px] = GRID_LIGHT

outdir = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures', 'blocks')

write_png(os.path.join(outdir, 'crafting_table_top.png'), top, 16, 16)

# --- Side texture: oak planks with a darker band and subtle markings ---
side = [PLANK] * 256
for y in range(16):
    for x in range(16):
        # Plank grain
        if (x + y * 2) % 5 == 0:
            side[y * 16 + x] = PLANK_DARK
        # Top dark band (tool shelf look)
        if y <= 2:
            side[y * 16 + x] = SIDE_DARK
        # Bottom band
        if y >= 14:
            side[y * 16 + x] = SIDE_DARK
        # Vertical frame edges
        if x == 0 or x == 15:
            side[y * 16 + x] = SIDE_DARK
        # Small tool markings in center
        if 5 <= x <= 10 and 5 <= y <= 10:
            if (x + y) % 3 == 0:
                side[y * 16 + x] = SIDE_TOOL

write_png(os.path.join(outdir, 'crafting_table_side.png'), side, 16, 16)

print("Generated crafting_table_top.png and crafting_table_side.png")
