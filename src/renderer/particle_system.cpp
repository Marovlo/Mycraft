#include "particle_system.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <random>

// === 随机数辅助 ===

// 使用线程局部的 mt19937 替代 rand()，线程安全且随机质量更好
static thread_local std::mt19937 sRng{std::random_device{}()};

float ParticleSystem::randFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(sRng);
}

glm::vec3 ParticleSystem::randDir() {
    // 避免零向量：循环直到得到非零向量再归一化
    for (int i = 0; i < 8; i++) {
        glm::vec3 v(randFloat(-1.0f, 1.0f), randFloat(-1.0f, 1.0f), randFloat(-1.0f, 1.0f));
        float lenSq = glm::dot(v, v);
        if (lenSq > 0.001f) return v / std::sqrt(lenSq);
    }
    return glm::vec3(0.0f, 1.0f, 0.0f); // 极端情况回退
}

// === 初始化/销毁 ===

void ParticleSystem::init(VulkanEngine* engine, const TextureAtlas* atlas) {
    engine_ = engine;
    atlas_ = atlas;
    particles_.reserve(256);
    vertices_.reserve(1024);
    indices_.reserve(1536);
}

void ParticleSystem::destroy() {
    if (vertexBuffer_.buffer) {
        vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        vertexBuffer_ = {};
    }
    if (indexBuffer_.buffer) {
        vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        indexBuffer_ = {};
    }
}

// === 粒子生成 ===

void ParticleSystem::spawnBlockBreak(const glm::vec3& blockPos, uint16_t tileIndex) {
    // 获取方块纹理的 UV
    glm::vec4 tileUV = atlas_->getTileUV(tileIndex);

    // 生成 8~12 个碎片粒子
    int count = 8 + rand() % 5;
    for (int i = 0; i < count; i++) {
        Particle p;
        // 在方块内随机位置生成
        p.position = blockPos + glm::vec3(
            randFloat(0.1f, 0.9f),
            randFloat(0.1f, 0.9f),
            randFloat(0.1f, 0.9f)
        );
        // 随机向外飞散
        p.velocity = glm::vec3(
            randFloat(-2.0f, 2.0f),
            randFloat(1.0f, 4.0f),
            randFloat(-2.0f, 2.0f)
        );
        p.gravity = glm::vec3(0.0f, -14.0f, 0.0f);

        // 从纹理中取一小块作为碎片 UV（4x4 像素区域）
        float uRange = tileUV.z - tileUV.x;
        float vRange = tileUV.w - tileUV.y;
        float subU = randFloat(0.0f, 0.75f);
        float subV = randFloat(0.0f, 0.75f);
        p.uvMin = glm::vec2(tileUV.x + subU * uRange, tileUV.y + subV * vRange);
        p.uvMax = glm::vec2(tileUV.x + (subU + 0.25f) * uRange, tileUV.y + (subV + 0.25f) * vRange);

        p.color = glm::vec4(1.0f);
        p.size = randFloat(0.05f, 0.12f);
        p.lifetime = randFloat(0.6f, 1.2f);
        p.age = 0.0f;
        p.friction = 0.6f;
        p.type = ParticleType::BlockBreak;
        p.onGround = false;
        p.alive = true;

        particles_.push_back(p);
    }
}

void ParticleSystem::spawnExplosion(const glm::vec3& pos, float radius) {
    int count = 20 + rand() % 10;
    for (int i = 0; i < count; i++) {
        Particle p;
        p.position = pos + randDir() * randFloat(0.0f, radius * 0.3f);
        p.velocity = randDir() * randFloat(2.0f, 6.0f * radius);
        p.gravity = glm::vec3(0.0f, -8.0f, 0.0f);

        // 爆炸使用灰白色纹理（取图集中第一个 tile 的一小块）
        glm::vec4 tileUV = atlas_->getTileUV(0);
        p.uvMin = glm::vec2(tileUV.x, tileUV.y);
        p.uvMax = glm::vec2(tileUV.x + (tileUV.z - tileUV.x) * 0.25f,
                            tileUV.y + (tileUV.w - tileUV.y) * 0.25f);

        // 灰色到橙色渐变
        float t = randFloat(0.0f, 1.0f);
        p.color = glm::mix(glm::vec4(0.3f, 0.3f, 0.3f, 1.0f),
                           glm::vec4(1.0f, 0.6f, 0.2f, 1.0f), t);
        p.size = randFloat(0.1f, 0.25f);
        p.lifetime = randFloat(0.5f, 1.5f);
        p.age = 0.0f;
        p.friction = 0.7f;
        p.type = ParticleType::Explosion;
        p.onGround = false;
        p.alive = true;

        particles_.push_back(p);
    }
}

