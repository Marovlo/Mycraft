#include "mesh_builder.h"
#include "texture_atlas.h"
#include "world/light_engine.h"

MeshBuilder::FaceQuad MeshBuilder::getFaceQuad(Direction dir) {
    switch (dir) {
        case Direction::PosX: return {{1,0,0},{1,1,0},{1,1,1},{1,0,1}};
        case Direction::NegX: return {{0,0,1},{0,1,1},{0,1,0},{0,0,0}};
        case Direction::PosY: return {{0,1,1},{1,1,1},{1,1,0},{0,1,0}};
        case Direction::NegY: return {{0,0,0},{1,0,0},{1,0,1},{0,0,1}};
        case Direction::PosZ: return {{1,0,1},{1,1,1},{0,1,1},{0,0,1}};
        case Direction::NegZ: return {{0,0,0},{0,1,0},{1,1,0},{1,0,0}};
        default: return {};
    }
}

void MeshBuilder::addFace(const glm::vec3& blockPos, Direction dir, uint16_t texId, float light) {
    FaceQuad quad = getFaceQuad(dir);
    glm::vec3 normal = directionNormal(dir);

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (atlas_) {
        uvRect = atlas_->getTileUV(texId);
    }

    glm::vec2 uv0(uvRect.x, uvRect.y);
    glm::vec2 uv1(uvRect.x, uvRect.w);
    glm::vec2 uv2(uvRect.z, uvRect.w);
    glm::vec2 uv3(uvRect.z, uvRect.y);

    uint32_t baseIdx = static_cast<uint32_t>(vertices_.size());

    vertices_.push_back({blockPos + quad.v0, normal, uv0, light});
    vertices_.push_back({blockPos + quad.v1, normal, uv1, light});
    vertices_.push_back({blockPos + quad.v2, normal, uv2, light});
    vertices_.push_back({blockPos + quad.v3, normal, uv3, light});

    indices_.push_back(baseIdx + 0);
    indices_.push_back(baseIdx + 1);
    indices_.push_back(baseIdx + 2);
    indices_.push_back(baseIdx + 0);
    indices_.push_back(baseIdx + 2);
    indices_.push_back(baseIdx + 3);
}

void MeshBuilder::addTransparentFace(const glm::vec3& blockPos, Direction dir, uint16_t texId, float light) {
    FaceQuad quad = getFaceQuad(dir);
    glm::vec3 normal = directionNormal(dir);

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (atlas_) {
        uvRect = atlas_->getTileUV(texId);
    }

    glm::vec2 uv0(uvRect.x, uvRect.y);
    glm::vec2 uv1(uvRect.x, uvRect.w);
    glm::vec2 uv2(uvRect.z, uvRect.w);
    glm::vec2 uv3(uvRect.z, uvRect.y);

    uint32_t baseIdx = static_cast<uint32_t>(transVertices_.size());

    transVertices_.push_back({blockPos + quad.v0, normal, uv0, light});
    transVertices_.push_back({blockPos + quad.v1, normal, uv1, light});
    transVertices_.push_back({blockPos + quad.v2, normal, uv2, light});
    transVertices_.push_back({blockPos + quad.v3, normal, uv3, light});

    transIndices_.push_back(baseIdx + 0);
    transIndices_.push_back(baseIdx + 1);
    transIndices_.push_back(baseIdx + 2);
    transIndices_.push_back(baseIdx + 0);
    transIndices_.push_back(baseIdx + 2);
    transIndices_.push_back(baseIdx + 3);
}

void MeshBuilder::addCrossFaces(const glm::vec3& blockPos, uint16_t texId, float light) {
    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (atlas_) uvRect = atlas_->getTileUV(texId);
    glm::vec2 uv0(uvRect.x, uvRect.y);
    glm::vec2 uv1(uvRect.x, uvRect.w);
    glm::vec2 uv2(uvRect.z, uvRect.w);
    glm::vec2 uv3(uvRect.z, uvRect.y);

    float p = 0.15f;
    glm::vec3 n(0, 1, 0);

    {
        uint32_t base = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back({blockPos + glm::vec3(p,   0, p),   n, uv1, light});
        vertices_.push_back({blockPos + glm::vec3(p,   1, p),   n, uv0, light});
        vertices_.push_back({blockPos + glm::vec3(1-p, 1, 1-p), n, uv3, light});
        vertices_.push_back({blockPos + glm::vec3(1-p, 0, 1-p), n, uv2, light});
        indices_.push_back(base); indices_.push_back(base+1); indices_.push_back(base+2);
        indices_.push_back(base); indices_.push_back(base+2); indices_.push_back(base+3);
        indices_.push_back(base); indices_.push_back(base+2); indices_.push_back(base+1);
        indices_.push_back(base); indices_.push_back(base+3); indices_.push_back(base+2);
    }
    {
        uint32_t base = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back({blockPos + glm::vec3(1-p, 0, p),   n, uv1, light});
        vertices_.push_back({blockPos + glm::vec3(1-p, 1, p),   n, uv0, light});
        vertices_.push_back({blockPos + glm::vec3(p,   1, 1-p), n, uv3, light});
        vertices_.push_back({blockPos + glm::vec3(p,   0, 1-p), n, uv2, light});
        indices_.push_back(base); indices_.push_back(base+1); indices_.push_back(base+2);
        indices_.push_back(base); indices_.push_back(base+2); indices_.push_back(base+3);
        indices_.push_back(base); indices_.push_back(base+2); indices_.push_back(base+1);
        indices_.push_back(base); indices_.push_back(base+3); indices_.push_back(base+2);
    }
}

