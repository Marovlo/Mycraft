#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/common.h"
#include "core/block.h"

class World;

class Player {
public:
    Player();

    // Position (feet position, eye is at position + eyeOffset)
    glm::vec3 position;     // Feet position
    glm::vec3 velocity;

    // Camera
    float yaw   = -90.0f;
    float pitch  = 0.0f;
    float fov    = 70.0f;
    float sensitivity = 0.1f;

    // State
    bool onGround = false;
    bool sprinting = false;

    // Selected block for placement
    BlockId selectedBlock = Block::Grass;

    // Eye position (world space)
    glm::vec3 getEyePosition() const;

    // Camera vectors
    glm::vec3 getForward() const;
    glm::vec3 getFlatForward() const;
    glm::vec3 getRight() const;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;

    // Look (apply mouse delta)
    void look(double deltaX, double deltaY);
};

// ========== Raycasting ==========

struct RayHit {
    bool hit = false;
    int blockX, blockY, blockZ;       // Block that was hit
    int prevX, prevY, prevZ;          // Air block before hit (for placement)
    float distance = 0.0f;
    Direction face;                    // Which face was hit
};

// Cast a ray into the world, returns first solid block hit
RayHit raycastWorld(const World& world, const glm::vec3& origin,
                    const glm::vec3& direction, float maxDistance);
