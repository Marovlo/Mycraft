#include "mob_renderer.h"
#include "entity/entity_manager.h"
#include "world/day_night_cycle.h"
#include "core/common.h"
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

// ========== 模型注册 ==========

void MobRenderer::registerModels() {
    // ========== 猪 (Pig) ==========
    // MC原版Java: 纹理64x32, 头8x8x8 UV(0,0), 身体addBox(-5,-10,-7, 10,16,8) UV(28,8), 腿4x6x4 UV(0,16)
    // 身体在Java中是竖直定义后旋转90度变水平，所以3D尺寸(10,8,16)但UV用(10,16,8)
    {
        auto& pig = models_[static_cast<int>(MobType::Pig)];
        pig.type = MobType::Pig; pig.textureName = "pig"; pig.texWidth = 64; pig.texHeight = 32;
        // 身体：3D空间 宽10 高8 长16，UV展开用 sx=10 sy=16 sz=8
        pig.body = {{-5, 7, -8}, {10, 8, 16}, {0, 11, 0}, 28, 8, {10, 16, 8}};
        // 头部：8x8x8
        pig.head = {{-4, 8, -14}, {8, 8, 8}, {0, 12, -6}, 0, 0};
        // 四条腿：4x6x4
        pig.legFrontLeft  = {{-5, 1, -7}, {4, 6, 4}, {-3, 7, -5}, 0, 16};
        pig.legFrontRight = {{ 1, 1, -7}, {4, 6, 4}, { 3, 7, -5}, 0, 16};
        pig.legBackLeft   = {{-5, 1,  3}, {4, 6, 4}, {-3, 7,  5}, 0, 16};
        pig.legBackRight  = {{ 1, 1,  3}, {4, 6, 4}, { 3, 7,  5}, 0, 16};
        pig.isHumanoid = false;
    }

    // ========== 牛 (Cow) ==========
    // MC原版Java: 纹理64x32, 头8x8x8 UV(0,0), 身体addBox(-6,-10,-7, 12,18,10) UV(18,4), 腿4x12x4 UV(0,16)
    // 身体旋转90度：3D(12,10,18) UV(12,18,10)
    {
        auto& cow = models_[static_cast<int>(MobType::Cow)];
        cow.type = MobType::Cow; cow.textureName = "cow"; cow.texWidth = 64; cow.texHeight = 32;
        // 身体：3D空间 宽12 高10 长18，UV展开用 sx=12 sy=18 sz=10
        cow.body = {{-6, 12, -9}, {12, 10, 18}, {0, 17, 0}, 18, 4, {12, 18, 10}};
        // 头部：8x8x8
        cow.head = {{-4, 14, -17}, {8, 8, 8}, {0, 18, -9}, 0, 0};
        // 四条腿：4x12x4
        cow.legFrontLeft  = {{-6, 0, -8}, {4, 12, 4}, {-4, 12, -7}, 0, 16};
        cow.legFrontRight = {{ 2, 0, -8}, {4, 12, 4}, { 4, 12, -7}, 0, 16};
        cow.legBackLeft   = {{-6, 0,  5}, {4, 12, 4}, {-4, 12,  7}, 0, 16};
        cow.legBackRight  = {{ 2, 0,  5}, {4, 12, 4}, { 4, 12,  7}, 0, 16};
        cow.isHumanoid = false;
    }

    // ========== 羊 (Sheep) ==========
    // MC原版Java: 纹理64x32, 头6x6x8 UV(0,0), 身体addBox(-4,-10,-7, 8,16,6) UV(28,8), 腿4x12x4 UV(0,16)
    // 身体旋转90度：3D(8,6,16) UV(8,16,6)
    {
        auto& sheep = models_[static_cast<int>(MobType::Sheep)];
        sheep.type = MobType::Sheep; sheep.textureName = "sheep"; sheep.texWidth = 64; sheep.texHeight = 32;
        // 身体：3D空间 宽8 高6 长16，UV展开用 sx=8 sy=16 sz=6
        sheep.body = {{-4, 13, -8}, {8, 6, 16}, {0, 16, 0}, 28, 8, {8, 16, 6}};
        // 头部：6x6x8（深度8）
        sheep.head = {{-3, 13, -14}, {6, 6, 8}, {0, 16, -6}, 0, 0};
        // 四条腿：4x12x4
        sheep.legFrontLeft  = {{-4, 1, -7}, {4, 12, 4}, {-2, 13, -5}, 0, 16};
        sheep.legFrontRight = {{ 0, 1, -7}, {4, 12, 4}, { 2, 13, -5}, 0, 16};
        sheep.legBackLeft   = {{-4, 1,  3}, {4, 12, 4}, {-2, 13,  5}, 0, 16};
        sheep.legBackRight  = {{ 0, 1,  3}, {4, 12, 4}, { 2, 13,  5}, 0, 16};
        sheep.isHumanoid = false;
    }

    // ========== 鸡 (Chicken) ==========
    // MC原版Java: 纹理64x32, 头4x6x3 UV(0,0), 身体addBox(-3,-4,-3, 6,8,6) UV(0,9), 腿3x5x3 UV(26,0)
    // 身体旋转90度：3D(6,6,8) UV(6,8,6)
    {
        auto& chicken = models_[static_cast<int>(MobType::Chicken)];
        chicken.type = MobType::Chicken; chicken.textureName = "chicken"; chicken.texWidth = 64; chicken.texHeight = 32;
        // 身体：3D空间 宽6 高6 长8，UV展开用 sx=6 sy=8 sz=6
        chicken.body = {{-3, 4, -4}, {6, 6, 8}, {0, 7, 0}, 0, 9, {6, 8, 6}};
        // 头部：4x6x3 紧贴身体前方顶部
        chicken.head = {{-2, 9, -7}, {4, 6, 3}, {0, 12, -4}, 0, 0};
        // 两条细腿：3x5x3
        chicken.legFrontLeft  = {{-2, 0, -1}, {3, 5, 3}, {-1, 4, 0}, 26, 0};
        chicken.legFrontRight = {{ 0, 0, -1}, {3, 5, 3}, { 1, 4, 0}, 26, 0};
        chicken.legBackLeft   = {{-2, 0, -1}, {3, 5, 3}, {-1, 4, 0}, 26, 0};
        chicken.legBackRight  = {{ 0, 0, -1}, {3, 5, 3}, { 1, 4, 0}, 26, 0};
        chicken.isHumanoid = false;
    }

    // ========== 僵尸 (Zombie) ==========
    // MC原版：人形，头在身体上方，两条腿+两条手臂
    {
        auto& zombie = models_[static_cast<int>(MobType::Zombie)];
        zombie.type = MobType::Zombie; zombie.textureName = "zombie"; zombie.texWidth = 64; zombie.texHeight = 64;
        // 头部：8x8x8 在身体上方
        zombie.head = {{-4, 24, -4}, {8, 8, 8}, {0, 24, 0}, 0, 0};
        // 身体：8x12x4
        zombie.body = {{-4, 12, -2}, {8, 12, 4}, {0, 18, 0}, 16, 16};
        // 两条腿（原版左右腿共用UV）
        zombie.legFrontLeft  = {{-4, 0, -2}, {4, 12, 4}, {-2, 12, 0}, 0, 16};
        zombie.legFrontRight = {{ 0, 0, -2}, {4, 12, 4}, { 2, 12, 0}, 0, 16};
        zombie.legBackLeft   = {{-4, 0, -2}, {4, 12, 4}, {-2, 12, 0}, 0, 16};
        zombie.legBackRight  = {{ 0, 0, -2}, {4, 12, 4}, { 2, 12, 0}, 0, 16};
        // 手臂
        zombie.armLeft  = {{-8, 12, -2}, {4, 12, 4}, {-6, 22, 0}, 40, 16};
        zombie.armRight = {{ 4, 12, -2}, {4, 12, 4}, { 6, 22, 0}, 40, 16};
        zombie.isHumanoid = true; zombie.hasArms = true;
    }

    // ========== 骷髅 (Skeleton) ==========
    // MC原版：人形，细骨骼
    {
        auto& skeleton = models_[static_cast<int>(MobType::Skeleton)];
        skeleton.type = MobType::Skeleton; skeleton.textureName = "skeleton"; skeleton.texWidth = 64; skeleton.texHeight = 32;
        skeleton.head = {{-4, 24, -4}, {8, 8, 8}, {0, 24, 0}, 0, 0};
        skeleton.body = {{-4, 12, -2}, {8, 12, 4}, {0, 18, 0}, 16, 16};
        skeleton.legFrontLeft  = {{-3, 0, -1}, {2, 12, 2}, {-2, 12, 0}, 0, 16};
        skeleton.legFrontRight = {{ 1, 0, -1}, {2, 12, 2}, { 2, 12, 0}, 0, 16};
        skeleton.legBackLeft   = {{-3, 0, -1}, {2, 12, 2}, {-2, 12, 0}, 0, 16};
        skeleton.legBackRight  = {{ 1, 0, -1}, {2, 12, 2}, { 2, 12, 0}, 0, 16};
        skeleton.armLeft  = {{-7, 12, -1}, {2, 12, 2}, {-5, 22, 0}, 40, 16};
        skeleton.armRight = {{ 5, 12, -1}, {2, 12, 2}, { 5, 22, 0}, 40, 16};
        skeleton.isHumanoid = true; skeleton.hasArms = true;
    }

    // ========== 蜘蛛 (Spider) ==========
    // MC原版Java: 纹理64x32, 头8x8x8 UV(32,4), 身体10x8x12 UV(0,0), 腿16x2x2 UV(18,0)
    {
        auto& spider = models_[static_cast<int>(MobType::Spider)];
        spider.type = MobType::Spider; spider.textureName = "spider"; spider.texWidth = 64; spider.texHeight = 32;
        // 头部在身体前方
        spider.head = {{-4, 3, -11}, {8, 8, 8}, {0, 7, -3}, 32, 4};
        // 身体：宽10 高8 长12
        spider.body = {{-5, 3, -3}, {10, 8, 12}, {0, 7, 3}, 0, 0};
        // 四对腿（原版16x2x2长条，简化为4对向外伸展）
        spider.legFrontLeft  = {{-15, 0, -6}, {16, 2, 2}, {-5, 5, -5}, 18, 0};
        spider.legFrontRight = {{ -1, 0, -6}, {16, 2, 2}, { 5, 5, -5}, 18, 0};
        spider.legBackLeft   = {{-15, 0,  4}, {16, 2, 2}, {-5, 5,  5}, 18, 0};
        spider.legBackRight  = {{ -1, 0,  4}, {16, 2, 2}, { 5, 5,  5}, 18, 0};
        spider.isHumanoid = false;
    }

    // ========== 苦力怕 (Creeper) ==========
    // MC原版：头在身体上方，四条短腿，无手臂
    {
        auto& creeper = models_[static_cast<int>(MobType::Creeper)];
        creeper.type = MobType::Creeper; creeper.textureName = "creeper"; creeper.texWidth = 64; creeper.texHeight = 32;
        creeper.head = {{-4, 18, -4}, {8, 8, 8}, {0, 18, 0}, 0, 0};
        creeper.body = {{-4, 6, -2}, {8, 12, 4}, {0, 12, 0}, 16, 16};
        creeper.legFrontLeft  = {{-4, 0, -4}, {4, 6, 4}, {-2, 6, -2}, 0, 16};
        creeper.legFrontRight = {{ 0, 0, -4}, {4, 6, 4}, { 2, 6, -2}, 0, 16};
        creeper.legBackLeft   = {{-4, 0,  0}, {4, 6, 4}, {-2, 6,  2}, 0, 16};
        creeper.legBackRight  = {{ 0, 0,  0}, {4, 6, 4}, { 2, 6,  2}, 0, 16};
        creeper.isHumanoid = false;
    }
}

