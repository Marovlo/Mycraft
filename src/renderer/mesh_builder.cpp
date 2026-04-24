#include "mesh_builder.h"

MeshBuilder::FaceQuad MeshBuilder::getFaceQuad(Direction dir) {
    // Vertices must be counter-clockwise when viewed from the face's outward normal direction.
    // Triangles: v0→v1→v2, v0→v2→v3
    switch (dir) {
        case Direction::PosX: return {{1,0,0},{1,1,0},{1,1,1},{1,0,1}};
        case Direction::NegX: return {{0,0,1},{0,1,1},{0,1,0},{0,0,0}};
        case Direction::PosY: return {{0,1,1},{1,1,1},{1,1,0},{0,1,0}};
        case Direction::NegY: return {{0,0,0},{1,0,0},{1,0,1},{0,0,1}};
        case Direction::PosZ: return {{1,0,1},{1,1,1},{0,1,1},{0,0,1}};  // fixed: reversed winding
        case Direction::NegZ: return {{0,0,0},{0,1,0},{1,1,0},{1,0,0}};  // fixed: reversed winding
        default: return {};
    }
}

void MeshBuilder::addFace(const glm::vec3& blockPos, Direction dir, uint16_t texId) {
    FaceQuad quad = getFaceQuad(dir);
    glm::vec3 normal = directionNormal(dir);

    // Normalize UV to atlas coordinates: each tile occupies [texId/N, (texId+1)/N] in U
    float invN = 1.0f / static_cast<float>(atlasTileCount_);
    float uMin = static_cast<float>(texId) * invN;
    float uMax = static_cast<float>(texId + 1) * invN;

    glm::vec2 uv0(uMin, 0.0f);
    glm::vec2 uv1(uMin, 1.0f);
    glm::vec2 uv2(uMax, 1.0f);
    glm::vec2 uv3(uMax, 0.0f);

    uint32_t baseIdx = static_cast<uint32_t>(vertices_.size());

    vertices_.push_back({blockPos + quad.v0, normal, uv0});
    vertices_.push_back({blockPos + quad.v1, normal, uv1});
    vertices_.push_back({blockPos + quad.v2, normal, uv2});
    vertices_.push_back({blockPos + quad.v3, normal, uv3});

    // Two triangles per face (CCW winding)
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

    // Pre-allocate: rough estimate — exposed surface faces are a small fraction of total.
    // A typical chunk has ~2000-8000 visible faces. Reserve conservatively.
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

                // Check each face
                for (int d = 0; d < static_cast<int>(Direction::COUNT); d++) {
                    Direction dir = static_cast<Direction>(d);
                    glm::ivec3 offset = directionOffset(dir);

                    int lnx = x + offset.x;
                    int lny = y + offset.y;
                    int lnz = z + offset.z;

                    // Fast path: neighbor is within the same chunk — avoid world hash lookup
                    BlockId neighbor;
                    if (lnx >= 0 && lnx < CHUNK_SIZE &&
                        lny >= 0 && lny < CHUNK_HEIGHT &&
                        lnz >= 0 && lnz < CHUNK_SIZE) {
                        neighbor = chunk.getBlock(lnx, lny, lnz);
                    } else {
                        // Border: query world (cross-chunk or out-of-bounds)
                        neighbor = world.getBlock(wx + offset.x, y + offset.y, wz + offset.z);
                    }

                    const auto& neighborProps = registry.get(neighbor);

                    // Determine if this face should be rendered
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
