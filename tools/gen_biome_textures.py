#!/usr/bin/env python3
import struct,zlib,os,random
def write_png(p,px,w,h):
    def c(t,d):
        x=t+d;return struct.pack('>I',len(d))+x+struct.pack('>I',zlib.crc32(x)&0xFFFFFFFF)
    r=b''
    for y in range(h):
        r+=b'\x00'
        for x2 in range(w):
            a,b2,g,al=px[y*w+x2];r+=struct.pack('BBBB',a,b2,g,al)
    with open(p,'wb') as f:f.write(b'\x89PNG\r\n\x1a\n'+c(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+c(b'IDAT',zlib.compress(r))+c(b'IEND',b''))
T=16;B=os.path.join(os.path.dirname(__file__),'..','assets','textures','blocks')
def gen(base,var,seed):
    rng=random.Random(seed)
    return [(max(0,min(255,base[0]+rng.randint(-var,var))),max(0,min(255,base[1]+rng.randint(-var,var))),max(0,min(255,base[2]+rng.randint(-var,var))),255) for _ in range(T*T)]
write_png(f'{B}/snow.png',gen((240,245,255),10,800),T,T)
write_png(f'{B}/sandstone.png',gen((210,190,140),15,801),T,T)
write_png(f'{B}/spruce_log_side.png',gen((60,40,25),10,802),T,T)
write_png(f'{B}/spruce_log_top.png',gen((80,60,35),15,803),T,T)
write_png(f'{B}/spruce_leaves.png',gen((25,60,30),12,804),T,T)
write_png(f'{B}/cactus_side.png',gen((40,120,45),15,805),T,T)
write_png(f'{B}/cactus_top.png',gen((30,100,35),12,806),T,T)
print('Generated 7 biome textures')
