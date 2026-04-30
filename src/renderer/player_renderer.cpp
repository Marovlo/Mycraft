#include "player_renderer.h"
#include "texture_atlas.h"
#include "player/player.h"
#include "player/inventory.h"
#include "core/item.h"
#include "core/block.h"
#include "world/day_night_cycle.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stb_image.h>

// Steve 皮肤纹理尺寸
static constexpr int SKIN_TEX_W = 64;
static constexpr int SKIN_TEX_H = 64;

// Steve 右臂 UV 参数（wide 模型，4像素宽手臂）
// 右臂在 64x64 皮肤中的位置: UV(40, 16), size 4x12x4
static constexpr int ARM_UV_X = 40;
static constexpr int ARM_UV_Y = 16;
static constexpr int ARM_SX = 4;   // 宽
static constexpr int ARM_SY = 12;  // 高
static constexpr int ARM_SZ = 4;   // 深

// ========== 初始化/销毁 ==========

void PlayerRenderer::init(VulkanEngine* engine, const TextureAtlas* blockAtlas) {
    engine_ = engine;
    blockAtlas_ = blockAtlas;

    constexpr size_t INITIAL_VERTS = 256;
    constexpr size_t INITIAL_INDS  = 384;
    vertexBufferSize_ = sizeof(Vertex) * INITIAL_VERTS;
    indexBufferSize_  = sizeof(uint32_t) * INITIAL_INDS;
    vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indexBuffer_  = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void PlayerRenderer::destroy() {
    if (!engine_) return;
    if (vertexBuffer_.allocation) {
        vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        vertexBuffer_ = {};
    }
    if (indexBuffer_.allocation) {
        vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        indexBuffer_ = {};
    }
    if (skinImage_.allocation) {
        engine_->destroyTexture(skinImage_);
        skinImage_ = {};
    }
    engine_ = nullptr;
}

// ========== 皮肤纹理加载 ==========

bool PlayerRenderer::loadSkinTexture(VulkanEngine& engine, const std::string& skinPath) {
    // 使用 stbi 加载皮肤 PNG
    int w, h, ch;
    uint8_t* data = stbi_load(skinPath.c_str(), &w, &h, &ch, 4);
    if (!data) {
        std::cerr << "PlayerRenderer: failed to load skin: " << skinPath << "\n";
        return false;
    }

    // 上传到 GPU 纹理
    skinImage_ = engine.uploadTexture(data, w, h, 4);
    stbi_image_free(data);

    if (!skinImage_.allocation) {
        std::cerr << "PlayerRenderer: failed to upload skin texture\n";
        return false;
    }

    // 为每帧分配独立的 descriptor set（绑定皮肤纹理）
    for (int i = 0; i < 2; i++) {
        descriptorSets_[i] = engine.allocateExtraDescriptorSet(
            skinImage_.imageView, engine.getDefaultSampler());
        if (descriptorSets_[i] == VK_NULL_HANDLE) {
            std::cerr << "PlayerRenderer: failed to allocate descriptor set for frame " << i << "\n";
            return false;
        }
    }

    std::cout << "[PlayerRenderer] Loaded skin: " << skinPath
              << " (" << w << "x" << h << ")\n";
    return true;
}

// ========== 容量管理 ==========

void PlayerRenderer::ensureCapacity() {
    VkDeviceSize neededV = sizeof(Vertex) * vertices_.size();
    VkDeviceSize neededI = sizeof(uint32_t) * indices_.size();

    if (neededV > vertexBufferSize_) {
        if (vertexBuffer_.allocation)
            vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        vertexBufferSize_ = neededV * 2;
        vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
    if (neededI > indexBufferSize_) {
        if (indexBuffer_.allocation)
            vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        indexBufferSize_ = neededI * 2;
        indexBuffer_ = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }
}

// ========== 辅助函数 ==========

void PlayerRenderer::addFace(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                              glm::vec3 normal, float u0, float v0uv, float u1, float v1uv, float light) {
    uint32_t base = static_cast<uint32_t>(vertices_.size());
    vertices_.push_back({v0, normal, {u0, v1uv}, light});
    vertices_.push_back({v1, normal, {u0, v0uv}, light});
    vertices_.push_back({v2, normal, {u1, v0uv}, light});
    vertices_.push_back({v3, normal, {u1, v1uv}, light});
    indices_.push_back(base + 0);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);
    indices_.push_back(base + 0);
    indices_.push_back(base + 2);
    indices_.push_back(base + 3);
}

// ========== 手臂长方体渲染 ==========

void PlayerRenderer::addArmCuboid(const ArmCuboid& cuboid, const glm::mat4& transform,
                                   float light, int texW, int texH) {
    float x0 = cuboid.origin.x;
    float y0 = cuboid.origin.y;
    float z0 = cuboid.origin.z;
    float sx = cuboid.size.x;
    float sy = cuboid.size.y;
    float sz = cuboid.size.z;

    // 8 个顶点
    glm::vec3 corners[8] = {
        {x0,      y0,      z0},
        {x0 + sx, y0,      z0},
        {x0 + sx, y0 + sy, z0},
        {x0,      y0 + sy, z0},
        {x0,      y0,      z0 + sz},
        {x0 + sx, y0,      z0 + sz},
        {x0 + sx, y0 + sy, z0 + sz},
        {x0,      y0 + sy, z0 + sz},
    };

    // 变换到 viewmodel 空间
    glm::vec3 wc[8];
    for (int i = 0; i < 8; i++) {
        glm::vec4 p = transform * glm::vec4(corners[i], 1.0f);
        wc[i] = glm::vec3(p);
    }

    int uvX = cuboid.uvX;
    int uvY = cuboid.uvY;
    int isx = static_cast<int>(sx);
    int isy = static_cast<int>(sy);
    int isz = static_cast<int>(sz);

    float uPixel = 1.0f / texW;
    float vPixel = 1.0f / texH;

    // MC 标准 UV 展开
    struct FaceUV { float u0, v0, u1, v1; };

    FaceUV top = {
        (uvX + isz) * uPixel, uvY * vPixel,
        (uvX + isz + isx) * uPixel, (uvY + isz) * vPixel
    };
    FaceUV bottom = {
        (uvX + isz + isx) * uPixel, uvY * vPixel,
        (uvX + isz + isx + isx) * uPixel, (uvY + isz) * vPixel
    };
    FaceUV front = {
        (uvX + isz) * uPixel, (uvY + isz) * vPixel,
        (uvX + isz + isx) * uPixel, (uvY + isz + isy) * vPixel
    };
    FaceUV back = {
        (uvX + isz + isx + isz) * uPixel, (uvY + isz) * vPixel,
        (uvX + isz + isx + isz + isx) * uPixel, (uvY + isz + isy) * vPixel
    };
    FaceUV leftF = {
        uvX * uPixel, (uvY + isz) * vPixel,
        (uvX + isz) * uPixel, (uvY + isz + isy) * vPixel
    };
    FaceUV rightF = {
        (uvX + isz + isx) * uPixel, (uvY + isz) * vPixel,
        (uvX + isz + isx + isz) * uPixel, (uvY + isz + isy) * vPixel
    };

    glm::mat3 N = glm::mat3(transform);

    // +Y (top)
    addFace(wc[7], wc[6], wc[2], wc[3], glm::normalize(N * glm::vec3(0, 1, 0)),
            top.u0, top.v0, top.u1, top.v1, light);
    // -Y (bottom)
    addFace(wc[0], wc[1], wc[5], wc[4], glm::normalize(N * glm::vec3(0, -1, 0)),
            bottom.u0, bottom.v0, bottom.u1, bottom.v1, light);
    // -Z (front)
    addFace(wc[0], wc[3], wc[2], wc[1], glm::normalize(N * glm::vec3(0, 0, -1)),
            front.u0, front.v0, front.u1, front.v1, light);
    // +Z (back)
    addFace(wc[5], wc[6], wc[7], wc[4], glm::normalize(N * glm::vec3(0, 0, 1)),
            back.u0, back.v0, back.u1, back.v1, light);
    // -X (left)
    addFace(wc[4], wc[7], wc[3], wc[0], glm::normalize(N * glm::vec3(-1, 0, 0)),
            leftF.u0, leftF.v0, leftF.u1, leftF.v1, light);
    // +X (right)
    addFace(wc[1], wc[2], wc[6], wc[5], glm::normalize(N * glm::vec3(1, 0, 0)),
            rightF.u0, rightF.v0, rightF.u1, rightF.v1, light);
}

// ========== 手持方块渲染 ==========

void PlayerRenderer::addHeldBlock(BlockId blockId, const glm::mat4& transform, float light) {
    if (!blockAtlas_) return;
    const auto& blockProps = BlockRegistry::instance().get(blockId);

    // 方块 6 面纹理
    auto getUV = [&](Direction dir) -> glm::vec4 {
        uint16_t tile = blockProps.textures.forDirection(dir);
        return blockAtlas_->getTileUV(tile);
    };

    // 小方块尺寸（viewmodel 空间中约 0.4 单位）
    float s = 0.4f;
    glm::vec3 corners[8] = {
        {0, 0, 0}, {s, 0, 0}, {s, s, 0}, {0, s, 0},
        {0, 0, s}, {s, 0, s}, {s, s, s}, {0, s, s},
    };

    glm::vec3 wc[8];
    for (int i = 0; i < 8; i++) {
        glm::vec4 p = transform * glm::vec4(corners[i], 1.0f);
        wc[i] = glm::vec3(p);
    }

    glm::mat3 N = glm::mat3(transform);

    auto drawBlockFace = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                              glm::vec3 norm, Direction dir) {
        glm::vec4 uv = getUV(dir);
        addFace(v0, v1, v2, v3, glm::normalize(N * norm), uv.x, uv.y, uv.z, uv.w, light);
    };

    drawBlockFace(wc[7], wc[6], wc[2], wc[3], {0, 1, 0}, Direction::PosY);
    drawBlockFace(wc[0], wc[1], wc[5], wc[4], {0, -1, 0}, Direction::NegY);
    drawBlockFace(wc[0], wc[3], wc[2], wc[1], {0, 0, -1}, Direction::NegZ);
    drawBlockFace(wc[5], wc[6], wc[7], wc[4], {0, 0, 1}, Direction::PosZ);
    drawBlockFace(wc[4], wc[7], wc[3], wc[0], {-1, 0, 0}, Direction::NegX);
    drawBlockFace(wc[1], wc[2], wc[6], wc[5], {1, 0, 0}, Direction::PosX);
}

