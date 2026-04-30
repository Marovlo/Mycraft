#pragma once

#include "engine/vulkan_engine.h"
#include "entity/mob_entity.h"
#include <vector>
#include <unordered_map>
#include <string>

class TextureAtlas;
class EntityManager;

// 生物模型的一个长方体部件
struct MobCuboid {
    glm::vec3 origin;    // 部件原点（相对于模型中心）
    glm::vec3 size;      // 部件尺寸（3D空间）
    glm::vec3 pivot;     // 旋转轴心
    int uvX, uvY;        // 纹理 UV 起始位置（像素坐标）
    glm::vec3 uvSize{0}; // UV展开用的尺寸（当与size不同时使用，如身体旋转90度）
                         // 全0表示使用size
};

// 一种生物的完整模型定义
struct MobModelDef {
    MobType type;
    std::string textureName;  // 纹理文件名（不含扩展名）
    int texWidth, texHeight;  // 纹理尺寸

    // 部件列表
    MobCuboid head;
    MobCuboid body;
    MobCuboid legFrontLeft, legFrontRight;
    MobCuboid legBackLeft, legBackRight;
    MobCuboid armLeft, armRight;  // 仅人形生物使用

    // 额外部件（如牛的嘴巴）
    MobCuboid extraParts[4];     // 最多4个额外部件
    int extraPartCount = 0;      // 实际使用的额外部件数
    bool extraFollowHead[4] = {false, false, false, false};  // 是否跟随头部旋转

    // 羊毛覆盖层（MC原版：羊有独立的毛层渲染在身体/头上方）
    bool hasWoolOverlay = false;
    MobCuboid woolBody;          // 羊毛身体层（比身体稍大）
    MobCuboid woolHead;          // 羊毛头部层
    int woolTexOffsetX = 0, woolTexOffsetY = 0;  // 羊毛纹理在图集中的UV偏移

    bool isHumanoid = false;     // 人形（僵尸/骷髅）vs 四足
    bool hasArms = false;
};

// 生物渲染器：收集所有可见生物，构建顶点数据，一次 draw call
class MobRenderer {
public:
    void init(VulkanEngine* engine);
    void destroy();

    // 加载所有生物纹理到独立的纹理图集
    // vanillaTexDir: MC原版纹理根目录（用于加载羊毛等额外纹理）
    bool loadMobTextures(VulkanEngine& engine, const std::string& mobTextureDir,
                         const std::string& vanillaTexDir = "");

    // 每帧重建 mesh
    void buildFrame(const EntityManager& mgr, float partialTick,
                    const class DayNightCycle* dayNight);

    // 渲染（需要在 3D 管线绑定后调用）
    // 使用独立的 descriptor set，不影响方块纹理
    void render(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout);

    // 获取生物纹理图集的 image view 和 sampler
    VkImageView getMobAtlasImageView() const { return mobAtlasImage_.imageView; }
    bool hasMobAtlas() const { return mobAtlasImage_.allocation != nullptr; }
    bool hasContent() const { return indexCountThisFrame_ > 0; }

    // 获取 mob 专用 descriptor set（每帧需要更新 UBO 绑定）
    VkDescriptorSet getMobDescriptorSet(int frameIndex) const { return mobDescriptorSets_[frameIndex]; }

private:
    VulkanEngine* engine_ = nullptr;

    // 生物纹理图集
    AllocatedImage mobAtlasImage_;
    uint32_t atlasWidth_ = 0, atlasHeight_ = 0;

    // 每帧独立的 descriptor set（绑定 mob 纹理 + 当前帧 UBO）
    VkDescriptorSet mobDescriptorSets_[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    // 模型定义（动态容器，支持扩展）
    std::vector<MobModelDef> models_;
    void registerModels();

    // 每种生物纹理在图集中的 UV 偏移
    struct TexRegion {
        float uOffset, vOffset;  // 归一化偏移
        float uScale, vScale;    // 归一化缩放
    };
    std::vector<TexRegion> texRegions_;

    // 羊毛纹理在图集中的独立区域（MC原版使用独立的 sheep_wool.png）
    TexRegion woolTexRegion_{};
    bool hasWoolTexture_ = false;
    int woolTexWidth_ = 64, woolTexHeight_ = 32;  // 羊毛纹理尺寸

    // CPU staging
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    // GPU buffers
    AllocatedBuffer vertexBuffer_;
    AllocatedBuffer indexBuffer_;
    VkDeviceSize vertexBufferSize_ = 0;
    VkDeviceSize indexBufferSize_ = 0;
    uint32_t indexCountThisFrame_ = 0;

    void ensureCapacity();
    void appendMobMesh(const MobEntity& mob, float partialTick, float skyLightFactor);
    void addCuboid(const MobCuboid& cuboid, const TexRegion& texReg,
                   int texW, int texH,
                   const glm::mat4& parentTransform, float light);
};
