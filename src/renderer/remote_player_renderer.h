#pragma once

#include "engine/vulkan_engine.h"
#include "network/client_connection.h"
#include <vector>
#include <string>
#include <unordered_map>

class DayNightCycle;

// ============================================================
// RemotePlayerRenderer - 第三人称玩家模型渲染器
// MC 原版 Steve 模型：头(8x8x8) + 身体(8x12x4) + 双臂(4x12x4) + 双腿(4x12x4)
// 使用 64x64 Steve 皮肤纹理
// 支持行走动画（四肢摆动）、头部朝向
// ============================================================

// Steve 模型的一个长方体部件
struct PlayerCuboid {
    glm::vec3 origin;    // 部件原点（相对于模型中心）
    glm::vec3 size;      // 部件尺寸（宽x高x深）
    glm::vec3 pivot;     // 旋转轴心（关节位置）
    int uvX, uvY;        // 纹理 UV 起始位置（像素坐标）
};

class RemotePlayerRenderer {
public:
    void init(VulkanEngine* engine);
    void destroy();

    // 加载 Steve 皮肤纹理
    bool loadSkinTexture(VulkanEngine& engine, const std::string& skinPath);

    // 每帧重建所有远程玩家的 mesh
    // remotePlayers: 从 ClientConnection 获取的远程玩家列表
    // localPlayerPos: 本地玩家位置（用于距离剔除）
    // partialTick: 帧间插值因子
    void buildFrame(const std::unordered_map<uint32_t, RemotePlayer>& remotePlayers,
                    const glm::vec3& localPlayerPos,
                    float partialTick,
                    const DayNightCycle* dayNight);

    // 构建本地玩家的第三人称模型（F5 视角切换时使用）
    // 将本地玩家数据转换为 RemotePlayer 格式并追加到当前帧的 mesh 中
    void appendLocalPlayer(const glm::vec3& position, const glm::vec3& prevPosition,
                           float yaw, float pitch, bool sneaking,
                           bool isSwingArm, int swingTicks,
                           bool isChargingBow, int bowChargeTicks,
                           bool isEating, int eatingTicks,
                           float partialTick, float skyLightFactor);

    // 渲染所有远程玩家（在不透明管线中调用）
    void render(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout);

    bool hasContent() const { return indexCountThisFrame_ > 0; }
    bool hasSkinTexture() const { return skinImage_.allocation != nullptr; }

private:
    VulkanEngine* engine_ = nullptr;

    // Steve 皮肤纹理
    AllocatedImage skinImage_;
    VkDescriptorSet descriptorSets_[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    // Steve 模型部件定义
    PlayerCuboid head_;
    PlayerCuboid body_;
    PlayerCuboid rightArm_;
    PlayerCuboid leftArm_;
    PlayerCuboid rightLeg_;
    PlayerCuboid leftLeg_;

    void defineModel();

    // CPU staging
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    // GPU buffers
    AllocatedBuffer vertexBuffer_;
    AllocatedBuffer indexBuffer_;
    VkDeviceSize vertexBufferSize_ = 0;
    VkDeviceSize indexBufferSize_ = 0;
    uint32_t indexCountThisFrame_ = 0;

    // 渲染距离限制（方块数）
    static constexpr float MAX_RENDER_DISTANCE = 128.0f;

    // 皮肤纹理尺寸
    static constexpr int SKIN_TEX_W = 64;
    static constexpr int SKIN_TEX_H = 64;

    void ensureCapacity();
    void finishFrame();  // 上传当前帧的 mesh 数据到 GPU

    // 构建单个玩家的 mesh
    void appendPlayerMesh(const RemotePlayer& player, float partialTick, float skyLightFactor);

    // 添加一个长方体部件
    void addCuboid(const PlayerCuboid& cuboid, const glm::mat4& parentTransform, float light);

    // 辅助：添加一个面
    void addFace(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                 glm::vec3 normal, float u0, float v0uv, float u1, float v1uv, float light);

    // 行走动画：根据移动速度计算肢体摆动角度
    float calcLimbSwing(const RemotePlayer& player, float partialTick) const;

    // 用于行走动画的状态追踪
    struct PlayerAnimState {
        float limbSwingAmount = 0.0f;  // 摆动幅度（0~1）
        float limbSwingPos = 0.0f;     // 摆动相位
        double lastUpdateTime = 0.0;
    };
    mutable std::unordered_map<uint32_t, PlayerAnimState> animStates_;
};