// ========== 手持物品 3D 挤出模型渲染 ==========
// MC 原版：工具/食物在手中是每个像素都有 1px 厚度的 3D 模型（浮雕效果）
// 正面和背面使用原始纹理 UV，侧面使用边缘像素的颜色

void PlayerRenderer::addHeldItem3D(const std::string& tileName, const glm::mat4& transform, float light) {
    if (!blockAtlas_ || tileName.empty()) return;

    uint16_t tileIdx = blockAtlas_->getTileIndex(tileName);
    glm::vec4 tileUV = blockAtlas_->getTileUV(tileIdx);

    // 获取像素数据
    const auto& cpuPixels = blockAtlas_->getCpuPixels();
    uint32_t tileSize = blockAtlas_->getTileSize();
    uint32_t atlasSize = blockAtlas_->getAtlasPixelSize();

    if (cpuPixels.empty() || atlasSize == 0) {
        // 回退：如果没有 CPU 像素数据，渲染简单的正反面
        float w = 0.5f, h = 0.5f;
        glm::vec3 v0 = glm::vec3(transform * glm::vec4(0, 0, 0, 1));
        glm::vec3 v1 = glm::vec3(transform * glm::vec4(0, h, 0, 1));
        glm::vec3 v2 = glm::vec3(transform * glm::vec4(w, h, 0, 1));
        glm::vec3 v3 = glm::vec3(transform * glm::vec4(w, 0, 0, 1));
        glm::vec3 normal = glm::normalize(glm::mat3(transform) * glm::vec3(0, 0, -1));
        addFace(v0, v1, v2, v3, normal, tileUV.x, tileUV.y, tileUV.z, tileUV.w, light);
        return;
    }

    // 使用缓存的像素列表（避免每帧重新扫描 256 像素）
    const auto& pixels = getOrBuildPixelCache(tileIdx);

    // 每个像素在 viewmodel 空间中的尺寸
    float pixelScale = 0.5f / static_cast<float>(tileSize);
    float depth = pixelScale;

    // UV 每像素步长
    float uStep = (tileUV.z - tileUV.x) / static_cast<float>(tileSize);
    float vStep = (tileUV.w - tileUV.y) / static_cast<float>(tileSize);

    // 预计算变换矩阵的列向量（避免每像素做完整矩阵乘法）
    // transform * vec4(x, y, z, 1) = col0*x + col1*y + col2*z + col3
    glm::vec3 col0 = glm::vec3(transform[0]);
    glm::vec3 col1 = glm::vec3(transform[1]);
    glm::vec3 col2 = glm::vec3(transform[2]);
    glm::vec3 col3 = glm::vec3(transform[3]);

    glm::mat3 N = glm::mat3(transform);
    glm::vec3 frontNormal = glm::normalize(N * glm::vec3(0, 0, -1));
    glm::vec3 backNormal = glm::normalize(N * glm::vec3(0, 0, 1));
    glm::vec3 leftNormal = glm::normalize(N * glm::vec3(-1, 0, 0));
    glm::vec3 rightNormal = glm::normalize(N * glm::vec3(1, 0, 0));
    glm::vec3 upNormal = glm::normalize(N * glm::vec3(0, 1, 0));
    glm::vec3 downNormal = glm::normalize(N * glm::vec3(0, -1, 0));

    // 预计算 depth 偏移向量
    glm::vec3 depthOffset = col2 * depth;

    for (const auto& pi : pixels) {
        float x0 = static_cast<float>(pi.px) * pixelScale;
        float y0 = static_cast<float>(tileSize - 1 - pi.py) * pixelScale;
        float x1 = x0 + pixelScale;
        float y1 = y0 + pixelScale;

        // UV 坐标
        float u0 = tileUV.x + static_cast<float>(pi.px) * uStep;
        float v0uv = tileUV.y + static_cast<float>(pi.py) * vStep;
        float u1 = u0 + uStep;
        float v1uv = v0uv + vStep;

        // 使用列向量计算顶点位置（比矩阵乘法快 ~3x）
        glm::vec3 p00 = col0 * x0 + col1 * y0 + col3;  // (x0, y0, 0)
        glm::vec3 p01 = col0 * x0 + col1 * y1 + col3;  // (x0, y1, 0)
        glm::vec3 p10 = col0 * x1 + col1 * y0 + col3;  // (x1, y0, 0)
        glm::vec3 p11 = col0 * x1 + col1 * y1 + col3;  // (x1, y1, 0)

        // 正面 (z = 0)
        addFace(p00, p01, p11, p10, frontNormal, u0, v0uv, u1, v1uv, light);

        // 背面 (z = depth)
        glm::vec3 bp00 = p00 + depthOffset;
        glm::vec3 bp01 = p01 + depthOffset;
        glm::vec3 bp10 = p10 + depthOffset;
        glm::vec3 bp11 = p11 + depthOffset;
        addFace(bp10, bp11, bp01, bp00, backNormal, u1, v0uv, u0, v1uv, light);

        // 侧面（使用缓存的 sideFlags）
        if (pi.sideFlags & 0x01) {  // 左侧 (-X)
            addFace(bp00, bp01, p01, p00, leftNormal, u0, v0uv, u0 + uStep * 0.5f, v1uv, light * 0.8f);
        }
        if (pi.sideFlags & 0x02) {  // 右侧 (+X)
            addFace(p10, p11, bp11, bp10, rightNormal, u1 - uStep * 0.5f, v0uv, u1, v1uv, light * 0.8f);
        }
        if (pi.sideFlags & 0x04) {  // 上侧 (+Y)
            addFace(p01, bp01, bp11, p11, upNormal, u0, v0uv, u1, v0uv + vStep * 0.5f, light * 0.9f);
        }
        if (pi.sideFlags & 0x08) {  // 下侧 (-Y)
            addFace(bp00, p00, p10, bp10, downNormal, u0, v1uv - vStep * 0.5f, u1, v1uv, light * 0.9f);
        }
    }
}

