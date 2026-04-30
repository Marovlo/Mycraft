#include "remote_player_renderer.h"
#include "world/day_night_cycle.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <cstring>
#include <cmath>
#include <iostream>
#include <algorithm>

// ============================================================
// MC 原版 Steve 模型定义（64x64 皮肤，wide 模型）
// 参考: https://minecraft.wiki/w/Skin#Model
//
// 坐标系：Y轴向上，模型中心在脚底中央
// 所有尺寸单位为像素（1像素 = 1/16 方块）
// ============================================================

void RemotePlayerRenderer::defineModel() {
    // 头部: 8x8x8, UV(0, 0), 轴心在脖子处
    head_ = {{-4, 24, -4}, {8, 8, 8}, {0, 24, 0}, 0, 0};

    // 身体: 8x12x4, UV(16, 16), 轴心在身体中心
    body_ = {{-4, 12, -2}, {8, 12, 4}, {0, 12, 0}, 16, 16};

    // 右臂: 4x12x4, UV(40, 16), 轴心在肩膀
    // MC 原版：右臂挂在身体右侧外，pivot 在肩膀处(y=22)
    // 手臂从肩膀向下延伸12px: origin.y = 22 - 12 = 10
    // origin.x 使手臂中心对齐 pivot.x（-5）: origin.x = -5 - 2 = -7
    rightArm_ = {{-7, 10, -2}, {4, 12, 4}, {-5, 22, 0}, 40, 16};

    // 左臂: 4x12x4, UV(32, 48), 轴心在肩膀
    // origin.y = 22 - 12 = 10, origin.x = 5 - 2 = 3
    leftArm_ = {{3, 10, -2}, {4, 12, 4}, {5, 22, 0}, 32, 48};

    // 右腿: 4x12x4, UV(0, 16), 轴心在胯部
    // MC 原版：右腿在身体右半侧，pivot.x = -2，origin.x = -4（腿中心在 x=-2）
    rightLeg_ = {{-4, 0, -2}, {4, 12, 4}, {-2, 12, 0}, 0, 16};

    // 左腿: 4x12x4, UV(16, 48), 轴心在胯部
    // MC 原版：左腿在身体左半侧，pivot.x = 2，origin.x = 0（腿中心在 x=2）
    leftLeg_ = {{0, 0, -2}, {4, 12, 4}, {2, 12, 0}, 16, 48};
}

// ============================================================
// 初始化与销毁
// ============================================================

