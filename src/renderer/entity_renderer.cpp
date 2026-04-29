#include "entity_renderer.h"
#include "texture_atlas.h"
#include "entity/item_entity.h"
#include "entity/arrow_entity.h"
#include "entity/xp_orb_entity.h"
#include "core/item.h"
#include "core/block.h"
#include "core/common.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <cmath>

// ----- Unit-cube face table (same convention as MeshBuilder) -----
//       CCW winding when viewed from outside. Coordinates span [0,1]³.
struct UnitFace {
    glm::vec3 v0, v1, v2, v3;
    glm::vec3 normal;
    Direction dir;
};
static const UnitFace kUnitFaces[6] = {
    {{1,0,0},{1,1,0},{1,1,1},{1,0,1}, { 1, 0, 0}, Direction::PosX},
    {{0,0,1},{0,1,1},{0,1,0},{0,0,0}, {-1, 0, 0}, Direction::NegX},
    {{0,1,1},{1,1,1},{1,1,0},{0,1,0}, { 0, 1, 0}, Direction::PosY},
    {{0,0,0},{1,0,0},{1,0,1},{0,0,1}, { 0,-1, 0}, Direction::NegY},
    {{1,0,1},{1,1,1},{0,1,1},{0,0,1}, { 0, 0, 1}, Direction::PosZ},
    {{0,0,0},{0,1,0},{1,1,0},{1,0,0}, { 0, 0,-1}, Direction::NegZ},
};

