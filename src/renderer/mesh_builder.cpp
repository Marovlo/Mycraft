#include "mesh_builder.h"
#include "texture_atlas.h"

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

void MeshBuilder::addFace(const glm::vec3& blockPos, Direction dir, uint16_t texId) {
    FaceQuad quad = getFaceQuad(dir);
    glm::vec3 normal = directionNormal(dir);

    // Get 2D UV rect from atlas: {uMin, vMin, uMax, vMax}
    glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (atlas_) {
        uvRect = atlas_->getTileUV(texId);
    }

    glm::vec2 uv0(uvRect.x, uvRect.y);  // top-left
    glm::vec2 uv1(uvRect.x, uvRect.w);  // bottom-left
    glm::vec2 uv2(uvRect.z, uvRect.w);  // bottom-right
    glm::vec2 uv3(uvRect.z, uvRect.y);  // top-right

    uint32_t baseIdx = static_cast<uint32_t>(vertices_.size());

    vertices_.push_back({blockPos + quad.v0, normal, uv0});
    vertices_.push_back({blockPos + quad.v1, normal, uv1});
    vertices_.push_back({blockPos + quad.v2, normal, uv2});
    vertices_.push_back({blockPos + quad.v3, normal, uv3});

    indices_.push_back(baseIdx + 0);
    indices_.push_back(baseIdx + 1);
    indices_.push_back(baseIdx + 2);
    indices_.push_back(baseIdx + 0);
    indices_.push_back(baseIdx + 2);
    indices_.push_back(baseIdx + 3);
}

void MeshBuilder::build(const World& world, const Chunk& chunk) {
    vertices_.clear();
    indices_.clear();

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
                        addFace(blockPos, dir, texId);
                    }
                }
            }
        }
    }
}
