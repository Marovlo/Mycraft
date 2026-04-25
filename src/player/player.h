#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/common.h"
#include "core/block.h"

class World;
class BinaryWriter;
class BinaryReader;

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
    bool sneaking  = false;

    // Health (MC: 20 HP = 10 hearts)
    int   hp    = 20;
    int   maxHp = 20;
    bool  dead  = false;
    glm::vec3 spawnPoint {0.0f, 100.0f, 0.0f};

    // Hunger (MC: 20 points = 10 drumsticks)
    int   hunger    = 20;
    int   maxHunger = 20;
    float saturation = 5.0f;    // hidden buffer, consumed before hunger
    int   hungerTickTimer = 0;  // for periodic regen / starve ticks

    // Fall damage tracking
    float fallStartY = 0.0f;
    bool  wasFalling = false;

    // Eating state: right-click hold on food → 32 ticks to eat.
    int  eatingTicks = 0;     // ticks spent eating, 0 = not eating
    bool isEating    = false;

    // Attack cooldown (MC Java 1.9+). Full strength after cooldown expires.
    int attackCooldownTicks    = 0;
    int attackCooldownMax      = 10;

    // Breathing / underwater (MC: 300 ticks = 15 seconds of air)
    int  air       = 300;    // current air supply (ticks remaining)
    int  maxAir    = 300;
    bool inWater   = false;  // head submerged in water
    bool isSwimming = false; // actively swimming (space in water)

    // Damage the player. Clamps to 0; sets dead flag. Returns actual damage dealt.
    int takeDamage(int amount);

    // Hurt visual effect: ticks remaining for screen shake + red flash.
    // Set by takeDamage(), decremented each tick.
    int hurtTicks = 0;

    // 无敌帧：受伤后短暂无敌（MC: 10 ticks = 0.5s）
    int invulnerableTicks = 0;

    // Full heal + reset state. Used on respawn.
    void respawn();

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

    // Serialization
    void serialize(BinaryWriter& w) const;
    void deserialize(BinaryReader& r);
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