void EntityRenderer::init(VulkanEngine* engine, const TextureAtlas* atlas) {
    engine_ = engine;
    atlas_  = atlas;

    // Start with a small capacity; we grow on demand.
    constexpr size_t INITIAL_VERTS = 6 * 4 * 32; // 32 items worth of faces
    constexpr size_t INITIAL_INDS  = 6 * 6 * 32;
    vertexBufferSize_ = sizeof(Vertex)   * INITIAL_VERTS;
    indexBufferSize_  = sizeof(uint32_t) * INITIAL_INDS;
    vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indexBuffer_  = engine_->createDynamicBuffer(indexBufferSize_,  VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void EntityRenderer::destroy() {
    if (!engine_) return;
    if (vertexBuffer_.allocation) {
        vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        vertexBuffer_ = {};
    }
    if (indexBuffer_.allocation) {
        vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        indexBuffer_ = {};
    }
    engine_ = nullptr;
    atlas_  = nullptr;
}

void EntityRenderer::ensureCapacity() {
    VkDeviceSize needV = sizeof(Vertex)   * vertices_.size();
    VkDeviceSize needI = sizeof(uint32_t) * indices_.size();

    if (needV > vertexBufferSize_) {
        if (vertexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        }
        vertexBufferSize_ = needV * 2;
        vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
    if (needI > indexBufferSize_) {
        if (indexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        }
        indexBufferSize_ = needI * 2;
        indexBuffer_ = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }
}

void EntityRenderer::appendItemMesh(const ItemEntity& item, float partialTick) {
    const auto& itemProps = ItemRegistry::instance().get(item.stack.id);

    // Decide rendering style: block items with full-cube render type → mini cube,
    // everything else (tools, minerals, Cross-type blocks like torches/flowers) → flat sprite.
    bool isBlockItem = false;
    if (itemProps.type == ItemType::Block && itemProps.blockId > 0) {
        auto rt = BlockRegistry::instance().get(itemProps.blockId).renderType;
        isBlockItem = (rt == BlockRenderType::Opaque || rt == BlockRenderType::Liquid);
    }

    // --- Render interpolation ---
    glm::vec3 renderPos = glm::mix(item.prevPosition, item.position, partialTick);

    // MC-style bobbing: gentle sine-wave vertical oscillation.
    // Period ~40 ticks (2 s), amplitude ~0.1 blocks. visualPhase offsets each
    // item so a pile of drops doesn't bob in perfect sync.
    float bobTime = (static_cast<float>(item.tickCount) + partialTick) * 0.15708f; // 2π/40
    float bobOffset = std::sin(bobTime + item.visualPhase) * 0.065f;
    renderPos.y += bobOffset;

    // Yaw wraparound: when the spin crosses 2π we reset it in tick(), which
    // makes prev ≈ 2π and current ≈ 0. Detect a big negative delta and bias
    // current up by 2π so the short-way interpolation stays continuous.
    float curYaw  = item.visualYaw;
    float prevYaw = item.prevVisualYaw;
    if (curYaw - prevYaw < -3.14159265f) curYaw += 6.2831853f;
    float renderYaw = glm::mix(prevYaw, curYaw, partialTick);

    if (isBlockItem) {
        // ===== Block item: render as a mini cube (existing behavior) =====
        BlockId blockId = itemProps.blockId;

        glm::mat4 M(1.0f);
        M = glm::translate(M, renderPos);
        M = glm::rotate(M, renderYaw, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec3 size = item.halfExtents * 2.0f;
        M = glm::scale(M, size);
        M = glm::translate(M, glm::vec3(-0.5f, -0.5f, -0.5f));

        glm::mat3 N = glm::mat3(glm::rotate(glm::mat4(1.0f), renderYaw, glm::vec3(0,1,0)));

        for (const UnitFace& f : kUnitFaces) {
            const auto& blockProps = BlockRegistry::instance().get(blockId);
            uint16_t texId = blockProps.textures.forDirection(f.dir);
            glm::vec4 uvRect = atlas_->getTileUV(texId);
            glm::vec2 uv0(uvRect.x, uvRect.w);
            glm::vec2 uv1(uvRect.x, uvRect.y);
            glm::vec2 uv2(uvRect.z, uvRect.y);
            glm::vec2 uv3(uvRect.z, uvRect.w);

            auto xf = [&](const glm::vec3& v) {
                glm::vec4 p = M * glm::vec4(v, 1.0f);
                return glm::vec3(p);
            };
            glm::vec3 wn = glm::normalize(N * f.normal);

            uint32_t base = static_cast<uint32_t>(vertices_.size());
            vertices_.push_back({xf(f.v0), wn, uv0, 1.0f});
            vertices_.push_back({xf(f.v1), wn, uv1, 1.0f});
            vertices_.push_back({xf(f.v2), wn, uv2, 1.0f});
            vertices_.push_back({xf(f.v3), wn, uv3, 1.0f});
            indices_.push_back(base + 0);
            indices_.push_back(base + 1);
            indices_.push_back(base + 2);
            indices_.push_back(base + 0);
            indices_.push_back(base + 2);
            indices_.push_back(base + 3);
        }
    } else {
        // ===== Non-block item: MC-style flat sprite with thin edges =====
        // A card-like shape: front face + back face + 4 thin edge strips.
        // Thickness = 1/16 of the sprite size (1 pixel in MC's 16px items).
        uint16_t texId = 0;
        if (!itemProps.iconTileName.empty()) {
            texId = atlas_->getTileIndex(itemProps.iconTileName);
        } else {
            // Fallback: cobblestone
            texId = BlockRegistry::instance().get(Block::Cobblestone).textures.forDirection(Direction::PosZ);
        }

        glm::vec4 uvRect = atlas_->getTileUV(texId);
        float uMin = uvRect.x, vMin = uvRect.y, uMax = uvRect.z, vMax = uvRect.w;

        // Sprite dimensions: square face, 3/16 thickness for visible depth
        // MC dropped items look like a small card with noticeable thickness.
        float spriteSize = item.halfExtents.x * 2.0f;  // full width/height
        float thickness  = spriteSize * 3.0f / 16.0f;   // 3 pixels thick — visible edge

        // Build model matrix: translate to position, rotate around Y, then
        // scale/offset so the sprite is centered at origin.
        glm::mat4 M(1.0f);
        M = glm::translate(M, renderPos);
        M = glm::rotate(M, renderYaw, glm::vec3(0.0f, 1.0f, 0.0f));

        // The sprite spans [-halfSize, +halfSize] on X and Y, [-halfThick, +halfThick] on Z.
        float hs = spriteSize * 0.5f;
        float ht = thickness * 0.5f;

        glm::mat3 N = glm::mat3(glm::rotate(glm::mat4(1.0f), renderYaw, glm::vec3(0,1,0)));

        auto xf = [&](const glm::vec3& v) {
            glm::vec4 p = M * glm::vec4(v, 1.0f);
            return glm::vec3(p);
        };

        auto addQuad = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                           glm::vec3 normal,
                           glm::vec2 uv0, glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3) {
            glm::vec3 wn = glm::normalize(N * normal);
            uint32_t base = static_cast<uint32_t>(vertices_.size());
            vertices_.push_back({xf(v0), wn, uv0, 1.0f});
            vertices_.push_back({xf(v1), wn, uv1, 1.0f});
            vertices_.push_back({xf(v2), wn, uv2, 1.0f});
            vertices_.push_back({xf(v3), wn, uv3, 1.0f});
            indices_.push_back(base + 0);
            indices_.push_back(base + 1);
            indices_.push_back(base + 2);
            indices_.push_back(base + 0);
            indices_.push_back(base + 2);
            indices_.push_back(base + 3);
        };

        // 1 pixel in UV space (for edge strip sampling)
        float pixelU = (uMax - uMin) / 16.0f;
        float pixelV = (vMax - vMin) / 16.0f;

        // --- Front face (+Z): full texture ---
        addQuad(
            {-hs, -hs, +ht}, {-hs, +hs, +ht}, {+hs, +hs, +ht}, {+hs, -hs, +ht},
            {0, 0, 1},
            {uMin, vMax}, {uMin, vMin}, {uMax, vMin}, {uMax, vMax}
        );

        // --- Back face (-Z): same texture, UV mirrored horizontally ---
        addQuad(
            {+hs, -hs, -ht}, {+hs, +hs, -ht}, {-hs, +hs, -ht}, {-hs, -hs, -ht},
            {0, 0, -1},
            {uMin, vMax}, {uMin, vMin}, {uMax, vMin}, {uMax, vMax}
        );

        // --- 4 thin edge strips ---
        // Top edge (+Y): sample from top row of texture (1 pixel tall)
        addQuad(
            {-hs, +hs, +ht}, {-hs, +hs, -ht}, {+hs, +hs, -ht}, {+hs, +hs, +ht},
            {0, 1, 0},
            {uMin, vMin + pixelV}, {uMin, vMin}, {uMax, vMin}, {uMax, vMin + pixelV}
        );

        // Bottom edge (-Y): sample from bottom row of texture
        addQuad(
            {-hs, -hs, -ht}, {-hs, -hs, +ht}, {+hs, -hs, +ht}, {+hs, -hs, -ht},
            {0, -1, 0},
            {uMin, vMax}, {uMin, vMax - pixelV}, {uMax, vMax - pixelV}, {uMax, vMax}
        );

        // Right edge (+X): sample from right column of texture (1 pixel wide)
        addQuad(
            {+hs, -hs, +ht}, {+hs, +hs, +ht}, {+hs, +hs, -ht}, {+hs, -hs, -ht},
            {1, 0, 0},
            {uMax - pixelU, vMax}, {uMax - pixelU, vMin}, {uMax, vMin}, {uMax, vMax}
        );

        // Left edge (-X): sample from left column of texture
        addQuad(
            {-hs, -hs, -ht}, {-hs, +hs, -ht}, {-hs, +hs, +ht}, {-hs, -hs, +ht},
            {-1, 0, 0},
            {uMin, vMax}, {uMin, vMin}, {uMin + pixelU, vMin}, {uMin + pixelU, vMax}
        );
    }
}

void EntityRenderer::buildFrame(const EntityManager& mgr, float partialTick) {
    vertices_.clear();
    indices_.clear();

    for (const auto& e : mgr.entities()) {
        if (!e || !e->alive) continue;
        if (e->kind() == EntityKind::Item) {
            appendItemMesh(static_cast<const ItemEntity&>(*e), partialTick);
        } else if (e->kind() == EntityKind::Arrow) {
            appendArrowMesh(static_cast<const ArrowEntity&>(*e), partialTick);
        } else if (e->kind() == EntityKind::XPOrb) {
            appendXPOrbMesh(static_cast<const XPOrbEntity&>(*e), partialTick);
        }
    }

    indexCountThisFrame_ = static_cast<uint32_t>(indices_.size());
    if (indexCountThisFrame_ == 0) return;

    ensureCapacity();

    // Persistent-mapped dynamic buffers — no staging, no GPU stall.
    void* vData = engine_->mapBuffer(vertexBuffer_);
    std::memcpy(vData, vertices_.data(), sizeof(Vertex) * vertices_.size());
    engine_->unmapBuffer(vertexBuffer_);

    void* iData = engine_->mapBuffer(indexBuffer_);
    std::memcpy(iData, indices_.data(), sizeof(uint32_t) * indices_.size());
    engine_->unmapBuffer(indexBuffer_);
}

void EntityRenderer::appendArrowMesh(const ArrowEntity& arrow, float partialTick) {
    // 箭矢渲染：一个细长的长方体，朝飞行方向旋转
    // 使用方块纹理图集中的 oak_planks 纹理作为简单的棕色纹理
    // （后续可替换为专用箭矢纹理）

    glm::vec3 renderPos = glm::mix(arrow.prevPosition, arrow.position, partialTick);

    // 插值朝向
    float renderYaw = glm::mix(arrow.prevYaw, arrow.yaw, partialTick);
    float renderPitch = glm::mix(arrow.prevPitch, arrow.pitch, partialTick);

    // 箭矢尺寸：长 0.5 格，宽/高 0.05 格
    float length = 0.5f;
    float thick = 0.05f;

    // 构建变换矩阵：先绕 Y 轴旋转 yaw，再绕 X 轴旋转 pitch
    glm::mat4 M(1.0f);
    M = glm::translate(M, renderPos);
    M = glm::rotate(M, renderYaw, glm::vec3(0.0f, 1.0f, 0.0f));
    M = glm::rotate(M, renderPitch, glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat3 N = glm::mat3(M);

    // 使用 oak_planks 纹理（棕色）
    uint16_t texId = atlas_->getTileIndex("oak_planks");
    glm::vec4 uvRect = atlas_->getTileUV(texId);
    // 只取纹理的一小部分（中心区域）作为箭矢颜色
    float uMid = (uvRect.x + uvRect.z) * 0.5f;
    float vMid = (uvRect.y + uvRect.w) * 0.5f;
    float uHalf = (uvRect.z - uvRect.x) * 0.1f;
    float vHalf = (uvRect.w - uvRect.y) * 0.1f;
    glm::vec2 uv00(uMid - uHalf, vMid + vHalf);
    glm::vec2 uv01(uMid - uHalf, vMid - vHalf);
    glm::vec2 uv10(uMid + uHalf, vMid - vHalf);
    glm::vec2 uv11(uMid + uHalf, vMid + vHalf);

    auto xf = [&](const glm::vec3& v) {
        glm::vec4 p = M * glm::vec4(v, 1.0f);
        return glm::vec3(p);
    };

    auto addQuad = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                       glm::vec3 normal) {
        glm::vec3 wn = glm::normalize(N * normal);
        uint32_t base = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back({xf(v0), wn, uv00, 1.0f});
        vertices_.push_back({xf(v1), wn, uv01, 1.0f});
        vertices_.push_back({xf(v2), wn, uv10, 1.0f});
        vertices_.push_back({xf(v3), wn, uv11, 1.0f});
        indices_.push_back(base + 0);
        indices_.push_back(base + 1);
        indices_.push_back(base + 2);
        indices_.push_back(base + 0);
        indices_.push_back(base + 2);
        indices_.push_back(base + 3);
    };

    float hl = length * 0.5f;
    float ht = thick * 0.5f;

    // 箭矢沿 -Z 方向为前端（飞行方向）
    // 6 个面
    // Top (+Y)
    addQuad({-ht, +ht, +hl}, {-ht, +ht, -hl}, {+ht, +ht, -hl}, {+ht, +ht, +hl}, {0, 1, 0});
    // Bottom (-Y)
    addQuad({-ht, -ht, -hl}, {-ht, -ht, +hl}, {+ht, -ht, +hl}, {+ht, -ht, -hl}, {0, -1, 0});
    // Right (+X)
    addQuad({+ht, -ht, +hl}, {+ht, +ht, +hl}, {+ht, +ht, -hl}, {+ht, -ht, -hl}, {1, 0, 0});
    // Left (-X)
    addQuad({-ht, -ht, -hl}, {-ht, +ht, -hl}, {-ht, +ht, +hl}, {-ht, -ht, +hl}, {-1, 0, 0});
    // Front (-Z) — 箭头
    addQuad({-ht, -ht, -hl}, {-ht, +ht, -hl}, {+ht, +ht, -hl}, {+ht, -ht, -hl}, {0, 0, -1});
    // Back (+Z) — 箭尾
    addQuad({+ht, -ht, +hl}, {+ht, +ht, +hl}, {-ht, +ht, +hl}, {-ht, -ht, +hl}, {0, 0, 1});
}

void EntityRenderer::appendXPOrbMesh(const XPOrbEntity& orb, float partialTick) {
    // 经验球渲染：一个小的发光立方体，带旋转动画
    // 使用 emerald_block 纹理的中心区域作为绿色发光效果

    glm::vec3 renderPos = glm::mix(orb.prevPosition, orb.position, partialTick);

    // 浮动动画
    float bobTime = (static_cast<float>(orb.tickCount) + partialTick) * 0.2f;
    float bobOffset = std::sin(bobTime + orb.visualPhase) * 0.05f;
    renderPos.y += bobOffset;

    // 旋转
    float renderYaw = orb.visualYaw + partialTick * 0.15f;

    // 根据经验值确定大小
    float size = orb.getVisualSize();
    float hs = size * 0.5f;

    // 构建变换矩阵
    glm::mat4 M(1.0f);
    M = glm::translate(M, renderPos);
    M = glm::rotate(M, renderYaw, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat3 N = glm::mat3(M);

    // 使用 emerald_block 纹理中心区域（绿色）
    uint16_t texId = atlas_->getTileIndex("emerald_block");
    if (texId == 0) texId = atlas_->getTileIndex("slime_block");
    glm::vec4 uvRect = atlas_->getTileUV(texId);
    // 取纹理中心小区域
    float uMid = (uvRect.x + uvRect.z) * 0.5f;
    float vMid = (uvRect.y + uvRect.w) * 0.5f;
    float uSpan = (uvRect.z - uvRect.x) * 0.3f;
    float vSpan = (uvRect.w - uvRect.y) * 0.3f;
    glm::vec2 uv00(uMid - uSpan, vMid + vSpan);
    glm::vec2 uv01(uMid - uSpan, vMid - vSpan);
    glm::vec2 uv10(uMid + uSpan, vMid - vSpan);
    glm::vec2 uv11(uMid + uSpan, vMid + vSpan);

    auto xf = [&](const glm::vec3& v) {
        glm::vec4 p = M * glm::vec4(v, 1.0f);
        return glm::vec3(p);
    };

    // 经验球使用更高的光照值（模拟发光效果）
    float light = 1.0f;

    auto addQuad = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                       glm::vec3 normal) {
        glm::vec3 wn = glm::normalize(N * normal);
        uint32_t base = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back({xf(v0), wn, uv00, light});
        vertices_.push_back({xf(v1), wn, uv01, light});
        vertices_.push_back({xf(v2), wn, uv10, light});
        vertices_.push_back({xf(v3), wn, uv11, light});
        indices_.push_back(base + 0);
        indices_.push_back(base + 1);
        indices_.push_back(base + 2);
        indices_.push_back(base + 0);
        indices_.push_back(base + 2);
        indices_.push_back(base + 3);
    };

    // 6 个面的小立方体
    // Top (+Y)
    addQuad({-hs, +hs, +hs}, {-hs, +hs, -hs}, {+hs, +hs, -hs}, {+hs, +hs, +hs}, {0, 1, 0});
    // Bottom (-Y)
    addQuad({-hs, -hs, -hs}, {-hs, -hs, +hs}, {+hs, -hs, +hs}, {+hs, -hs, -hs}, {0, -1, 0});
    // Right (+X)
    addQuad({+hs, -hs, +hs}, {+hs, +hs, +hs}, {+hs, +hs, -hs}, {+hs, -hs, -hs}, {1, 0, 0});
    // Left (-X)
    addQuad({-hs, -hs, -hs}, {-hs, +hs, -hs}, {-hs, +hs, +hs}, {-hs, -hs, +hs}, {-1, 0, 0});
    // Front (+Z)
    addQuad({-hs, -hs, +hs}, {-hs, +hs, +hs}, {+hs, +hs, +hs}, {+hs, -hs, +hs}, {0, 0, 1});
    // Back (-Z)
    addQuad({+hs, -hs, -hs}, {+hs, +hs, -hs}, {-hs, +hs, -hs}, {-hs, -hs, -hs}, {0, 0, -1});
}

void EntityRenderer::render(VkCommandBuffer cmd) {
    if (indexCountThisFrame_ == 0) return;

    // Assumes caller already bound the 3D pipeline + the block-atlas descriptor
    // set (chunk rendering does this right before). We only need to swap vertex/
    // index buffers.
    VkBuffer vb[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCountThisFrame_, 1, 0, 0, 0);
}