void RemotePlayerRenderer::init(VulkanEngine* engine) {
    engine_ = engine;
    defineModel();

    constexpr size_t INITIAL_VERTS = 1024;
    constexpr size_t INITIAL_INDS = 1536;
    vertexBufferSize_ = sizeof(Vertex) * INITIAL_VERTS;
    indexBufferSize_ = sizeof(uint32_t) * INITIAL_INDS;
    vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indexBuffer_ = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void RemotePlayerRenderer::destroy() {
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

// ============================================================
// 加载皮肤纹理
// ============================================================

bool RemotePlayerRenderer::loadSkinTexture(VulkanEngine& engine, const std::string& skinPath) {
    int w, h, channels;
    unsigned char* pixels = stbi_load(skinPath.c_str(), &w, &h, &channels, 4);
    if (!pixels) {
        std::cerr << "[RemotePlayerRenderer] Failed to load skin: " << skinPath << std::endl;
        return false;
    }

    if (w != SKIN_TEX_W || h != SKIN_TEX_H) {
        std::cerr << "[RemotePlayerRenderer] Skin size mismatch: " << w << "x" << h
                  << " (expected 64x64)" << std::endl;
        stbi_image_free(pixels);
        return false;
    }

    // 上传到 GPU 纹理
    skinImage_ = engine.uploadTexture(pixels, w, h, 4);
    stbi_image_free(pixels);

    if (!skinImage_.allocation) {
        std::cerr << "[RemotePlayerRenderer] Failed to upload skin texture" << std::endl;
        return false;
    }

    // 为每帧分配独立的 descriptor set（绑定皮肤纹理）
    for (int i = 0; i < 2; i++) {
        descriptorSets_[i] = engine.allocateExtraDescriptorSet(
            skinImage_.imageView, engine.getDefaultSampler());
        if (descriptorSets_[i] == VK_NULL_HANDLE) {
            std::cerr << "[RemotePlayerRenderer] Failed to allocate descriptor set for frame " << i << std::endl;
            return false;
        }
    }

    std::cout << "[RemotePlayerRenderer] Loaded skin texture: " << skinPath << std::endl;
    return true;
}

// ============================================================
// 每帧构建 mesh
// ============================================================

void RemotePlayerRenderer::buildFrame(
    const std::unordered_map<uint32_t, RemotePlayer>& remotePlayers,
    const glm::vec3& localPlayerPos,
    float partialTick,
    const DayNightCycle* dayNight)
{
    vertices_.clear();
    indices_.clear();
    indexCountThisFrame_ = 0;

    if (!hasSkinTexture()) return;

    float skyLightFactor = dayNight ? dayNight->getSkyLightFactor() : 1.0f;

    for (const auto& [id, player] : remotePlayers) {
        // 距离剔除
        float dx = player.position.x - localPlayerPos.x;
        float dz = player.position.z - localPlayerPos.z;
        float distSq = dx * dx + dz * dz;
        if (distSq > MAX_RENDER_DISTANCE * MAX_RENDER_DISTANCE) continue;

        appendPlayerMesh(player, partialTick, skyLightFactor);
    }

    // 注意：不在这里上传 GPU，由 finishFrame() 统一上传
    // 这样可以在 buildFrame 之后追加本地玩家的第三人称模型
    finishFrame();
}

void RemotePlayerRenderer::ensureCapacity() {
    VkDeviceSize neededVB = vertices_.size() * sizeof(Vertex);
    VkDeviceSize neededIB = indices_.size() * sizeof(uint32_t);

    if (neededVB > vertexBufferSize_) {
        if (vertexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        }
        vertexBufferSize_ = neededVB * 2;
        vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
    if (neededIB > indexBufferSize_) {
        if (indexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        }
        indexBufferSize_ = neededIB * 2;
        indexBuffer_ = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }
}

void RemotePlayerRenderer::finishFrame() {
    indexCountThisFrame_ = static_cast<uint32_t>(indices_.size());
    if (indexCountThisFrame_ == 0) return;

    ensureCapacity();

    void* data;
    vmaMapMemory(engine_->getAllocator(), vertexBuffer_.allocation, &data);
    std::memcpy(data, vertices_.data(), vertices_.size() * sizeof(Vertex));
    vmaUnmapMemory(engine_->getAllocator(), vertexBuffer_.allocation);

    vmaMapMemory(engine_->getAllocator(), indexBuffer_.allocation, &data);
    std::memcpy(data, indices_.data(), indices_.size() * sizeof(uint32_t));
    vmaUnmapMemory(engine_->getAllocator(), indexBuffer_.allocation);
}

// ============================================================
// 构建单个玩家的 mesh
// ============================================================

void RemotePlayerRenderer::appendPlayerMesh(const RemotePlayer& player, float partialTick, float skyLightFactor) {
    // 插值位置
    glm::vec3 pos = glm::mix(player.prevPosition, player.position, partialTick);
    float light = skyLightFactor;

    // 计算行走动画
    float limbSwing = calcLimbSwing(player, partialTick);

    // MC 原版行走动画：四肢前后摆动，最大角度约 ±40°
    float swingAngle = limbSwing;

    // 模型缩放：1像素 = 1/16 方块
    float scale = 1.0f / 16.0f;

    // 潜行时身体下移 + 前倾
    float sneakOffset = player.isSneaking ? -0.2f * 16.0f : 0.0f;  // 像素单位
    float sneakLean = player.isSneaking ? glm::radians(25.0f) : 0.0f;

    // 基础变换：平移到世界位置 + 旋转朝向
    glm::mat4 baseTransform = glm::mat4(1.0f);
    baseTransform = glm::translate(baseTransform, pos);
    // 潜行时整体下移
    if (player.isSneaking) {
        baseTransform = glm::translate(baseTransform, glm::vec3(0, -0.2f, 0));
    }
    // 玩家朝向：getForward() 使用标准球坐标（yaw=0 面向 +X），
    // 但模型默认面向 -Z，需要额外偏移 90° 使模型朝向与 getForward() 一致
    baseTransform = glm::rotate(baseTransform, glm::radians(-player.yaw - 90.0f), glm::vec3(0, 1, 0));
    baseTransform = glm::scale(baseTransform, glm::vec3(scale));

    // 潜行时身体前倾
    glm::mat4 bodyTransform = baseTransform;
    if (sneakLean != 0.0f) {
        // 绕身体底部前倾
        bodyTransform = glm::translate(bodyTransform, glm::vec3(0, 12, 0));
        bodyTransform = glm::rotate(bodyTransform, sneakLean, glm::vec3(1, 0, 0));
        bodyTransform = glm::translate(bodyTransform, glm::vec3(0, -12, 0));
    }

    // === 身体 ===
    addCuboid(body_, bodyTransform, light);

    // === 头部（跟随 pitch） ===
    {
        glm::mat4 headTransform = bodyTransform;
        headTransform = glm::translate(headTransform, head_.pivot);
        // 头部 pitch 旋转（减去身体前倾角度，保持头部水平看向前方）
        float headPitch = -player.pitch;
        if (sneakLean != 0.0f) headPitch -= sneakLean;
        headTransform = glm::rotate(headTransform, glm::radians(headPitch), glm::vec3(1, 0, 0));
        headTransform = glm::translate(headTransform, -head_.pivot);
        addCuboid(head_, headTransform, light);
    }

    // === 计算手臂动画角度 ===
    float rightArmAngle = swingAngle;
    float leftArmAngle = -swingAngle;

    // 挥臂动画（MC 原版：右臂快速向前挥动）
    if (player.isSwingArm && player.swingTicks < RemotePlayer::SWING_DURATION) {
        float swingProgress = static_cast<float>(player.swingTicks) / static_cast<float>(RemotePlayer::SWING_DURATION);
        // MC 原版挥臂：快速向前再回来（sin 曲线）
        float swingAnim = std::sin(swingProgress * 3.14159f) * glm::radians(-90.0f);
        rightArmAngle = swingAnim;
    }

    // 拉弓动画（MC 原版：双臂举起到水平，指向前方）
    if (player.isChargingBow) {
        float chargeRatio = std::min(static_cast<float>(player.bowChargeTicks) / 20.0f, 1.0f);
        // 右臂举起到水平（-90°）
        rightArmAngle = glm::radians(-90.0f) * chargeRatio;
        // 左臂也举起（辅助手拉弦）
        leftArmAngle = glm::radians(-70.0f) * chargeRatio;
        // 满蓄力时轻微抖动
        if (chargeRatio >= 0.9f) {
            float tremblePhase = static_cast<float>(player.bowChargeTicks) * 7.0f;
            rightArmAngle += std::sin(tremblePhase) * glm::radians(2.0f);
        }
    }

    // 吃东西动画（MC 原版：右臂举起到嘴边）
    if (player.isEating) {
        float eatProgress = std::min(static_cast<float>(player.eatingTicks) / 32.0f, 1.0f);
        // 右臂举起到嘴边（约 -100°）
        float eatAngle = glm::radians(-100.0f) * eatProgress;
        // 吃东西时有轻微上下抖动
        float bobFreq = static_cast<float>(player.eatingTicks) * 1.5f;
        eatAngle += std::sin(bobFreq) * glm::radians(5.0f) * eatProgress;
        rightArmAngle = eatAngle;
    }

    // === 右臂 ===
    {
        glm::mat4 armTransform = bodyTransform;
        armTransform = glm::translate(armTransform, rightArm_.pivot);
        armTransform = glm::rotate(armTransform, rightArmAngle, glm::vec3(1, 0, 0));
        armTransform = glm::translate(armTransform, -rightArm_.pivot);
        addCuboid(rightArm_, armTransform, light);
    }

    // === 左臂 ===
    {
        glm::mat4 armTransform = bodyTransform;
        armTransform = glm::translate(armTransform, leftArm_.pivot);
        armTransform = glm::rotate(armTransform, leftArmAngle, glm::vec3(1, 0, 0));
        armTransform = glm::translate(armTransform, -leftArm_.pivot);
        addCuboid(leftArm_, armTransform, light);
    }

    // === 右腿（行走时前后摆动） ===
    {
        glm::mat4 legTransform = baseTransform;
        legTransform = glm::translate(legTransform, rightLeg_.pivot);
        legTransform = glm::rotate(legTransform, -swingAngle, glm::vec3(1, 0, 0));
        legTransform = glm::translate(legTransform, -rightLeg_.pivot);
        addCuboid(rightLeg_, legTransform, light);
    }

    // === 左腿 ===
    {
        glm::mat4 legTransform = baseTransform;
        legTransform = glm::translate(legTransform, leftLeg_.pivot);
        legTransform = glm::rotate(legTransform, swingAngle, glm::vec3(1, 0, 0));
        legTransform = glm::translate(legTransform, -leftLeg_.pivot);
        addCuboid(leftLeg_, legTransform, light);
    }
}

// ============================================================
// 行走动画计算
// MC 原版：limbSwingAmount 基于移动速度，limbSwing 是累积相位
// ============================================================

float RemotePlayerRenderer::calcLimbSwing(const RemotePlayer& player, float partialTick) const {
    auto& state = animStates_[player.playerId];

    // 计算移动速度（水平分量）
    glm::vec3 delta = player.position - player.prevPosition;
    float speed = std::sqrt(delta.x * delta.x + delta.z * delta.z);

    // MC 原版：limbSwingAmount 平滑趋近于 speed（最大1.0）
    float targetAmount = std::min(speed * 4.0f, 1.0f);
    state.limbSwingAmount += (targetAmount - state.limbSwingAmount) * 0.4f;

    // 累积相位（速度越快摆动越快）
    if (state.limbSwingAmount > 0.01f) {
        state.limbSwingPos += speed * 0.6f;
    }

    // MC 原版行走动画：sin 波形，最大角度约 40°
    float maxAngle = glm::radians(40.0f);
    float swing = std::sin(state.limbSwingPos) * maxAngle * state.limbSwingAmount;

    return swing;
}

// ============================================================
// 添加长方体部件（MC 标准 UV 展开）
// ============================================================

void RemotePlayerRenderer::addCuboid(const PlayerCuboid& cuboid,
                                      const glm::mat4& parentTransform, float light) {
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

    // 变换到世界空间
    glm::vec3 wc[8];
    for (int i = 0; i < 8; i++) {
        glm::vec4 p = parentTransform * glm::vec4(corners[i], 1.0f);
        wc[i] = glm::vec3(p);
    }

    // UV 计算（MC 标准长方体展开）
    float uPixel = 1.0f / static_cast<float>(SKIN_TEX_W);
    float vPixel = 1.0f / static_cast<float>(SKIN_TEX_H);

    int uvX = cuboid.uvX;
    int uvY = cuboid.uvY;
    int isx = static_cast<int>(sx);
    int isy = static_cast<int>(sy);
    int isz = static_cast<int>(sz);

    // MC 标准 UV 展开布局：
    // 顶面: (uvX+sz, uvY) to (uvX+sz+sx, uvY+sz)
    struct FaceUV { float u0, v0, u1, v1; };

    FaceUV top = {
        (uvX + isz) * uPixel, uvY * vPixel,
        (uvX + isz + isx) * uPixel, (uvY + isz) * vPixel
    };
    // 底面: (uvX+sz+sx, uvY) to (uvX+sz+sx+sx, uvY+sz)
    FaceUV bottom = {
        (uvX + isz + isx) * uPixel, uvY * vPixel,
        (uvX + isz + isx + isx) * uPixel, (uvY + isz) * vPixel
    };
    // 正面(-Z): (uvX+sz, uvY+sz) to (uvX+sz+sx, uvY+sz+sy)
    FaceUV front = {
        (uvX + isz) * uPixel, (uvY + isz) * vPixel,
        (uvX + isz + isx) * uPixel, (uvY + isz + isy) * vPixel
    };
    // 背面(+Z): (uvX+sz+sx+sz, uvY+sz) to (uvX+sz+sx+sz+sx, uvY+sz+sy)
    FaceUV back = {
        (uvX + isz + isx + isz) * uPixel, (uvY + isz) * vPixel,
        (uvX + isz + isx + isz + isx) * uPixel, (uvY + isz + isy) * vPixel
    };
    // 左面(-X): (uvX, uvY+sz) to (uvX+sz, uvY+sz+sy)
    FaceUV left = {
        uvX * uPixel, (uvY + isz) * vPixel,
        (uvX + isz) * uPixel, (uvY + isz + isy) * vPixel
    };
    // 右面(+X): (uvX+sz+sx, uvY+sz) to (uvX+sz+sx+sz, uvY+sz+sy)
    FaceUV right = {
        (uvX + isz + isx) * uPixel, (uvY + isz) * vPixel,
        (uvX + isz + isx + isz) * uPixel, (uvY + isz + isy) * vPixel
    };

    glm::mat3 N = glm::mat3(parentTransform);

    // +Y (top): corners 7,6,2,3
    addFace(wc[7], wc[6], wc[2], wc[3], glm::normalize(N * glm::vec3(0, 1, 0)),
            top.u0, top.v0, top.u1, top.v1, light);
    // -Y (bottom): corners 0,1,5,4
    addFace(wc[0], wc[1], wc[5], wc[4], glm::normalize(N * glm::vec3(0, -1, 0)),
            bottom.u0, bottom.v0, bottom.u1, bottom.v1, light);
    // -Z (front): corners 0,3,2,1
    addFace(wc[0], wc[3], wc[2], wc[1], glm::normalize(N * glm::vec3(0, 0, -1)),
            front.u0, front.v0, front.u1, front.v1, light);
    // +Z (back): corners 5,6,7,4
    addFace(wc[5], wc[6], wc[7], wc[4], glm::normalize(N * glm::vec3(0, 0, 1)),
            back.u0, back.v0, back.u1, back.v1, light);
    // -X (left): corners 4,7,3,0
    addFace(wc[4], wc[7], wc[3], wc[0], glm::normalize(N * glm::vec3(-1, 0, 0)),
            left.u0, left.v0, left.u1, left.v1, light);
    // +X (right): corners 1,2,6,5
    addFace(wc[1], wc[2], wc[6], wc[5], glm::normalize(N * glm::vec3(1, 0, 0)),
            right.u0, right.v0, right.u1, right.v1, light);
}

// ============================================================
// 辅助：添加一个面（4 顶点 + 6 索引）
// ============================================================

void RemotePlayerRenderer::addFace(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
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

// ============================================================
// 构建本地玩家的第三人称模型（F5 视角切换时使用）
// ============================================================

void RemotePlayerRenderer::appendLocalPlayer(
    const glm::vec3& position, const glm::vec3& prevPosition,
    float yaw, float pitch, bool sneaking,
    bool isSwingArm, int swingTicks,
    bool isChargingBow, int bowChargeTicks,
    bool isEating, int eatingTicks,
    float partialTick, float skyLightFactor)
{
    // 构造一个临时的 RemotePlayer 来复用 appendPlayerMesh
    RemotePlayer localAsRemote;
    localAsRemote.playerId = 0xFFFFFFFF;  // 特殊 ID 标识本地玩家
    localAsRemote.position = position;
    localAsRemote.prevPosition = prevPosition;
    localAsRemote.yaw = yaw;
    localAsRemote.pitch = pitch;
    localAsRemote.isSneaking = sneaking;
    localAsRemote.isSwingArm = isSwingArm;
    localAsRemote.swingTicks = swingTicks;
    localAsRemote.isChargingBow = isChargingBow;
    localAsRemote.bowChargeTicks = bowChargeTicks;
    localAsRemote.isEating = isEating;
    localAsRemote.eatingTicks = eatingTicks;

    appendPlayerMesh(localAsRemote, partialTick, skyLightFactor);

    // 重新上传到 GPU（覆盖 buildFrame 的上传结果）
    finishFrame();
}

// ============================================================
// 渲染
// ============================================================

void RemotePlayerRenderer::render(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout) {
    if (indexCountThisFrame_ == 0 || !hasSkinTexture()) return;

    // 绑定皮肤纹理 descriptor set
    int frameIdx = engine_->getCurrentFrameIndex();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 0, 1, &descriptorSets_[frameIdx], 0, nullptr);

    VkBuffer vb[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCountThisFrame_, 1, 0, 0, 0);
}
