#!/usr/bin/env python3
import struct, zlib, os, random, math

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
CX, CY = T / 2.0, T / 2.0
MAX_R = 10.5

rng = random.Random(31415)
pixels = [(0, 0, 0, 0)] * (T * T)

# 2 cracks per stage × 10 stages = 20 cracks total.
# First 12 use evenly spaced angles (every 30°) to guarantee full coverage,
# remaining 8 use random angles for fill.
fixed_angles = [i * math.pi / 6 for i in range(12)]  # 0°,30°,60°,...,330°
rng.shuffle(fixed_angles)
random_angles = [rng.uniform(0, 2 * math.pi) for _ in range(8)]
all_angles = fixed_angles + random_angles  # 20 angles for 20 cracks

crack_idx = 0
for stage in range(10):
    radius = 1.5 + (MAX_R - 1.5) * (stage / 9.0)

    for _ in range(2):  # 2 cracks per stage
        angle = all_angles[crack_idx] + rng.uniform(-0.25, 0.25)
        crack_idx += 1

        start_r = rng.uniform(0, 1.5)
        x = CX + start_r * math.cos(angle)
        y = CY + start_r * math.sin(angle)

        length = rng.randint(8, 12 + stage)
        for step in range(length):
            ix, iy = int(x), int(y)
            dist = math.sqrt((ix - CX)**2 + (iy - CY)**2)
            if dist > radius:
                break
            if 0 <= ix < T and 0 <= iy < T and pixels[iy * T + ix][3] == 0:
                if rng.random() < 0.6:
                    v = 75 + rng.randint(0, 35)
                    pixels[iy * T + ix] = (v, v, v, 255)

            x += math.cos(angle) * 0.85 + rng.uniform(-0.5, 0.5)
            y += math.sin(angle) * 0.85 + rng.uniform(-0.5, 0.5)

    write_png(os.path.join(BLOCKS, f'destroy_stage_{stage}.png'), list(pixels), T, T)

print("Done")