void MeshBuilder::build(const World& world, const Chunk& chunk) {
    vertices_.clear();
    indices_.clear();
    transVertices_.clear();
    transIndices_.clear();

    vertices_.reserve(4096 * 4);
    indices_.reserve(4096 * 6);

    const auto& registry = BlockRegistry::instance();

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockId block = chunk.getBlock(x, y, z);
                if (registry.isAir(block)) continue;

                const auto& props = registry.get(block);
                if (props.renderType == BlockRenderType::None) continue;

                int wx = chunk.worldX() + x;
                int wz = chunk.worldZ() + z;
                glm::vec3 blockPos(static_cast<float>(wx),
                                   static_cast<float>(y),
                                   static_cast<float>(wz));

                // Cross-rendered blocks (flowers, grass): two diagonal faces, always visible
                if (props.renderType == BlockRenderType::Cross) {
                    uint16_t texId = props.textures.top;
                    uint8_t lightLvl = LightEngine::getLight(world, wx, y, wz);
                    addCrossFaces(blockPos, texId, LightEngine::lightToFloat(lightLvl));
                    continue;
                }

                for (int d = 0; d < static_cast<int>(Direction::COUNT); d++) {
                    Direction dir = static_cast<Direction>(d);
                    glm::ivec3 offset = directionOffset(dir);

                    int lnx = x + offset.x;
                    int lny = y + offset.y;
                    int lnz = z + offset.z;

                    BlockId neighbor;
                    if (lnx >= 0 && lnx < CHUNK_SIZE &&
                        lny >= 0 && lny < CHUNK_HEIGHT &&
                        lnz >= 0 && lnz < CHUNK_SIZE) {
                        neighbor = chunk.getBlock(lnx, lny, lnz);
                    } else {
                        neighbor = world.getBlock(wx + offset.x, y + offset.y, wz + offset.z);
                    }

                    const auto& neighborProps = registry.get(neighbor);

                    bool shouldRender = false;
                    if (neighborProps.isAir()) {
                        shouldRender = true;
                    } else if (!neighborProps.isOpaque) {
                        if (props.isLiquid && neighborProps.isLiquid && block == neighbor) {
                            shouldRender = false;
                        } else {
                            shouldRender = true;
                        }
                    }

                    if (shouldRender) {
                        uint16_t texId = props.textures.forDirection(dir);
                        // Light at the neighbor position (the air/transparent block the face borders)
                        int nlx = wx + offset.x;
                        int nly = y + offset.y;
                        int nlz = wz + offset.z;
                        uint8_t lightLvl = LightEngine::getLight(world, nlx, nly, nlz);
                        float lightF = LightEngine::lightToFloat(lightLvl);

                        // Route transparent blocks (water, glass) to the transparent mesh
                        if (props.renderType == BlockRenderType::Liquid) {
                            // Encode water: light = -(actualLight + 2.0) so shader detects it
                            addTransparentFace(blockPos, dir, texId, -(lightF + 2.0f));
                        } else if (props.renderType == BlockRenderType::Transparent) {
                            addTransparentFace(blockPos, dir, texId, lightF);
                        } else {
                            addFace(blockPos, dir, texId, lightF);
                        }
                    }
                }
            }
        }
    }
}

// ============================================================
// ChunkNeighbors — 线程安全的邻居访问
// ============================================================