// ========== 初始化 / 销毁 ==========

void MobRenderer::init(VulkanEngine* engine) {
    engine_ = engine;
    registerModels();

    constexpr size_t INITIAL_VERTS = 6 * 4 * 64;
    constexpr size_t INITIAL_INDS  = 6 * 6 * 64;
    vertexBufferSize_ = sizeof(Vertex) * INITIAL_VERTS;
    indexBufferSize_  = sizeof(uint32_t) * INITIAL_INDS;
    vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indexBuffer_  = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void MobRenderer::destroy() {
    if (!engine_) return;
    if (vertexBuffer_.allocation) {
        vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        vertexBuffer_ = {};
    }
    if (indexBuffer_.allocation) {
        vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        indexBuffer_ = {};
    }
    if (mobAtlasImage_.allocation) {
        engine_->destroyTexture(mobAtlasImage_);
        mobAtlasImage_ = {};
    }
    engine_ = nullptr;
}

// ========== 加载生物纹理 ==========

bool MobRenderer::loadMobTextures(VulkanEngine& engine, const std::string& mobTextureDir) {
    // 收集所有生物纹理，打包成一个图集
    // 布局：垂直堆叠，每种生物一行
    struct TexEntry {
        MobType type;
        std::string path;
        int w, h;
        uint8_t* data;
    };

    std::vector<TexEntry> entries;
    for (int i = 0; i < static_cast<int>(MobType::COUNT); i++) {
        auto& model = models_[i];
        std::string path = mobTextureDir + "/" + model.textureName + ".png";
        if (!fs::exists(path)) {
            std::cerr << "MobRenderer: missing texture " << path << "\n";
            continue;
        }
        int w, h, ch;
        uint8_t* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (!data) {
            std::cerr << "MobRenderer: failed to load " << path << "\n";
            continue;
        }
        entries.push_back({static_cast<MobType>(i), path, w, h, data});
    }

    if (entries.empty()) return false;

    // 计算图集尺寸：所有纹理水平排列
    int totalW = 0, maxH = 0;
    for (auto& e : entries) {
        totalW += e.w;
        maxH = std::max(maxH, e.h);
    }

    // 对齐到2的幂
    auto nextPow2 = [](int v) -> int {
        int p = 1;
        while (p < v) p *= 2;
        return p;
    };
    atlasWidth_ = nextPow2(totalW);
    atlasHeight_ = nextPow2(maxH);

    std::vector<uint8_t> atlasPixels(atlasWidth_ * atlasHeight_ * 4, 0);

    int xOffset = 0;
    for (auto& e : entries) {
        int idx = static_cast<int>(e.type);
        texRegions_[idx].uOffset = static_cast<float>(xOffset) / atlasWidth_;
        texRegions_[idx].vOffset = 0.0f;
        texRegions_[idx].uScale = static_cast<float>(e.w) / atlasWidth_;
        texRegions_[idx].vScale = static_cast<float>(e.h) / atlasHeight_;

        // Blit
        for (int y = 0; y < e.h; y++) {
            uint32_t srcOff = y * e.w * 4;
            uint32_t dstOff = (y * atlasWidth_ + xOffset) * 4;
            std::memcpy(&atlasPixels[dstOff], &e.data[srcOff], e.w * 4);
        }

        xOffset += e.w;
        stbi_image_free(e.data);
    }

    // 上传到 GPU
    mobAtlasImage_ = engine.uploadTexture(atlasPixels.data(),
        static_cast<int>(atlasWidth_), static_cast<int>(atlasHeight_), 4);

    // 为每帧分配独立的 descriptor set
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        mobDescriptorSets_[i] = engine.allocateExtraDescriptorSet(
            mobAtlasImage_.imageView, engine.getDefaultSampler());
        if (mobDescriptorSets_[i] == VK_NULL_HANDLE) {
            std::cerr << "MobRenderer: failed to allocate descriptor set for frame " << i << "\n";
            return false;
        }
    }

    std::cout << "MobRenderer: loaded " << entries.size() << " mob textures ("
              << atlasWidth_ << "x" << atlasHeight_ << " atlas)\n";
    return true;
}