void ParticleSystem::spawnFlame(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.position = pos + glm::vec3(randFloat(-0.2f, 0.2f), 0.0f, randFloat(-0.2f, 0.2f));
        p.velocity = glm::vec3(randFloat(-0.3f, 0.3f), randFloat(0.5f, 1.5f), randFloat(-0.3f, 0.3f));
        p.gravity = glm::vec3(0.0f, 0.5f, 0.0f); // 火焰向上飘

        // 使用图集中第一个 tile 的一小块
        glm::vec4 tileUV = atlas_->getTileUV(0);
        p.uvMin = glm::vec2(tileUV.x, tileUV.y);
        p.uvMax = glm::vec2(tileUV.x + (tileUV.z - tileUV.x) * 0.25f,
                            tileUV.y + (tileUV.w - tileUV.y) * 0.25f);

        p.color = glm::vec4(1.0f, 0.7f, 0.0f, 1.0f); // 橙黄色
        p.size = randFloat(0.05f, 0.1f);
        p.lifetime = randFloat(0.3f, 0.8f);
        p.age = 0.0f;
        p.friction = 0.9f;
        p.type = ParticleType::Flame;
        p.onGround = false;
        p.alive = true;

        particles_.push_back(p);
    }
}

void ParticleSystem::spawnSmoke(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; i++) {
        Particle p;
        p.position = pos + glm::vec3(randFloat(-0.2f, 0.2f), 0.0f, randFloat(-0.2f, 0.2f));
        p.velocity = glm::vec3(randFloat(-0.2f, 0.2f), randFloat(0.3f, 1.0f), randFloat(-0.2f, 0.2f));
        p.gravity = glm::vec3(0.0f, 0.2f, 0.0f); // 烟雾缓慢上升

        glm::vec4 tileUV = atlas_->getTileUV(0);
        p.uvMin = glm::vec2(tileUV.x, tileUV.y);
        p.uvMax = glm::vec2(tileUV.x + (tileUV.z - tileUV.x) * 0.25f,
                            tileUV.y + (tileUV.w - tileUV.y) * 0.25f);

        p.color = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f); // 灰色半透明
        p.size = randFloat(0.08f, 0.15f);
        p.lifetime = randFloat(1.0f, 2.5f);
        p.age = 0.0f;
        p.friction = 0.95f;
        p.type = ParticleType::Smoke;
        p.onGround = false;
        p.alive = true;

        particles_.push_back(p);
    }
}

void ParticleSystem::spawnCrit(const glm::vec3& pos) {
    int count = 5 + rand() % 4;
    for (int i = 0; i < count; i++) {
        Particle p;
        p.position = pos + glm::vec3(randFloat(-0.3f, 0.3f), randFloat(0.0f, 1.5f), randFloat(-0.3f, 0.3f));
        p.velocity = glm::vec3(randFloat(-1.0f, 1.0f), randFloat(0.5f, 2.0f), randFloat(-1.0f, 1.0f));
        p.gravity = glm::vec3(0.0f, -6.0f, 0.0f);

        glm::vec4 tileUV = atlas_->getTileUV(0);
        p.uvMin = glm::vec2(tileUV.x, tileUV.y);
        p.uvMax = glm::vec2(tileUV.x + (tileUV.z - tileUV.x) * 0.25f,
                            tileUV.y + (tileUV.w - tileUV.y) * 0.25f);

        p.color = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f); // 黄色星星
        p.size = randFloat(0.04f, 0.08f);
        p.lifetime = randFloat(0.3f, 0.6f);
        p.age = 0.0f;
        p.friction = 0.8f;
        p.type = ParticleType::Crit;
        p.onGround = false;
        p.alive = true;

        particles_.push_back(p);
    }
}

