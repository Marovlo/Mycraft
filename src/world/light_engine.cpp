#include "light_engine.h"
#include "core/block.h"
#include <queue>
#include <cmath>
#include <algorithm>

static const float kLightCurve[16] = {
    0.05f, 0.067f, 0.085f, 0.106f, 0.13f, 0.16f, 0.2f, 0.25f,
    0.31f, 0.39f, 0.48f, 0.6f, 0.73f, 0.84f, 0.93f, 1.0f,
};

float LightEngine::lightToFloat(uint8_t level) {
    return kLightCurve[level & 0xF];
}

// Helper: get/set light at world coords with cross-chunk support
static uint8_t worldSkyLight(const World& world, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_HEIGHT) return 15;
    int cx = blockToChunk(wx), cz = blockToChunk(wz);
    const Chunk* c = world.getChunk(cx, cz);
    if (!c) return 15;
    return c->getSkyLight(blockToLocal(wx), wy, blockToLocal(wz));
}

static uint8_t worldBlockLight(const World& world, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_HEIGHT) return 0;
    int cx = blockToChunk(wx), cz = blockToChunk(wz);
    const Chunk* c = world.getChunk(cx, cz);
    if (!c) return 0;
    return c->getBlockLight(blockToLocal(wx), wy, blockToLocal(wz));
}

static void setWorldSkyLight(World& world, int wx, int wy, int wz, uint8_t val) {
    if (wy < 0 || wy >= CHUNK_HEIGHT) return;
    int cx = blockToChunk(wx), cz = blockToChunk(wz);
    Chunk* c = world.getChunk(cx, cz);
    if (!c) return;
    c->setSkyLight(blockToLocal(wx), wy, blockToLocal(wz), val);
}

static void setWorldBlockLight(World& world, int wx, int wy, int wz, uint8_t val) {
    if (wy < 0 || wy >= CHUNK_HEIGHT) return;
    int cx = blockToChunk(wx), cz = blockToChunk(wz);
    Chunk* c = world.getChunk(cx, cz);
    if (!c) return;
    c->setBlockLight(blockToLocal(wx), wy, blockToLocal(wz), val);
}

static void markWorldChunkDirty(World& world, int wx, int wz) {
    world.markChunkDirty(blockToChunk(wx), blockToChunk(wz));
}

static const int dx6[] = {1, -1, 0, 0, 0, 0};
static const int dy6[] = {0, 0, 1, -1, 0, 0};
static const int dz6[] = {0, 0, 0, 0, 1, -1};

uint8_t LightEngine::getLight(const World& world, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_HEIGHT) return 15;
    int cx = blockToChunk(wx), cz = blockToChunk(wz);
    const Chunk* chunk = world.getChunk(cx, cz);
    if (!chunk) return 15;
    return chunk->getMaxLight(blockToLocal(wx), wy, blockToLocal(wz));
}

void LightEngine::initSkyLight(Chunk& chunk) {
    chunk.updateHeightMap();

    // Phase 1: Set skyLight=15 for all blocks above the heightmap (direct sunlight).
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int h = chunk.getHeight(x, z);
            for (int y = CHUNK_HEIGHT - 1; y >= h; --y) {
                chunk.setSkyLight(x, y, z, 15);
            }
        }
    }

    // Phase 2: BFS propagation into caves/overhangs.
    // Start from all blocks that have skyLight=15 and are adjacent to dark blocks.
    struct LightNode { int x, y, z; uint8_t level; };
    std::queue<LightNode> q;

    const auto& reg = BlockRegistry::instance();

    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            int h = chunk.getHeight(x, z);
            // The block just below the heightmap column is a good BFS seed
            if (h > 0 && h < CHUNK_HEIGHT) {
                // Check if any horizontal neighbor at this height has lower light
                q.push({x, h - 1, z, 14});
            }
        }
    }

    // BFS within chunk only (cross-chunk sky light is handled at chunk-load time)
    static const int dx[] = {1, -1, 0, 0, 0, 0};
    static const int dy[] = {0, 0, 1, -1, 0, 0};
    static const int dz[] = {0, 0, 0, 0, 1, -1};

    while (!q.empty()) {
        auto [nx, ny, nz, level] = q.front();
        q.pop();

        if (level == 0) continue;
        if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_HEIGHT || nz < 0 || nz >= CHUNK_SIZE)
            continue;

        BlockId block = chunk.getBlock(nx, ny, nz);
        if (reg.isOpaque(block)) continue;

        uint8_t current = chunk.getSkyLight(nx, ny, nz);
        if (level <= current) continue;

        chunk.setSkyLight(nx, ny, nz, level);

        for (int d = 0; d < 6; ++d) {
            // MC rule: sky light at level 15 propagates downward without attenuation
            // (simulates direct sunlight passing through transparent blocks).
            // dy[d]==-1 means propagating downward (neighbor is below current).
            bool downAt15 = (dy[d] == -1 && level == 15);
            uint8_t spread = downAt15 ? uint8_t(15) : static_cast<uint8_t>(level - 1);
            q.push({nx + dx[d], ny + dy[d], nz + dz[d], spread});
        }
    }
}