// ========== 构建帧 ==========

void MobRenderer::ensureCapacity() {
    VkDeviceSize needV = sizeof(Vertex) * vertices_.size();
    VkDeviceSize needI = sizeof(uint32_t) * indices_.size();

    if (needV > vertexBufferSize_) {
        if (vertexBuffer_.allocation)
            vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        vertexBufferSize_ = needV * 2;
        vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }
    if (needI > indexBufferSize_) {
        if (indexBuffer_.allocation)
            vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        indexBufferSize_ = needI * 2;
        indexBuffer_ = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }
}

void MobRenderer::buildFrame(const EntityManager& mgr, float partialTick,
                              const DayNightCycle* dayNight) {
    vertices_.clear();
    indices_.clear();

    float skyLightFactor = dayNight ? dayNight->getSkyLightFactor() : 1.0f;

    for (const auto& e : mgr.entities()) {
        if (!e || !e->alive) continue;
        if (e->kind() != EntityKind::Mob) continue;
        appendMobMesh(static_cast<const MobEntity&>(*e), partialTick, skyLightFactor);
    }

    indexCountThisFrame_ = static_cast<uint32_t>(indices_.size());
    if (indexCountThisFrame_ == 0) return;

    ensureCapacity();

    // 更新当前帧的 mob descriptor set 的 UBO 绑定
    {
        int frameIdx = engine_->getCurrentFrameIndex();
        auto& frame = engine_->getCurrentFrame();
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = frame.uniformBuffer.buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet uboWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        uboWrite.dstSet = mobDescriptorSets_[frameIdx];
        uboWrite.dstBinding = 0;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(engine_->getDevice(), 1, &uboWrite, 0, nullptr);
    }

    void* vData = engine_->mapBuffer(vertexBuffer_);
    std::memcpy(vData, vertices_.data(), sizeof(Vertex) * vertices_.size());
    engine_->unmapBuffer(vertexBuffer_);

    void* iData = engine_->mapBuffer(indexBuffer_);
    std::memcpy(iData, indices_.data(), sizeof(uint32_t) * indices_.size());
    engine_->unmapBuffer(indexBuffer_);
}