// MC 原版吃东西粒子：食物碎片从嘴巴位置飞出
// 每次调用生成 2~3 个碎片，使用食物纹理的随机小区域
void ParticleSystem::spawnEating(const glm::vec3& pos, const glm::vec3& forward, uint16_t tileIndex) {
    glm::vec4 tileUV = atlas_->getTileUV(tileIndex);

    int count = 2 + rand() % 2;
    for (int i = 0; i < count; i++) {
        Particle p;
        // 从嘴巴位置稍微偏移
        p.position = pos + forward * 0.3f + glm::vec3(
            randFloat(-0.1f, 0.1f),
            randFloat(-0.1f, 0.05f),
            randFloat(-0.1f, 0.1f)
        );

        // 向前方 + 向下飞散（MC 原版食物碎片向前飞出然后落下）
        p.velocity = forward * randFloat(0.5f, 1.5f) + glm::vec3(
            randFloat(-0.5f, 0.5f),
            randFloat(-0.5f, 0.5f),
            randFloat(-0.5f, 0.5f)
        );
        p.gravity = glm::vec3(0.0f, -12.0f, 0.0f);

        // 从食物纹理中取一小块作为碎片 UV（4x4 像素区域）
        float uRange = tileUV.z - tileUV.x;
        float vRange = tileUV.w - tileUV.y;
        float subU = randFloat(0.0f, 0.75f);
        float subV = randFloat(0.0f, 0.75f);
        p.uvMin = glm::vec2(tileUV.x + subU * uRange, tileUV.y + subV * vRange);
        p.uvMax = glm::vec2(tileUV.x + (subU + 0.25f) * uRange, tileUV.y + (subV + 0.25f) * vRange);

        p.color = glm::vec4(1.0f);  // 使用原始纹理颜色
        p.size = randFloat(0.03f, 0.07f);
        p.lifetime = randFloat(0.4f, 0.8f);
        p.age = 0.0f;
        p.friction = 0.7f;
        p.type = ParticleType::BlockBreak;  // 复用方块碎片的物理行为
        p.onGround = false;
        p.alive = true;

        particles_.push_back(p);
    }
}

// === 物理更新 ===

void ParticleSystem::update(float dt) {
    for (auto& p : particles_) {
        if (!p.alive) continue;

        p.age += dt;
        if (p.age >= p.lifetime) {
            p.alive = false;
            continue;
        }

        // 应用重力
        p.velocity += p.gravity * dt;

        // 应用摩擦力
        float frictionFactor = std::pow(p.friction, dt);
        p.velocity *= frictionFactor;

        // 更新位置
        p.position += p.velocity * dt;

        // 地面碰撞：检查粒子脚下方块是否为实心
        // 使用整数坐标查找地面高度（简化：只检查当前位置下方最近的实心方块）
        int bx = static_cast<int>(std::floor(p.position.x));
        int bz = static_cast<int>(std::floor(p.position.z));
        int by = static_cast<int>(std::floor(p.position.y));
        // 如果当前位置下方是实心方块，弹起
        if (by >= 0 && by < 256) {
            // 粒子在方块内部或下方有实心方块时碰撞
            float blockTopY = static_cast<float>(by + 1);
            if (p.velocity.y < 0.0f && p.position.y < blockTopY) {
                // 简化检查：如果粒子正在下落且 y 坐标低于某个整数边界
                // 回退到简单地面检测（y < 生成位置附近的地面）
                p.position.y = blockTopY;
                p.velocity.y = -p.velocity.y * 0.3f;
                p.velocity.x *= 0.7f;
                p.velocity.z *= 0.7f;
                p.onGround = true;
            }
        }
        // 虚空保护
        if (p.position.y < -64.0f) {
            p.alive = false;
        }
    }

    // 移除死亡粒子
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
                       [](const Particle& p) { return !p.alive; }),
        particles_.end()
    );

    // 限制最大粒子数
    if (particles_.size() > 2048) {
        particles_.erase(particles_.begin(), particles_.begin() + (particles_.size() - 2048));
    }
}

// === 渲染构建 ===