void LightEngine::initBlockLight(Chunk& chunk) {
    const auto& reg = BlockRegistry::instance();

    struct LightNode { int x, y, z; uint8_t level; };
    std::queue<LightNode> q;

    // Find all light-emitting blocks and seed BFS
    for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                BlockId block = chunk.getBlock(x, y, z);
                uint8_t emit = reg.get(block).lightEmit;
                if (emit > 0) {
                    chunk.setBlockLight(x, y, z, emit);
                    q.push({x, y, z, emit});
                }
            }
        }
    }

    static const int dx[] = {1, -1, 0, 0, 0, 0};
    static const int dy[] = {0, 0, 1, -1, 0, 0};
    static const int dz[] = {0, 0, 0, 0, 1, -1};

    while (!q.empty()) {
        auto [sx, sy, sz, level] = q.front();
        q.pop();

        if (level <= 1) continue;
        uint8_t spread = level - 1;

        for (int d = 0; d < 6; ++d) {
            int nx = sx + dx[d], ny = sy + dy[d], nz = sz + dz[d];
            if (nx < 0 || nx >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_HEIGHT || nz < 0 || nz >= CHUNK_SIZE)
                continue;

            BlockId block = chunk.getBlock(nx, ny, nz);
            if (reg.isOpaque(block)) continue;

            if (chunk.getBlockLight(nx, ny, nz) < spread) {
                chunk.setBlockLight(nx, ny, nz, spread);
                q.push({nx, ny, nz, spread});
            }
        }
    }
}

// ============================================================
// Dynamic light update
// ============================================================

