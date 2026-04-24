#include "player.h"
#include "world/world.h"
#include <cmath>
#include <algorithm>

Player::Player() {
    position = glm::vec3(0.0f, 80.0f, 0.0f);
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
    auto proj = glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 1000.0f);
    proj[1][1] *= -1; // Vulkan Y flip
    return proj;
}

void Player::look(double deltaX, double deltaY) {
    yaw   += static_cast<float>(deltaX) * sensitivity;
    pitch -= static_cast<float>(deltaY) * sensitivity;
    pitch  = std::clamp(pitch, -89.0f, 89.0f);
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
