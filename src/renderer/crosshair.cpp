#include "crosshair.h"
#include <vector>

void Crosshair::update(VulkanEngine& engine, const glm::vec3& eyePos,
                       const glm::vec3& forward, const glm::vec3& right, const glm::vec3& up) {
    // Destroy old mesh
    if (mesh_.indexCount > 0) {
        engine.destroyMesh(mesh_);
        mesh_ = {};
    }

    // Place a tiny white cross 0.5m in front of the camera
    float dist = 0.5f;
    float halfLen = 0.004f;
    float halfThk = 0.0006f;

    glm::vec3 c = eyePos + forward * dist;
    glm::vec3 n = -forward;  // face toward camera
    glm::vec2 uv(0.0f, 0.0f);  // sample top-left pixel of atlas

    auto r = right;
    auto u = up;

    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;

    auto quad = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3) {
        uint32_t b = static_cast<uint32_t>(verts.size());
        verts.push_back({v0, n, uv});
        verts.push_back({v1, n, uv});
        verts.push_back({v2, n, uv});
        verts.push_back({v3, n, uv});
        idx.push_back(b); idx.push_back(b+1); idx.push_back(b+2);
        idx.push_back(b); idx.push_back(b+2); idx.push_back(b+3);
    };

    // Horizontal bar
    quad(c - r*halfLen - u*halfThk, c - r*halfLen + u*halfThk,
         c + r*halfLen + u*halfThk, c + r*halfLen - u*halfThk);

    // Vertical bar
    quad(c - r*halfThk - u*halfLen, c - r*halfThk + u*halfLen,
         c + r*halfThk + u*halfLen, c + r*halfThk - u*halfLen);

    mesh_ = engine.uploadMesh(verts, idx);
}

void Crosshair::render(VkCommandBuffer cmd) {
    if (mesh_.indexCount == 0) return;

    VkBuffer vb[] = {mesh_.vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, mesh_.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mesh_.indexCount, 1, 0, 0, 0);
}

void Crosshair::destroy(VulkanEngine& engine) {
    if (mesh_.indexCount > 0) {
        engine.destroyMesh(mesh_);
        mesh_ = {};
    }
}