// ========== 渲染单个生物 ==========

void MobRenderer::appendMobMesh(const MobEntity& mob, float partialTick, float skyLightFactor) {
    int typeIdx = static_cast<int>(mob.mobType);
    const auto& model = models_[typeIdx];
    const auto& texReg = texRegions_[typeIdx];

    // 插值位置和朝向
    glm::vec3 renderPos = glm::mix(mob.prevPosition, mob.position, partialTick);
    float renderYaw = glm::mix(mob.prevBodyYaw, mob.bodyYaw, partialTick);
    float renderWalk = glm::mix(mob.prevWalkCycle, mob.walkCycle, partialTick);

    // 受伤闪红：使用特殊 light 编码 (light > 1.5 = 受伤, 实际亮度 = light - 2.0)
    float light = skyLightFactor;
    if (mob.hurtTicks > 0) {
        light = 2.0f + skyLightFactor;  // 着色器中 light > 1.5 触发红色混合
    }

    // 死亡动画：绕Z轴旋转倒地，使用 partialTick 平滑插值
    float deathAngle = 0.0f;
    if (mob.isDying) {
        // 用 partialTick 在两个 tick 之间插值，消除阶梯感
        float smoothDeathTicks = static_cast<float>(mob.deathTicks) + partialTick;
        float progress = std::min(smoothDeathTicks / 20.0f, 1.0f);
        // 使用 smoothstep 缓动函数让倒地动画更自然（先慢后快再慢）
        float eased = progress * progress * (3.0f - 2.0f * progress);
        deathAngle = eased * 1.5708f;  // 最终倒地90度
    }

    // 缩放因子：将像素坐标转换为方块坐标 (1/16)
    float scale = 1.0f / 16.0f;

    // 根变换：位置 + 旋转
    glm::mat4 root(1.0f);
    // renderPos 是碰撞箱中心，但模型定义中 Y=0 是脚底
    // 需要向下偏移 halfExtents.y 使模型脚底对齐地面
    root = glm::translate(root, renderPos - glm::vec3(0, mob.halfExtents.y, 0));
    root = glm::rotate(root, renderYaw, glm::vec3(0, 1, 0));
    if (deathAngle > 0.0f) {
        root = glm::rotate(root, deathAngle, glm::vec3(0, 0, 1));
    }
    root = glm::scale(root, glm::vec3(scale));

    // 苦力怕膨胀效果
    if (mob.mobType == MobType::Creeper && mob.ignited) {
        float swell = 1.0f + 0.3f * std::sin(mob.fuseTimer * 0.5f);
        root = glm::scale(root, glm::vec3(swell, swell, swell));
    }

    // 渲染各部件
    // 头部
    addCuboid(model.head, texReg, model.texWidth, model.texHeight, root, light);

    // 身体
    addCuboid(model.body, texReg, model.texWidth, model.texHeight, root, light);

    // 腿部动画
    float legSwing = std::sin(renderWalk) * 0.7f;

    if (model.isHumanoid) {
        // 人形：左右腿交替摆动
        glm::mat4 leftLeg = root;
        leftLeg = glm::translate(leftLeg, model.legFrontLeft.pivot);
        leftLeg = glm::rotate(leftLeg, legSwing, glm::vec3(1, 0, 0));
        leftLeg = glm::translate(leftLeg, -model.legFrontLeft.pivot);
        addCuboid(model.legFrontLeft, texReg, model.texWidth, model.texHeight, leftLeg, light);

        glm::mat4 rightLeg = root;
        rightLeg = glm::translate(rightLeg, model.legFrontRight.pivot);
        rightLeg = glm::rotate(rightLeg, -legSwing, glm::vec3(1, 0, 0));
        rightLeg = glm::translate(rightLeg, -model.legFrontRight.pivot);
        addCuboid(model.legFrontRight, texReg, model.texWidth, model.texHeight, rightLeg, light);

        // 手臂
        if (model.hasArms) {
            glm::mat4 leftArm = root;
            leftArm = glm::translate(leftArm, model.armLeft.pivot);
            leftArm = glm::rotate(leftArm, -legSwing * 0.5f, glm::vec3(1, 0, 0));
            leftArm = glm::translate(leftArm, -model.armLeft.pivot);
            addCuboid(model.armLeft, texReg, model.texWidth, model.texHeight, leftArm, light);

            glm::mat4 rightArm = root;
            rightArm = glm::translate(rightArm, model.armRight.pivot);
            rightArm = glm::rotate(rightArm, legSwing * 0.5f, glm::vec3(1, 0, 0));
            rightArm = glm::translate(rightArm, -model.armRight.pivot);
            addCuboid(model.armRight, texReg, model.texWidth, model.texHeight, rightArm, light);
        }
    } else {
        // 四足：前后腿交替摆动
        auto animateLeg = [&](const MobCuboid& leg, float angle) {
            glm::mat4 m = root;
            m = glm::translate(m, leg.pivot);
            m = glm::rotate(m, angle, glm::vec3(1, 0, 0));
            m = glm::translate(m, -leg.pivot);
            addCuboid(leg, texReg, model.texWidth, model.texHeight, m, light);
        };

        animateLeg(model.legFrontLeft,   legSwing);
        animateLeg(model.legFrontRight, -legSwing);
        animateLeg(model.legBackLeft,   -legSwing);
        animateLeg(model.legBackRight,   legSwing);
    }
}