void LightEngine::updateAfterBlockChange(World& world, int wx, int wy, int wz) {
    const auto& reg = BlockRegistry::instance();
    const int R = 16;

    struct Node { int x, y, z; uint8_t level; };
    std::queue<Node> skyQ, blkQ;

    // Phase 1: Clear light in sphere, collect emitters
    for (int ddx = -R; ddx <= R; ++ddx)
        for (int ddy = -R; ddy <= R; ++ddy)
            for (int ddz = -R; ddz <= R; ++ddz) {
                if (ddx*ddx+ddy*ddy+ddz*ddz > R*R) continue;
                int nx=wx+ddx, ny=wy+ddy, nz=wz+ddz;
                if (ny<0||ny>=CHUNK_HEIGHT) continue;
                setWorldSkyLight(world, nx, ny, nz, 0);
                setWorldBlockLight(world, nx, ny, nz, 0);
                BlockId b = world.getBlock(nx, ny, nz);
                uint8_t e = reg.get(b).lightEmit;
                if (e > 0) { setWorldBlockLight(world, nx, ny, nz, e); blkQ.push({nx,ny,nz,e}); }
            }

    // Phase 2: Seed sky from above (columns in radius)
    // For each column, re-establish the direct sunlight pillar (skyLight=15 from top
    // down to the highest opaque block). Then seed BFS from every sunlit block that
    // falls inside the cleared sphere so that light propagates horizontally into
    // adjacent caves/tunnels.
    int yMin = std::max(0, wy - R);
    int yMax = std::min(CHUNK_HEIGHT - 1, wy + R);
    for (int ddx = -R; ddx <= R; ++ddx)
        for (int ddz = -R; ddz <= R; ++ddz) {
            if (ddx*ddx+ddz*ddz > R*R) continue;
            int cx2 = wx+ddx, cz2 = wz+ddz;
            // Find heightmap at this column
            int cxc = blockToChunk(cx2), czc = blockToChunk(cz2);
            Chunk* ch = world.getChunk(cxc, czc);
            if (!ch) continue;
            int lx = blockToLocal(cx2), lz = blockToLocal(cz2);
            // Recalc height for this column
            int h = 0;
            for (int y = CHUNK_HEIGHT-1; y>=0; --y) {
                if (reg.isOpaque(ch->getBlock(lx,y,lz))) { h=y+1; break; }
            }
            for (int y = CHUNK_HEIGHT-1; y>=h; --y) {
                setWorldSkyLight(world, cx2, y, cz2, 15);
            }
            // Seed BFS from every sunlit block inside the cleared region.
            // These seeds allow sky light to propagate horizontally into tunnels.
            int seedLo = std::max(h, yMin);
            int seedHi = yMax;  // sunlit blocks go up to CHUNK_HEIGHT-1, but we only need seeds in cleared area
            for (int y = seedLo; y <= seedHi; ++y) {
                // Only seed non-opaque blocks that are within the cleared sphere
                int ddy = y - wy;
                if (ddx*ddx+ddy*ddy+ddz*ddz > R*R) continue;
                skyQ.push({cx2, y, cz2, 15});
            }
        }

    // Phase 3: Seed from boundary (light leaking in from outside cleared area)
    for (int ddx = -(R+1); ddx <= R+1; ++ddx)
        for (int ddy = -(R+1); ddy <= R+1; ++ddy)
            for (int ddz = -(R+1); ddz <= R+1; ++ddz) {
                int d2 = ddx*ddx+ddy*ddy+ddz*ddz;
                if (d2 <= R*R || d2 > (R+1)*(R+1)) continue;
                int nx=wx+ddx, ny=wy+ddy, nz=wz+ddz;
                if (ny<0||ny>=CHUNK_HEIGHT) continue;
                uint8_t s = worldSkyLight(world,nx,ny,nz);
                if (s>0) skyQ.push({nx,ny,nz,s});
                uint8_t bl = worldBlockLight(world,nx,ny,nz);
                if (bl>0) blkQ.push({nx,ny,nz,bl});
            }

    // Phase 4: BFS sky light
    // MC rule: sky light at level 15 propagates downward without attenuation
    // (direct sunlight passes through transparent blocks vertically).
    while (!skyQ.empty()) {
        auto [sx,sy,sz,lv] = skyQ.front(); skyQ.pop();
        if (lv<=0) continue;
        for (int d=0;d<6;++d) {
            int nx=sx+dx6[d], ny=sy+dy6[d], nz=sz+dz6[d];
            if (ny<0||ny>=CHUNK_HEIGHT) continue;
            if (reg.isOpaque(world.getBlock(nx,ny,nz))) continue;
            // dy6[d]==-1 means propagating downward; sky light 15 doesn't attenuate downward
            bool downAt15 = (dy6[d] == -1 && lv == 15);
            uint8_t sp = downAt15 ? uint8_t(15) : static_cast<uint8_t>(lv-1);
            if (worldSkyLight(world,nx,ny,nz)<sp) {
                setWorldSkyLight(world,nx,ny,nz,sp);
                skyQ.push({nx,ny,nz,sp});
                markWorldChunkDirty(world,nx,nz);
            }
        }
    }

    // Phase 5: BFS block light
    while (!blkQ.empty()) {
        auto [sx,sy,sz,lv] = blkQ.front(); blkQ.pop();
        if (lv<=1) continue;
        uint8_t sp = static_cast<uint8_t>(lv-1);
        for (int d=0;d<6;++d) {
            int nx=sx+dx6[d], ny=sy+dy6[d], nz=sz+dz6[d];
            if (ny<0||ny>=CHUNK_HEIGHT) continue;
            if (reg.isOpaque(world.getBlock(nx,ny,nz))) continue;
            if (worldBlockLight(world,nx,ny,nz)<sp) {
                setWorldBlockLight(world,nx,ny,nz,sp);
                blkQ.push({nx,ny,nz,sp});
                markWorldChunkDirty(world,nx,nz);
            }
        }
    }

    markWorldChunkDirty(world, wx, wz);
}
