#include "mesh_builder.h"

MeshBuilder::FaceQuad MeshBuilder::getFaceQuad(Direction dir) {
    switch (dir) {
        case Direction::PosX: return {{1,0,0},{1,1,0},{1,1,1},{1,0,1}};
        case Direction::NegX: return {{0,0,1},{0,1,1},{0,1,0},{0,0,0}};
        case Direction::PosY: return {{0,1,1},{1,1,1},{1,1,0},{0,1,0}};
        case Direction::NegY: return {{0,0,0},{1,0,0},{1,0,1},{0,0,1}};
        case Direction::PosZ: return {{0,0,1},{0,1,1},{1,1,1},{1,0,1}};
        case Direction::NegZ: return {{1,0,0},{1,1,0},{0,1,0},{0,0,0}};
        default: return {};
    }
}

void MeshBuilder::addFace(const glm::vec3& blockPos, Direction dir, uint16_t texId) {
    FaceQuad quad = getFaceQuad(dir);
    glm::vec3 normal = directionNormal(dir);

    // UV: encode texture ID as a normalized value for now.
    // Later this will map to actual atlas coordinates.
    float texU = static_cast<float>(texId);
    glm::vec2 uv0(texU, 0.0f);
    glm::vec2 uv1(texU, 1.0f);
    glm::vec2 uv2(texU + 1.0f, 1.0f);
    glm::vec2 uv3(texU + 1.0f, 0.0f);

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

                    int nx = wx + offset.x;
                    int ny = y  + offset.y;
                    int nz = wz + offset.z;

                    BlockId neighbor = world.getBlock(nx, ny, nz);
                    const auto& neighborProps = registry.get(neighbor);

                    // Determine if this face should be rendered
                    bool shouldRender = false;

                    if (neighborProps.isAir()) {
                        shouldRender = true;
                    } else if (!neighborProps.isOpaque) {
                        // Render face if neighbor is transparent/liquid
                        // But don't render same-type liquid faces against each other
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