// ========== 添加长方体 ==========

void MobRenderer::addCuboid(const MobCuboid& cuboid, const TexRegion& texReg,
                              int texW, int texH,
                              const glm::mat4& parentTransform, float light) {
    float x0 = cuboid.origin.x;
    float y0 = cuboid.origin.y;
    float z0 = cuboid.origin.z;
    float sx = cuboid.size.x;
    float sy = cuboid.size.y;
    float sz = cuboid.size.z;

    // 8个顶点
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

    // MC 纹理布局（标准长方体展开）：
    // 简化：使用均匀颜色采样（从纹理区域的中心取色）
    // 实际上我们用纹理区域的一小块来映射每个面
    float uBase = texReg.uOffset;
    float vBase = texReg.vOffset;
    float uPixel = texReg.uScale / texW;
    float vPixel = texReg.vScale / texH;

    // UV 坐标基于 cuboid 的 uvX, uvY 和尺寸
    int uvX = cuboid.uvX;
    int uvY = cuboid.uvY;
    // 如果指定了uvSize则用于UV展开（身体旋转90度时3D尺寸和UV尺寸不同）
    bool hasUvSize = (cuboid.uvSize.x > 0 || cuboid.uvSize.y > 0 || cuboid.uvSize.z > 0);
    int isx = hasUvSize ? static_cast<int>(cuboid.uvSize.x) : static_cast<int>(sx);
    int isy = hasUvSize ? static_cast<int>(cuboid.uvSize.y) : static_cast<int>(sy);
    int isz = hasUvSize ? static_cast<int>(cuboid.uvSize.z) : static_cast<int>(sz);

    // 6个面的 UV 映射（MC标准展开）
    struct FaceUV {
        float u0, v0, u1, v1;
    };

    // 顶面: (uvX+sz, uvY) to (uvX+sz+sx, uvY+sz)
    FaceUV top = {
        uBase + (uvX + isz) * uPixel, vBase + uvY * vPixel,
        uBase + (uvX + isz + isx) * uPixel, vBase + (uvY + isz) * vPixel
    };
    // 底面: (uvX+sz+sx, uvY) to (uvX+sz+sx+sx, uvY+sz)
    FaceUV bottom = {
        uBase + (uvX + isz + isx) * uPixel, vBase + uvY * vPixel,
        uBase + (uvX + isz + isx + isx) * uPixel, vBase + (uvY + isz) * vPixel
    };
    // 正面: (uvX+sz, uvY+sz) to (uvX+sz+sx, uvY+sz+sy)
    FaceUV front = {
        uBase + (uvX + isz) * uPixel, vBase + (uvY + isz) * vPixel,
        uBase + (uvX + isz + isx) * uPixel, vBase + (uvY + isz + isy) * vPixel
    };
    // 背面: (uvX+sz+sx+sz, uvY+sz) to (uvX+sz+sx+sz+sx, uvY+sz+sy)
    FaceUV back = {
        uBase + (uvX + isz + isx + isz) * uPixel, vBase + (uvY + isz) * vPixel,
        uBase + (uvX + isz + isx + isz + isx) * uPixel, vBase + (uvY + isz + isy) * vPixel
    };
    // 左面: (uvX, uvY+sz) to (uvX+sz, uvY+sz+sy)
    FaceUV left = {
        uBase + uvX * uPixel, vBase + (uvY + isz) * vPixel,
        uBase + (uvX + isz) * uPixel, vBase + (uvY + isz + isy) * vPixel
    };
    // 右面: (uvX+sz+sx, uvY+sz) to (uvX+sz+sx+sz, uvY+sz+sy)
    FaceUV right = {
        uBase + (uvX + isz + isx) * uPixel, vBase + (uvY + isz) * vPixel,
        uBase + (uvX + isz + isx + isz) * uPixel, vBase + (uvY + isz + isy) * vPixel
    };

    auto addFace = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                       glm::vec3 normal, const FaceUV& uv) {
        uint32_t base = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back({v0, normal, {uv.u0, uv.v1}, light});
        vertices_.push_back({v1, normal, {uv.u0, uv.v0}, light});
        vertices_.push_back({v2, normal, {uv.u1, uv.v0}, light});
        vertices_.push_back({v3, normal, {uv.u1, uv.v1}, light});
        indices_.push_back(base + 0);
        indices_.push_back(base + 1);
        indices_.push_back(base + 2);
        indices_.push_back(base + 0);
        indices_.push_back(base + 2);
        indices_.push_back(base + 3);
    };

    glm::mat3 N = glm::mat3(parentTransform);

    // +Y (top): corners 7,6,2,3 (修正绕序为CCW，避免被背面剔除)
    addFace(wc[7], wc[6], wc[2], wc[3], glm::normalize(N * glm::vec3(0, 1, 0)), top);
    // -Y (bottom): corners 0,1,5,4
    addFace(wc[0], wc[1], wc[5], wc[4], glm::normalize(N * glm::vec3(0, -1, 0)), bottom);
    // -Z (front): corners 0,3,2,1
    addFace(wc[0], wc[3], wc[2], wc[1], glm::normalize(N * glm::vec3(0, 0, -1)), front);
    // +Z (back): corners 5,6,7,4
    addFace(wc[5], wc[6], wc[7], wc[4], glm::normalize(N * glm::vec3(0, 0, 1)), back);
    // -X (left): corners 4,7,3,0
    addFace(wc[4], wc[7], wc[3], wc[0], glm::normalize(N * glm::vec3(-1, 0, 0)), left);
    // +X (right): corners 1,2,6,5
    addFace(wc[1], wc[2], wc[6], wc[5], glm::normalize(N * glm::vec3(1, 0, 0)), right);
}

// ========== 渲染 ==========

void MobRenderer::render(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout) {
    if (indexCountThisFrame_ == 0) return;

    // 绑定 mob 专用的 descriptor set（包含 mob 纹理）
    // 注意：调用者需要在调用后重新绑定方块纹理的 descriptor set
    int frameIdx = engine_->getCurrentFrameIndex();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout, 0, 1, &mobDescriptorSets_[frameIdx], 0, nullptr);

    VkBuffer vb[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCountThisFrame_, 1, 0, 0, 0);
}
