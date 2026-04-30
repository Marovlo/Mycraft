#pragma once

#include "engine/vulkan_engine.h"
#include "core/item.h"
#include "core/block.h"
#include <vector>
#include <string>
#include <unordered_map>

class Player;
class Inventory;
class TextureAtlas;
class DayNightCycle;

// 第一人称 viewmodel 渲染器：渲染玩家右手臂 + 手持物品
// MC 原版实现方式：
// - 使用独立的投影矩阵（更大 FOV，更近的近裁面）
// - 在所有世界几何体之后渲染，清除深度缓冲后再画
// - 手臂使用 Steve 皮肤纹理的右臂 UV 区域
// - 物品：方块→小方块，工具/食物→扁平面片
// - 动画：挥动（swing）、吃东西、方块放置
class PlayerRenderer {
public:
    void init(VulkanEngine* engine, const TextureAtlas* blockAtlas);
    void destroy();

    // 加载玩家皮肤纹理（Steve 64x64）
    bool loadSkinTexture(VulkanEngine& engine, const std::string& skinPath);

    // 每帧构建 viewmodel mesh
    // partialTick: 0~1 帧间插值因子
    void buildFrame(const Player& player, const Inventory& inventory,
                    float partialTick, const DayNightCycle* dayNight);

    // 渲染 viewmodel（在世界渲染之后、UI 之前调用）
    // 分两批：手臂用皮肤纹理，手持物品用方块图集
    // blockAtlasDescSet: 方块图集的 descriptor set（用于渲染手持物品）
    void render(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout,
                VkDescriptorSet blockAtlasDescSet);

    bool hasContent() const { return armIndexCount_ > 0 || itemIndexCount_ > 0; }
    bool hasSkinTexture() const { return skinImage_.allocation != nullptr; }

    // 获取 viewmodel 专用 descriptor set
    VkDescriptorSet getDescriptorSet(int frameIndex) const { return descriptorSets_[frameIndex]; }

private:
    VulkanEngine* engine_ = nullptr;
    const TextureAtlas* blockAtlas_ = nullptr;

    // 玩家皮肤纹理
    AllocatedImage skinImage_;

    // Descriptor sets（绑定皮肤纹理 + UBO）
    VkDescriptorSet descriptorSets_[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    // CPU staging
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    // GPU buffers
    AllocatedBuffer vertexBuffer_;
    AllocatedBuffer indexBuffer_;
    VkDeviceSize    vertexBufferSize_ = 0;
    VkDeviceSize    indexBufferSize_  = 0;
    uint32_t        armIndexCount_  = 0;   // 手臂部分的索引数量（使用皮肤纹理）
    uint32_t        itemIndexCount_ = 0;   // 物品部分的索引数量（使用方块图集）
    uint32_t        armIndexStart_  = 0;   // 手臂索引起始位置
    uint32_t        itemIndexStart_ = 0;   // 物品索引起始位置

    void ensureCapacity();

    // ===== 手臂模型 =====
    // Steve 右臂 UV 布局（64x64 皮肤）：
    // 右臂: UV(40, 16), size 4x12x4 (宽x高x深)
    // 右臂外层: UV(40, 32), size 4x12x4 (overlay, 暂不实现)
    struct ArmCuboid {
        glm::vec3 origin;
        glm::vec3 size;
        int uvX, uvY;
    };

    // 渲染手臂的长方体
    void addArmCuboid(const ArmCuboid& cuboid, const glm::mat4& transform,
                      float light, int texW, int texH);

    // 渲染手持方块（小方块）
    void addHeldBlock(BlockId blockId, const glm::mat4& transform, float light);

    // 渲染手持物品（3D 挤出模型 — MC 原版：每个像素都有 1px 厚度）
    void addHeldItem3D(const std::string& tileName, const glm::mat4& transform, float light);

    // 性能优化：缓存每个 tile 的不透明像素列表和侧面信息
    struct PixelInfo {
        uint8_t px, py;       // 像素坐标
        uint8_t sideFlags;    // bit0=左侧, bit1=右侧, bit2=上侧, bit3=下侧
    };
    std::unordered_map<uint16_t, std::vector<PixelInfo>> itemPixelCache_;
    const std::vector<PixelInfo>& getOrBuildPixelCache(uint16_t tileIdx);

    // 辅助：添加一个面
    void addFace(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                 glm::vec3 normal, float u0, float v0uv, float u1, float v1uv, float light);
};
