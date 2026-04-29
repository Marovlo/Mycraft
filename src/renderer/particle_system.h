#pragma once

#include "engine/vulkan_engine.h"
#include "renderer/texture_atlas.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

// 粒子类型
enum class ParticleType : uint8_t {
    BlockBreak,     // 方块破坏碎片
    Explosion,      // 爆炸粒子（苦力怕等）
    Flame,          // 火焰粒子
    Smoke,          // 烟雾粒子
    Crit,           // 暴击星星
    Drip,           // 水滴/岩浆滴
};

// 单个粒子
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 gravity;      // 重力加速度（通常 {0, -9.8, 0}）
    glm::vec2 uvMin;        // 纹理 UV 左上角
    glm::vec2 uvMax;        // 纹理 UV 右下角
    glm::vec4 color;        // 颜色/色调（RGBA）
    float size;             // 粒子尺寸（世界单位）
    float lifetime;         // 总生命时间（秒）
    float age;              // 已存活时间（秒）
    float friction;         // 速度衰减系数（0~1，每秒乘以此值）
    ParticleType type;
    bool onGround;          // 是否落地
    bool alive;
};

// 粒子系统
// 管理所有活跃粒子的生命周期、物理更新和渲染
// 使用与 EntityRenderer 相同的动态 mesh 方式：
// 每帧将所有粒子构建为面向摄像机的四边形，一次 draw call 渲染
class ParticleSystem {
public:
    void init(VulkanEngine* engine, const TextureAtlas* atlas);
    void destroy();

    // === 粒子生成接口 ===

    // 方块破坏粒子：在方块位置生成多个碎片粒子
    void spawnBlockBreak(const glm::vec3& blockPos, uint16_t tileIndex);

    // 爆炸粒子：在位置生成一圈爆炸碎片
    void spawnExplosion(const glm::vec3& pos, float radius = 1.0f);

    // 火焰粒子：在位置生成火焰
    void spawnFlame(const glm::vec3& pos, int count = 3);

    // 烟雾粒子：在位置生成烟雾
    void spawnSmoke(const glm::vec3& pos, int count = 3);

    // 暴击粒子：在实体位置生成星星
    void spawnCrit(const glm::vec3& pos);

    // === 每帧更新 ===

    // 物理更新（重力、碰撞、生命周期）
    // dt = 帧间隔时间（秒）
    void update(float dt);

    // 构建渲染 mesh（面向摄像机的 billboard 四边形）
    void buildFrame(const glm::vec3& cameraPos, const glm::vec3& cameraRight, const glm::vec3& cameraUp);

    // 渲染（在透明 pass 中调用）
    void render(VkCommandBuffer cmd);

    // 获取活跃粒子数
    size_t activeCount() const { return particles_.size(); }

private:
    VulkanEngine* engine_ = nullptr;
    const TextureAtlas* atlas_ = nullptr;

    std::vector<Particle> particles_;

    // 渲染数据
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    AllocatedBuffer vertexBuffer_;
    AllocatedBuffer indexBuffer_;
    VkDeviceSize vertexBufferSize_ = 0;
    VkDeviceSize indexBufferSize_ = 0;
    uint32_t indexCountThisFrame_ = 0;

    void ensureCapacity();

    // 随机数辅助
    float randFloat(float min, float max);
    glm::vec3 randDir();
};