BlockId ChunkNeighbors::getBlock(int wx, int wy, int wz) const {
    if (wy < 0 || wy >= CHUNK_HEIGHT) return Block::Air;

    int cx = blockToChunk(wx);
    int cz = blockToChunk(wz);
    int lx = blockToLocal(wx);
    int lz = blockToLocal(wz);

    int selfCx = self->chunkX();
    int selfCz = self->chunkZ();

    const Chunk* target = nullptr;
    if (cx == selfCx && cz == selfCz) target = self;
    else if (cx == selfCx + 1 && cz == selfCz) target = posX;
    else if (cx == selfCx - 1 && cz == selfCz) target = negX;
    else if (cx == selfCx && cz == selfCz + 1) target = posZ;
    else if (cx == selfCx && cz == selfCz - 1) target = negZ;

    if (!target) return Block::Air;
    return target->getBlock(lx, wy, lz);
}

// 线程安全的光照查询（只在 self + 4 邻居范围内查找）
static uint8_t neighborsGetLight(const ChunkNeighbors& n, int wx, int wy, int wz) {
    if (wy < 0 || wy >= CHUNK_HEIGHT) return 15;

    int cx = blockToChunk(wx);
    int cz = blockToChunk(wz);
    int lx = blockToLocal(wx);
    int lz = blockToLocal(wz);

    int selfCx = n.self->chunkX();
    int selfCz = n.self->chunkZ();

    const Chunk* target = nullptr;
    if (cx == selfCx && cz == selfCz) target = n.self;
    else if (cx == selfCx + 1 && cz == selfCz) target = n.posX;
    else if (cx == selfCx - 1 && cz == selfCz) target = n.negX;
    else if (cx == selfCx && cz == selfCz + 1) target = n.posZ;
    else if (cx == selfCx && cz == selfCz - 1) target = n.negZ;

    if (!target) return 15;
    return target->getMaxLight(lx, wy, lz);
}

// ============================================================
// Thread-safe build using ChunkNeighbors
// ============================================================

void MeshBuilder::build(const ChunkNeighbors& neighbors) {
    vertices_.clear();
    indices_.clear();
    transVertices_.clear();
    transIndices_.clear();

    vertices_.reserve(4096 * 4);
    indices_.reserve(4096 * 6);

    const auto& registry = BlockRegistry::instance();
    const Chunk& chunk = *neighbors.self;

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                BlockId block = chunk.getBlock(x, y, z);
                if (registry.isAir(block)) continue;

                const auto& props = registry.get(block);
                if (props.renderType == BlockRenderType::None) continue;

                int wx = chunk.worldX() + x;
                int wz = chunk.worldZ() + z;
                glm::vec3 blockPos(static_cast<float>(wx),
                                   static_cast<float>(y),
                                   static_cast<float>(wz));

                // Cross-rendered blocks (flowers, grass): two diagonal faces, always visible
                if (props.renderType == BlockRenderType::Cross) {
                    uint16_t texId = props.textures.top;
                    uint8_t lightLvl = neighborsGetLight(neighbors, wx, y, wz);
                    addCrossFaces(blockPos, texId, LightEngine::lightToFloat(lightLvl));
                    continue;
                }

                for (int d = 0; d < static_cast<int>(Direction::COUNT); d++) {
                    Direction dir = static_cast<Direction>(d);
                    glm::ivec3 offset = directionOffset(dir);

                    int lnx = x + offset.x;
                    int lny = y + offset.y;
                    int lnz = z + offset.z;

                    BlockId neighbor;
                    if (lnx >= 0 && lnx < CHUNK_SIZE &&
                        lny >= 0 && lny < CHUNK_HEIGHT &&
                        lnz >= 0 && lnz < CHUNK_SIZE) {
                        neighbor = chunk.getBlock(lnx, lny, lnz);
                    } else {
                        neighbor = neighbors.getBlock(wx + offset.x, y + offset.y, wz + offset.z);
                    }

                    const auto& neighborProps = registry.get(neighbor);

                    bool shouldRender = false;
                    if (neighborProps.isAir()) {
                        shouldRender = true;
                    } else if (!neighborProps.isOpaque) {
                        if (props.isLiquid && neighborProps.isLiquid && block == neighbor) {
                            shouldRender = false;
                        } else {
                            shouldRender = true;
                        }
                    }

                    if (shouldRender) {
                        uint16_t texId = props.textures.forDirection(dir);
                        int nlx = wx + offset.x;
                        int nly = y + offset.y;
                        int nlz = wz + offset.z;
                        uint8_t lightLvl = neighborsGetLight(neighbors, nlx, nly, nlz);
                        float lightF = LightEngine::lightToFloat(lightLvl);

                        if (props.renderType == BlockRenderType::Liquid) {
                            addTransparentFace(blockPos, dir, texId, -(lightF + 2.0f));
                        } else if (props.renderType == BlockRenderType::Transparent) {
                            addTransparentFace(blockPos, dir, texId, lightF);
                        } else {
                            addFace(blockPos, dir, texId, lightF);
                        }
                    }
                }
            }
        }
    }
}
