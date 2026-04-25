#!/usr/bin/env python3
"""Generate bubble icons matching heart/drumstick 9x9 pattern style."""
import struct, zlib, os

def write_png(path, px, w, h):
    def chunk(ct, d):
        c = ct + d
        return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    raw = b''
    for y in range(h):
        raw += b'\x00'
        for x in range(w):
            r, g, b, a = px[y * w + x]
            raw += struct.pack('BBBB', r, g, b, a)
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)
    with open(path, 'wb') as f:
        f.write(sig + chunk(b'IHDR', ihdr) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))

# Same 9x9 grid as hearts/drumsticks. 0=transparent, 1=fill, 2=outline
# Bubble shape: circle filling most of the 9x9 grid
bubble_pattern = [
    [0,0,2,2,2,2,2,0,0],
    [0,2,2,1,1,1,2,2,0],
    [2,2,1,1,1,1,1,2,2],
    [2,1,1,1,1,1,1,1,2],
    [2,1,1,1,1,1,1,1,2],
    [2,1,1,1,1,1,1,1,2],
    [2,2,1,1,1,1,1,2,2],
    [0,2,2,1,1,1,2,2,0],
    [0,0,2,2,2,2,2,0,0],
]

HUD = os.path.join(os.path.dirname(__file__), '..', 'assets', 'textures', 'hud')

# Full bubble: blue fill + darker outline + white highlight
full = [(0,0,0,0)] * 81
for y in range(9):
    for x in range(9):
        v = bubble_pattern[y][x]
        if v == 2:
            full[y*9+x] = (30, 70, 170, 255)    # outline
        elif v == 1:
            full[y*9+x] = (70, 150, 235, 255)    # fill
# White highlight top-left
full[1*9+3] = (190, 220, 255, 255)
full[1*9+4] = (170, 210, 250, 255)
full[2*9+2] = (170, 210, 250, 255)

write_png(os.path.join(HUD, 'hud_bubble_full.png'), full, 9, 9)

# Empty bubble: just outline, no fill (transparent inside)
empty = [(0,0,0,0)] * 81
for y in range(9):
    for x in range(9):
        v = bubble_pattern[y][x]
        if v == 2:
            empty[y*9+x] = (30, 70, 170, 200)   # outline only

write_png(os.path.join(HUD, 'hud_bubble_empty.png'), empty, 9, 9)

print("Generated bubble icons (9x9, matching heart/drumstick style)")