const std::vector<PlayerRenderer::PixelInfo>& PlayerRenderer::getOrBuildPixelCache(uint16_t tileIdx) {
    auto it = itemPixelCache_.find(tileIdx);
    if (it != itemPixelCache_.end()) return it->second;

    // 首次遇到此 tile：扫描像素并缓存
    std::vector<PixelInfo>& cache = itemPixelCache_[tileIdx];

    const auto& cpuPixels = blockAtlas_->getCpuPixels();
    uint32_t tileSize = blockAtlas_->getTileSize();
    uint32_t atlasSize = blockAtlas_->getAtlasPixelSize();
    uint32_t tilesPerRow = blockAtlas_->getTilesPerRow();

    uint32_t tileCol = tileIdx % tilesPerRow;
    uint32_t tileRow = tileIdx / tilesPerRow;
    uint32_t tilePixelX = tileCol * tileSize;
    uint32_t tilePixelY = tileRow * tileSize;

    auto getAlpha = [&](int px, int py) -> uint8_t {
        if (px < 0 || py < 0 || px >= (int)tileSize || py >= (int)tileSize) return 0;
        uint32_t ax = tilePixelX + px;
        uint32_t ay = tilePixelY + py;
        uint32_t idx = (ay * atlasSize + ax) * 4 + 3;
        if (idx >= cpuPixels.size()) return 0;
        return cpuPixels[idx];
    };

    for (uint32_t py = 0; py < tileSize; py++) {
        for (uint32_t px = 0; px < tileSize; px++) {
            if (getAlpha(px, py) < 128) continue;

            uint8_t flags = 0;
            if (getAlpha(px - 1, py) < 128) flags |= 0x01;  // 左侧
            if (getAlpha(px + 1, py) < 128) flags |= 0x02;  // 右侧
            if (getAlpha(px, py - 1) < 128) flags |= 0x04;  // 上侧
            if (getAlpha(px, py + 1) < 128) flags |= 0x08;  // 下侧

            cache.push_back({static_cast<uint8_t>(px), static_cast<uint8_t>(py), flags});
        }
    }

    return cache;
}

