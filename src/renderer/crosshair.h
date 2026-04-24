#pragma once

#include "engine/vulkan_engine.h"
#include <glm/glm.hpp>

// Renders a crosshair at screen center.
// Builds a tiny 3D mesh in front of the camera each frame.
class Crosshair {
public:
    // Update: rebuild mesh positioned in front of camera. Call during update().
    void update(VulkanEngine& engine, const glm::vec3& eyePos,
                const glm::vec3& forward, const glm::vec3& right, const glm::vec3& up);

    // Draw: render the pre-built mesh. Call during render().
    void render(VkCommandBuffer cmd);

    // Cleanup
    void destroy(VulkanEngine& engine);

private:
    Mesh mesh_;
};