void ParticleSystem::buildFrame(const glm::vec3& cameraPos, const glm::vec3& cameraRight, const glm::vec3& cameraUp) {
    vertices_.clear();
    indices_.clear();

    if (particles_.empty()) {
        indexCountThisFrame_ = 0;
        return;
    }

    for (const auto& p : particles_) {
        if (!p.alive) continue;

        // 计算透明度（随生命衰减）
        float lifeRatio = p.age / p.lifetime;
        float alpha = 1.0f - lifeRatio * lifeRatio; // 二次衰减

        // Billboard 四边形（面向摄像机）
        float halfSize = p.size * 0.5f;

        // 缩放：粒子在生命末期缩小
        float scale = 1.0f;
        if (lifeRatio > 0.7f) {
            scale = 1.0f - (lifeRatio - 0.7f) / 0.3f;
        }
        halfSize *= scale;

        glm::vec3 right = cameraRight * halfSize;
        glm::vec3 up = cameraUp * halfSize;

        glm::vec3 bl = p.position - right - up;
        glm::vec3 br = p.position + right - up;
        glm::vec3 tr = p.position + right + up;
        glm::vec3 tl = p.position - right + up;

        // 法线朝向摄像机
        glm::vec3 normal = glm::normalize(cameraPos - p.position);

        // 光照值：使用颜色 alpha 通道编码透明度
        float light = alpha * p.color.a;

        uint32_t baseIdx = static_cast<uint32_t>(vertices_.size());

        vertices_.push_back({bl, normal, {p.uvMin.x, p.uvMax.y}, light});
        vertices_.push_back({br, normal, {p.uvMax.x, p.uvMax.y}, light});
        vertices_.push_back({tr, normal, {p.uvMax.x, p.uvMin.y}, light});
        vertices_.push_back({tl, normal, {p.uvMin.x, p.uvMin.y}, light});

        indices_.push_back(baseIdx + 0);
        indices_.push_back(baseIdx + 1);
        indices_.push_back(baseIdx + 2);
        indices_.push_back(baseIdx + 0);
        indices_.push_back(baseIdx + 2);
        indices_.push_back(baseIdx + 3);
    }

    indexCountThisFrame_ = static_cast<uint32_t>(indices_.size());
    if (indexCountThisFrame_ == 0) return;

    ensureCapacity();

    // 上传顶点数据
    void* data;
    vmaMapMemory(engine_->getAllocator(), vertexBuffer_.allocation, &data);
    std::memcpy(data, vertices_.data(), vertices_.size() * sizeof(Vertex));
    vmaUnmapMemory(engine_->getAllocator(), vertexBuffer_.allocation);

    vmaMapMemory(engine_->getAllocator(), indexBuffer_.allocation, &data);
    std::memcpy(data, indices_.data(), indices_.size() * sizeof(uint32_t));
    vmaUnmapMemory(engine_->getAllocator(), indexBuffer_.allocation);
}

void ParticleSystem::render(VkCommandBuffer cmd) {
    if (indexCountThisFrame_ == 0) return;

    VkBuffer vb[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCountThisFrame_, 1, 0, 0, 0);
}

void ParticleSystem::ensureCapacity() {
    VkDeviceSize neededVB = vertices_.size() * sizeof(Vertex);
    VkDeviceSize neededIB = indices_.size() * sizeof(uint32_t);

    if (neededVB > vertexBufferSize_) {
        if (vertexBuffer_.buffer) {
            vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        }
        vertexBufferSize_ = neededVB * 2; // 2x 摊销
        VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufInfo.size = vertexBufferSize_;
        bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaCreateBuffer(engine_->getAllocator(), &bufInfo, &allocInfo,
                        &vertexBuffer_.buffer, &vertexBuffer_.allocation, nullptr);
    }

    if (neededIB > indexBufferSize_) {
        if (indexBuffer_.buffer) {
            vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        }
        indexBufferSize_ = neededIB * 2;
        VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufInfo.size = indexBufferSize_;
        bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        vmaCreateBuffer(engine_->getAllocator(), &bufInfo, &allocInfo,
                        &indexBuffer_.buffer, &indexBuffer_.allocation, nullptr);
    }
}