// ========== 每帧构建 ==========

void PlayerRenderer::buildFrame(const Player& player, const Inventory& inventory,
                                 float partialTick, const DayNightCycle* dayNight) {
    vertices_.clear();
    indices_.clear();

    if (player.dead) {
        armIndexCount_  = 0;
        itemIndexCount_ = 0;
        return;
    }

    float skyLight = dayNight ? dayNight->getSkyLightFactor() : 1.0f;
    float light = skyLight;

    // 获取手持物品
    const ItemStack& held = inventory.getSlot(inventory.getSelectedSlot());

    // ===== 计算 viewmodel 变换矩阵 =====
    // MC 原版 viewmodel 的基础位置（右下角，相对于相机）
    // 这些坐标在 viewmodel 空间中（相机空间，右手坐标系）
    float baseX = 0.56f;    // 右偏
    float baseY = -0.52f;   // 下偏
    float baseZ = -0.72f;   // 前方

    // ===== 挥动动画 (swing) =====
    // MC 原版 swing 动画：手臂绕肩膀旋转，同时向前伸出
    float swingProg = glm::mix(player.prevSwingProgress, player.swingProgress, partialTick);
    float swingAngle = 0.0f;
    float swingTransX = 0.0f;
    float swingTransY = 0.0f;
    float swingTransZ = 0.0f;

    if (swingProg > 0.0f) {
        // MC 原版 swing 曲线：sin(sqrt(progress) * π) 用于主旋转
        float sqrtProg = std::sqrt(swingProg);
        float sinProg = std::sin(sqrtProg * 3.14159265f);
        float sinProg2 = std::sin(swingProg * swingProg * 3.14159265f);

        // 手臂向前伸出 + 向下挥
        swingTransX = -sinProg2 * 0.4f;
        swingTransY = std::sin(sqrtProg * 3.14159265f * 2.0f) * -0.2f;
        swingTransZ = -sinProg * 0.2f;

        // 旋转角度
        swingAngle = sinProg * -1.2f;  // 绕 Y 轴旋转（向左挥）
    }

    // ===== 吃东西动画 =====
    float eatTransX = 0.0f;
    float eatTransY = 0.0f;
    float eatTransZ = 0.0f;
    float eatRotation = 0.0f;

    if (player.isEating && !held.isEmpty()) {
        // MC 原版吃东西动画：物品反复靠近嘴巴 + 随机旋转抖动
        float eatProg = static_cast<float>(player.eatingTicks) / 32.0f;
        float bobFreq = static_cast<float>(player.eatingTicks) + partialTick;
        float bob = std::sin(bobFreq * 1.5f) * 0.1f;

        // 物品向上靠近嘴巴（MC 原版：物品移向屏幕中心偏上）
        eatTransX = -0.4f * eatProg;
        eatTransY = 0.25f * eatProg + bob * 0.5f;
        eatTransZ = -0.15f * eatProg;

        // MC 原版：吃东西时物品有随机旋转抖动
        // 使用 tick 数作为伪随机种子，产生抖动效果
        float jitterX = std::sin(bobFreq * 3.7f) * 0.15f * eatProg;
        float jitterY = std::cos(bobFreq * 2.9f) * 0.1f * eatProg;
        eatRotation = jitterX + jitterY;
    }

    // ===== 拉弓动画（MC 原版） =====
    // 拉弓时手臂移到屏幕中央偏左，弓拉满时有轻微抖动
    float bowTransX = 0.0f;
    float bowTransY = 0.0f;
    float bowTransZ = 0.0f;
    float bowRotation = 0.0f;
    bool isBowCharging = player.isChargingBow && held.id == Item::Bow;

    if (isBowCharging) {
        float chargeRatio = player.getBowChargeRatio();

        // MC 原版：拉弓时手臂移到屏幕中央（向左+向上）
        bowTransX = -0.3f * chargeRatio;   // 向左移动
        bowTransY = 0.2f * chargeRatio;    // 向上抬起
        bowTransZ = -0.1f * chargeRatio;   // 略微前伸

        // MC 原版：满蓄力时有轻微抖动（表示张力）
        if (chargeRatio >= 0.9f) {
            float trembleFreq = static_cast<float>(player.bowChargeTicks) + partialTick;
            bowTransX += std::sin(trembleFreq * 7.0f) * 0.005f;
            bowTransY += std::cos(trembleFreq * 8.3f) * 0.005f;
        }

        // 弓的旋转：拉弓时弓稍微向左旋转
        bowRotation = -0.3f * chargeRatio;
    }

    // ===== 组合变换 =====
    // viewmodel 的顶点需要在世界空间中生成（因为着色器使用 proj * view * model * pos）
    // 策略：先在相机局部空间中定义 viewmodel，然后用 view 矩阵的逆矩阵转换到世界空间
    glm::mat4 invView = glm::inverse(player.getViewMatrix());

    glm::mat4 viewmodel = invView;

    // 基础位置（相机空间中的偏移）
    viewmodel = viewmodel * glm::translate(glm::mat4(1.0f), glm::vec3(
        baseX + swingTransX + eatTransX + bowTransX,
        baseY + swingTransY + eatTransY + bowTransY,
        baseZ + swingTransZ + eatTransZ + bowTransZ
    ));

    // 挥动旋转（在相机空间中）
    if (swingAngle != 0.0f) {
        viewmodel = viewmodel * glm::rotate(glm::mat4(1.0f), swingAngle, glm::vec3(0, 1, 0));
        viewmodel = viewmodel * glm::rotate(glm::mat4(1.0f), swingAngle * 0.5f, glm::vec3(1, 0, 0));
    }

    // 吃东西旋转（在相机空间中）
    if (eatRotation != 0.0f) {
        viewmodel = viewmodel * glm::rotate(glm::mat4(1.0f), eatRotation, glm::vec3(0, 0, 1));
    }

    // 拉弓旋转（在相机空间中）
    if (bowRotation != 0.0f) {
        viewmodel = viewmodel * glm::rotate(glm::mat4(1.0f), bowRotation, glm::vec3(0, 1, 0));
    }

    // ===== 渲染手臂 =====
    armIndexStart_ = 0;
    if (hasSkinTexture()) {
        // 手臂缩放：像素坐标 → viewmodel 空间（1/16 缩放）
        float armScale = 1.0f / 16.0f;
        glm::mat4 armTransform = viewmodel;
        armTransform = armTransform * glm::scale(glm::mat4(1.0f), glm::vec3(armScale));

        // 手臂原点：肩膀在 viewmodel 空间的右上方
        // 手臂从肩膀向下延伸 12 像素
        ArmCuboid rightArm = {{-2, -12, -2}, {4, 12, 4}, ARM_UV_X, ARM_UV_Y};
        addArmCuboid(rightArm, armTransform, light, SKIN_TEX_W, SKIN_TEX_H);
    }
    armIndexCount_ = static_cast<uint32_t>(indices_.size()) - armIndexStart_;

    // ===== 渲染手持物品 =====
    itemIndexStart_ = static_cast<uint32_t>(indices_.size());
    if (!held.isEmpty()) {
        const auto& itemProps = ItemRegistry::instance().get(held.id);

        // 物品位置：在手臂末端（手的位置）
        glm::mat4 itemTransform = viewmodel;
        // 向下偏移到手的位置
        itemTransform = glm::translate(itemTransform, glm::vec3(0.0f, -0.65f, 0.0f));

        bool isBlock = (itemProps.type == ItemType::Block && itemProps.blockId > 0
                        && BlockRegistry::instance().get(itemProps.blockId).renderType != BlockRenderType::Cross);

        if (isBlock) {
            // 方块物品：渲染小方块
            glm::mat4 blockTransform = itemTransform;
            // 方块稍微旋转使其看起来更自然
            blockTransform = glm::rotate(blockTransform, glm::radians(45.0f), glm::vec3(0, 1, 0));
            blockTransform = glm::translate(blockTransform, glm::vec3(-0.2f, -0.1f, -0.2f));
            addHeldBlock(itemProps.blockId, blockTransform, light);
        } else if (!itemProps.iconTileName.empty()) {
            // 工具/食物/材料：渲染 3D 挤出模型（MC 原版：每个像素都有厚度）
            glm::mat4 spriteTransform = itemTransform;

            // MC 原版：弓在拉弓时使用不同的蓄力贴图
            std::string tileName = itemProps.iconTileName;
            if (isBowCharging && held.id == Item::Bow) {
                float chargeRatio = player.getBowChargeRatio();
                // MC 原版弓蓄力贴图切换：
                // 0~0.33: bow_pulling_0
                // 0.33~0.66: bow_pulling_1
                // 0.66~1.0: bow_pulling_2
                if (chargeRatio >= 0.66f) {
                    tileName = "item_bow_pulling_2";
                } else if (chargeRatio >= 0.33f) {
                    tileName = "item_bow_pulling_1";
                } else {
                    tileName = "item_bow_pulling_0";
                }
            }

            // MC 原版工具在手中是斜着拿的（约 -45° 绕 Z 轴 + 30° 绕 Y 轴）
            spriteTransform = glm::rotate(spriteTransform, glm::radians(-45.0f), glm::vec3(0, 0, 1));
            spriteTransform = glm::rotate(spriteTransform, glm::radians(30.0f), glm::vec3(0, 1, 0));
            spriteTransform = glm::translate(spriteTransform, glm::vec3(-0.1f, -0.15f, 0.0f));
            addHeldItem3D(tileName, spriteTransform, light);
        }
    }
    itemIndexCount_ = static_cast<uint32_t>(indices_.size()) - itemIndexStart_;

    if (indices_.empty()) return;

    ensureCapacity();

    // 更新 descriptor set 的 UBO 绑定
    if (hasSkinTexture()) {
        int frameIdx = engine_->getCurrentFrameIndex();
        auto& frame = engine_->getCurrentFrame();
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = frame.uniformBuffer.buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet uboWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        uboWrite.dstSet = descriptorSets_[frameIdx];
        uboWrite.dstBinding = 0;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(engine_->getDevice(), 1, &uboWrite, 0, nullptr);
    }

    // 上传到 GPU
    void* vData = engine_->mapBuffer(vertexBuffer_);
    std::memcpy(vData, vertices_.data(), sizeof(Vertex) * vertices_.size());
    engine_->unmapBuffer(vertexBuffer_);

    void* iData = engine_->mapBuffer(indexBuffer_);
    std::memcpy(iData, indices_.data(), sizeof(uint32_t) * indices_.size());
    engine_->unmapBuffer(indexBuffer_);
}

// ========== 渲染 ==========

void PlayerRenderer::render(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout,
                             VkDescriptorSet blockAtlasDescSet) {
    if (armIndexCount_ == 0 && itemIndexCount_ == 0) return;

    VkBuffer vb[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);

    // 第一批：渲染手臂（使用皮肤纹理）
    if (armIndexCount_ > 0 && hasSkinTexture()) {
        int frameIdx = engine_->getCurrentFrameIndex();
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 0, 1, &descriptorSets_[frameIdx], 0, nullptr);
        vkCmdDrawIndexed(cmd, armIndexCount_, 1, armIndexStart_, 0, 0);
    }

    // 第二批：渲染手持物品（使用方块图集纹理）
    if (itemIndexCount_ > 0 && blockAtlasDescSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 0, 1, &blockAtlasDescSet, 0, nullptr);
        vkCmdDrawIndexed(cmd, itemIndexCount_, 1, itemIndexStart_, 0, 0);
    }
}
