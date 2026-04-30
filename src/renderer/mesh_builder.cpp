#include "mesh_builder.h"
#include "texture_atlas.h"
#include "world/light_engine.h"
#include "world/biome_colormap.h"
#include "world/terrain_generator.h"

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

void MeshBuilder::addFace(const glm::vec3& blockPos, Direction dir, uint16_t texId, float light, const glm::vec3& color) {
    FaceQuad quad = getFaceQuad(dir);
    glm::vec3 normal = directionNormal(dir);

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (atlas_) {
        uvRect = atlas_->getTileUV(texId);
    }

    // UV坐标：vMax对应图片底部，vMin对应图片顶部（Vulkan纹理坐标系）
    // 顶点顺序中 v0/v3 是底部(Y=0)，v1/v2 是顶部(Y=1)
    // 所以 v0/v3 应映射到 vMax（图片底部），v1/v2 映射到 vMin（图片顶部）
    glm::vec2 uv0(uvRect.x, uvRect.w);  // 左下（图片底部）
    glm::vec2 uv1(uvRect.x, uvRect.y);  // 左上（图片顶部）
    glm::vec2 uv2(uvRect.z, uvRect.y);  // 右上（图片顶部）
    glm::vec2 uv3(uvRect.z, uvRect.w);  // 右下（图片底部）

    uint32_t baseIdx = static_cast<uint32_t>(vertices_.size());

    vertices_.push_back({blockPos + quad.v0, normal, uv0, light, color});
    vertices_.push_back({blockPos + quad.v1, normal, uv1, light, color});
    vertices_.push_back({blockPos + quad.v2, normal, uv2, light, color});
    vertices_.push_back({blockPos + quad.v3, normal, uv3, light, color});

    indices_.push_back(baseIdx + 0);
    indices_.push_back(baseIdx + 1);
    indices_.push_back(baseIdx + 2);
    indices_.push_back(baseIdx + 0);
    indices_.push_back(baseIdx + 2);
    indices_.push_back(baseIdx + 3);
}

void MeshBuilder::addTransparentFace(const glm::vec3& blockPos, Direction dir, uint16_t texId, float light, const glm::vec3& color) {
    FaceQuad quad = getFaceQuad(dir);
    glm::vec3 normal = directionNormal(dir);

    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (atlas_) {
        uvRect = atlas_->getTileUV(texId);
    }

    glm::vec2 uv0(uvRect.x, uvRect.w);
    glm::vec2 uv1(uvRect.x, uvRect.y);
    glm::vec2 uv2(uvRect.z, uvRect.y);
    glm::vec2 uv3(uvRect.z, uvRect.w);

    uint32_t baseIdx = static_cast<uint32_t>(transVertices_.size());

    transVertices_.push_back({blockPos + quad.v0, normal, uv0, light, color});
    transVertices_.push_back({blockPos + quad.v1, normal, uv1, light, color});
    transVertices_.push_back({blockPos + quad.v2, normal, uv2, light, color});
    transVertices_.push_back({blockPos + quad.v3, normal, uv3, light, color});

    transIndices_.push_back(baseIdx + 0);
    transIndices_.push_back(baseIdx + 1);
    transIndices_.push_back(baseIdx + 2);
    transIndices_.push_back(baseIdx + 0);
    transIndices_.push_back(baseIdx + 2);
    transIndices_.push_back(baseIdx + 3);
}

void MeshBuilder::addCrossFaces(const glm::vec3& blockPos, uint16_t texId, float light, const glm::vec3& color) {
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
        vertices_.push_back({blockPos + glm::vec3(p,   0, p),   n, uv1, light, color});
        vertices_.push_back({blockPos + glm::vec3(p,   1, p),   n, uv0, light, color});
        vertices_.push_back({blockPos + glm::vec3(1-p, 1, 1-p), n, uv3, light, color});
        vertices_.push_back({blockPos + glm::vec3(1-p, 0, 1-p), n, uv2, light, color});
        indices_.push_back(base); indices_.push_back(base+1); indices_.push_back(base+2);
        indices_.push_back(base); indices_.push_back(base+2); indices_.push_back(base+3);
        indices_.push_back(base); indices_.push_back(base+2); indices_.push_back(base+1);
        indices_.push_back(base); indices_.push_back(base+3); indices_.push_back(base+2);
    }
    {
        uint32_t base = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back({blockPos + glm::vec3(1-p, 0, p),   n, uv1, light, color});
        vertices_.push_back({blockPos + glm::vec3(1-p, 1, p),   n, uv0, light, color});
        vertices_.push_back({blockPos + glm::vec3(p,   1, 1-p), n, uv3, light, color});
        vertices_.push_back({blockPos + glm::vec3(p,   0, 1-p), n, uv2, light, color});
        indices_.push_back(base); indices_.push_back(base+1); indices_.push_back(base+2);
        indices_.push_back(base); indices_.push_back(base+2); indices_.push_back(base+3);
        indices_.push_back(base); indices_.push_back(base+2); indices_.push_back(base+1);
        indices_.push_back(base); indices_.push_back(base+3); indices_.push_back(base+2);
    }
}

void MeshBuilder::build(const World& world, const Chunk& chunk) {
    // 构造 ChunkNeighbors 后委托给线程安全版本，避免代码重复
    ChunkNeighbors neighbors;
    neighbors.self = &chunk;
    neighbors.posX = world.getChunk(chunk.chunkX() + 1, chunk.chunkZ());
    neighbors.negX = world.getChunk(chunk.chunkX() - 1, chunk.chunkZ());
    neighbors.posZ = world.getChunk(chunk.chunkX(), chunk.chunkZ() + 1);
    neighbors.negZ = world.getChunk(chunk.chunkX(), chunk.chunkZ() - 1);
    build(neighbors);
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

glm::vec3 MeshBuilder::getTintColor(TintType tintType, int wx, int wz) const {
    if (tintType == TintType::None) return glm::vec3(1.0f);
    if (tintType == TintType::SpruceFixed) return BiomeColorMap::getSpruceColor();

    // 需要查询生物群系
    int biomeType = 0; // 默认 Plains
    if (terrainGen_) {
        auto biome = terrainGen_->getBiome(wx, wz);
        biomeType = static_cast<int>(biome);
    }

    if (!biomeColorMap_) return glm::vec3(1.0f);

    if (tintType == TintType::Grass) {
        return biomeColorMap_->getGrassColorForBiome(biomeType);
    } else if (tintType == TintType::Foliage) {
        return biomeColorMap_->getFoliageColorForBiome(biomeType);
    }
    return glm::vec3(1.0f);
}

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
                    TintType tint = props.faceTint.top;
                    glm::vec3 color = getTintColor(tint, wx, wz);
                    addCrossFaces(blockPos, texId, LightEngine::lightToFloat(lightLvl), color);
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

                        // 获取该面的 tint 颜色
                        TintType tint = props.faceTint.forDirection(dir);
                        glm::vec3 color = getTintColor(tint, wx, wz);

                        if (props.renderType == BlockRenderType::Liquid) {
                            addTransparentFace(blockPos, dir, texId, -(lightF + 2.0f), color);
                        } else if (props.renderType == BlockRenderType::Transparent) {
                            addTransparentFace(blockPos, dir, texId, lightF, color);
                        } else {
                            addFace(blockPos, dir, texId, lightF, color);
                        }
                    }
                }
            }
        }
    }
}
