#include "player.h"
#include "world/world.h"
#include "core/serialization.h"
#include "audio/sound_engine.h"
#include <cmath>
#include <algorithm>

Player::Player() {
    position = glm::vec3(0.0f, 100.0f, 0.0f);
    velocity = glm::vec3(0.0f);
}

glm::vec3 Player::getEyePosition() const {
    return position + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);
}

glm::vec3 Player::getForward() const {
    return glm::normalize(glm::vec3(
        cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
        sin(glm::radians(pitch)),
        sin(glm::radians(yaw)) * cos(glm::radians(pitch))
    ));
}

glm::vec3 Player::getFlatForward() const {
    return glm::normalize(glm::vec3(cos(glm::radians(yaw)), 0.0f, sin(glm::radians(yaw))));
}

glm::vec3 Player::getRight() const {
    return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::mat4 Player::getViewMatrix() const {
    glm::vec3 eye = getEyePosition();
    return glm::lookAt(eye, eye + getForward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Player::getProjectionMatrix(float aspectRatio) const {
    auto proj = glm::perspective(glm::radians(fov), aspectRatio, 0.05f, 1000.0f);
    proj[1][1] *= -1; // Vulkan Y flip
    return proj;
}

void Player::look(double deltaX, double deltaY) {
    yaw   += static_cast<float>(deltaX) * sensitivity;
    pitch -= static_cast<float>(deltaY) * sensitivity;
    pitch  = std::clamp(pitch, -89.0f, 89.0f);
}

int Player::takeDamage(int amount) {
    if (dead || amount <= 0) return 0;
    int actual = std::min(amount, hp);
    hp -= actual;
    hurtTicks = 10;  // 0.5 seconds of screen shake

    // MC 原版：玩家受伤播放 hit 音效
    getSoundEngine().play(SoundEventId::DamageHit, position, 0.5f);

    if (hp <= 0) { hp = 0; dead = true; }
    return actual;
}

void Player::respawn() {
    position = spawnPoint;
    velocity = glm::vec3(0.0f);
    hp = maxHp;
    hunger = maxHunger;
    saturation = 5.0f;
    hungerTickTimer = 0;
    dead = false;
    fallStartY = spawnPoint.y;
    wasFalling = false;
    onGround = false;
    eatingTicks = 0;
    isEating = false;
    // MC 原版：死亡时丢失所有经验（简化：直接清零）
    xpLevel = 0;
    xpTotal = 0;
    xpProgress = 0.0f;
    // 重置动画状态
    swinging = false;
    swingTicks = 0;
    swingProgress = 0.0f;
    prevSwingProgress = 0.0f;
}

// ========== 第一人称手臂动画 ==========

void Player::startSwing() {
    // MC 原版：如果已经在挥动且进度 < 50%，不重新开始（防止连续点击重置动画）
    if (swinging && swingTicks < swingDuration / 2) return;
    swinging = true;
    swingTicks = 0;
}

void Player::tickSwing() {
    prevSwingProgress = swingProgress;
    if (swinging) {
        ++swingTicks;
        if (swingTicks >= swingDuration) {
            swinging = false;
            swingTicks = 0;
        }
    }
    // 计算归一化进度（用于渲染插值）
    if (swinging) {
        swingProgress = static_cast<float>(swingTicks) / static_cast<float>(swingDuration);
    } else {
        swingProgress = 0.0f;
    }
}

// ========== 经验值系统（MC 原版公式） ==========

int Player::xpForLevel(int level) const {
    // MC Java 1.8+ 升级所需经验公式：
    // Level 0-16:  2*level + 7
    // Level 17-31: 5*level - 38
    // Level 32+:   9*level - 158
    if (level <= 16) return 2 * level + 7;
    if (level <= 31) return 5 * level - 38;
    return 9 * level - 158;
}

int Player::xpToNextLevel() const {
    return xpForLevel(xpLevel);
}

int Player::xpAtCurrentLevel() const {
    // 从 xpTotal 反推当前等级已积累的经验
    int totalForCurrentLevel = 0;
    for (int i = 0; i < xpLevel; i++) {
        totalForCurrentLevel += xpForLevel(i);
    }
    return xpTotal - totalForCurrentLevel;
}

void Player::addXP(int amount) {
    if (amount <= 0) return;
    xpTotal += amount;

    // 重新计算等级和进度
    int remaining = xpTotal;
    int level = 0;
    while (true) {
        int needed = xpForLevel(level);
        if (remaining < needed) break;
        remaining -= needed;
        level++;
    }
    xpLevel = level;
    int needed = xpForLevel(level);
    xpProgress = (needed > 0) ? static_cast<float>(remaining) / needed : 0.0f;
}

void Player::serialize(BinaryWriter& w) const {
    // Position
    w.writeF32(position.x);
    w.writeF32(position.y);
    w.writeF32(position.z);

    // View angles
    w.writeF32(yaw);
    w.writeF32(pitch);

    // Health
    w.writeI32(hp);
    w.writeI32(maxHp);

    // Hunger
    w.writeI32(hunger);
    w.writeI32(maxHunger);
    w.writeF32(saturation);

    // Breathing
    w.writeI32(air);

    // Spawn point
    w.writeF32(spawnPoint.x);
    w.writeF32(spawnPoint.y);
    w.writeF32(spawnPoint.z);

    // 经验值
    w.writeI32(xpLevel);
    w.writeI32(xpTotal);
    w.writeF32(xpProgress);
}

void Player::deserialize(BinaryReader& r) {
    // Position
    position.x = r.readF32();
    position.y = r.readF32();
    position.z = r.readF32();

    // View angles
    yaw   = r.readF32();
    pitch = r.readF32();

    // Health
    hp    = r.readI32();
    maxHp = r.readI32();

    // Hunger
    hunger    = r.readI32();
    maxHunger = r.readI32();
    saturation = r.readF32();

    // Breathing
    air = r.readI32();

    // Spawn point
    spawnPoint.x = r.readF32();
    spawnPoint.y = r.readF32();
    spawnPoint.z = r.readF32();

    // 经验值
    xpLevel = r.readI32();
    xpTotal = r.readI32();
    xpProgress = r.readF32();

    // Reset transient state
    velocity = glm::vec3(0.0f);
    dead = (hp <= 0);
    fallStartY = position.y;
    wasFalling = false;
    eatingTicks = 0;
    isEating = false;
    hurtTicks = 0;
    attackCooldownTicks = 0;
    swinging = false;
    swingTicks = 0;
    swingProgress = 0.0f;
    prevSwingProgress = 0.0f;
}

// ========== Raycasting (DDA algorithm) ==========
// More accurate than fixed-step marching

RayHit raycastWorld(const World& world, const glm::vec3& origin,
                    const glm::vec3& direction, float maxDistance) {
    RayHit result;

    // Current block position
    int x = static_cast<int>(std::floor(origin.x));
    int y = static_cast<int>(std::floor(origin.y));
    int z = static_cast<int>(std::floor(origin.z));

    // Direction signs
    int stepX = (direction.x >= 0) ? 1 : -1;
    int stepY = (direction.y >= 0) ? 1 : -1;
    int stepZ = (direction.z >= 0) ? 1 : -1;

    // Distance to next grid boundary along each axis
    float tMaxX = (direction.x != 0.0f)
        ? ((direction.x > 0 ? (x + 1.0f) : static_cast<float>(x)) - origin.x) / direction.x
        : 1e30f;
    float tMaxY = (direction.y != 0.0f)
        ? ((direction.y > 0 ? (y + 1.0f) : static_cast<float>(y)) - origin.y) / direction.y
        : 1e30f;
    float tMaxZ = (direction.z != 0.0f)
        ? ((direction.z > 0 ? (z + 1.0f) : static_cast<float>(z)) - origin.z) / direction.z
        : 1e30f;

    // How far along the ray we must move to cross one cell in each direction
    float tDeltaX = (direction.x != 0.0f) ? std::abs(1.0f / direction.x) : 1e30f;
    float tDeltaY = (direction.y != 0.0f) ? std::abs(1.0f / direction.y) : 1e30f;
    float tDeltaZ = (direction.z != 0.0f) ? std::abs(1.0f / direction.z) : 1e30f;

    float t = 0.0f;
    int prevX = x, prevY = y, prevZ = z;
    Direction hitFace = Direction::PosY;

    const auto& registry = BlockRegistry::instance();

    while (t < maxDistance) {
        BlockId block = world.getBlock(x, y, z);
        if (!registry.isAir(block) && !registry.isLiquid(block)) {
            result.hit = true;
            result.blockX = x;
            result.blockY = y;
            result.blockZ = z;
            result.prevX = prevX;
            result.prevY = prevY;
            result.prevZ = prevZ;
            result.distance = t;
            result.face = hitFace;
            return result;
        }

        prevX = x;
        prevY = y;
        prevZ = z;

        // Step to next voxel boundary
        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            t = tMaxX;
            x += stepX;
            tMaxX += tDeltaX;
            hitFace = (stepX > 0) ? Direction::NegX : Direction::PosX;
        } else if (tMaxY < tMaxZ) {
            t = tMaxY;
            y += stepY;
            tMaxY += tDeltaY;
            hitFace = (stepY > 0) ? Direction::NegY : Direction::PosY;
        } else {
            t = tMaxZ;
            z += stepZ;
            tMaxZ += tDeltaZ;
            hitFace = (stepZ > 0) ? Direction::NegZ : Direction::PosZ;
        }
    }

    return result;
}
