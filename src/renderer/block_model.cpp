#include "block_model.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>

void BlockModelRenderer::init(VulkanEngine& /*engine*/, const TextureAtlas& /*atlas*/) {
    // Stateless: nothing to allocate. Kept for symmetry with other renderers.
}

void BlockModelRenderer::destroy(VulkanEngine& /*engine*/) {
    // Nothing to release.
}

glm::mat4 BlockModelRenderer::getIconViewProj() const {
    // MC-style inventory icon view:
    //   * R_x(+30°): pitches the world forward so the top face comes into view
    //   * R_y(-45°): yaws so a vertical edge faces the camera, exposing
    //     top / east(+X) / south(+Z) as the three visible faces.
    //
    // Derivation of camera direction in world space:
    //   view = R_x(+30°) * R_y(-45°) * T(-0.5)
    //   R^T  = R_y(+45°) * R_x(-30°)
    //   camDir = R^T * (0,0,1) = (+0.612, +0.5, +0.612)
    // which yields positive dot-products with (+Y), (+X), (+Z) normals — the
    // three faces we want.
    const float scale = 1.05f;   // >1 so the hex corners don't touch the rect edges
    glm::mat4 proj = glm::ortho(-scale, scale, -scale, scale, -5.0f, 5.0f);

    glm::mat4 view = glm::mat4(1.0f);
    view = glm::rotate(view, glm::radians(30.0f),  glm::vec3(1, 0, 0));
    view = glm::rotate(view, glm::radians(-45.0f), glm::vec3(0, 1, 0));
    view = glm::translate(view, glm::vec3(-0.5f, -0.5f, -0.5f));
    return proj * view;
}

void BlockModelRenderer::enqueueBlockIcon(UIRenderer& ui, BlockId blockId,
                                          const TextureAtlas& atlas,
                                          float screenX, float screenY, float size) const {
    if (blockId == Block::Air) return;

    const auto& props = BlockRegistry::instance().get(blockId);
    const glm::mat4 mvp = getIconViewProj();

    // Each face: four CCW corners (when viewed from outside), normal, texture id, brightness.
    // Brightness mirrors the soft-lighting hack used in many MC clones (top brightest,
    // bottom darkest, side faces in between).
    struct Face {
        glm::vec3 v0, v1, v2, v3;
        glm::vec3 normal;
        uint16_t  texId;
        float     brightness;
    };

    const std::array<Face, 6> faces = {{
        // top
        {{0,1,1},{1,1,1},{1,1,0},{0,1,0}, {0, 1, 0}, props.textures.top,    1.00f},
        // bottom
        {{0,0,0},{1,0,0},{1,0,1},{0,0,1}, {0,-1, 0}, props.textures.bottom, 0.50f},
        // east  (+X)
        {{1,0,0},{1,1,0},{1,1,1},{1,0,1}, {1, 0, 0}, props.textures.east,   0.85f},
        // west  (-X)
        {{0,0,1},{0,1,1},{0,1,0},{0,0,0}, {-1,0, 0}, props.textures.west,   0.70f},
        // south (+Z)
        {{1,0,1},{1,1,1},{0,1,1},{0,0,1}, {0, 0, 1}, props.textures.south,  0.70f},
        // north (-Z)
        {{0,0,0},{0,1,0},{1,1,0},{1,0,0}, {0, 0,-1}, props.textures.north,  0.80f},
    }};

    // World → screen pixel inside the icon rect. NDC [-1,1] → rect [screen, screen+size].
    auto toScreen = [&](glm::vec3 worldPos) -> glm::vec2 {
        glm::vec4 clip = mvp * glm::vec4(worldPos, 1.0f);
        float px = (clip.x * 0.5f + 0.5f) * size + screenX;
        float py = (-clip.y * 0.5f + 0.5f) * size + screenY; // flip Y for screen
        return {px, py};
    };

    // The isometric view we use looks at the cube from roughly (+x, +y, +z).
    // See getIconViewProj() for the derivation — the camera direction in world
    // space is (+0.612, +0.5, +0.612), making top/east/south the front faces.
    const glm::vec3 cameraDir = glm::normalize(glm::vec3(0.612f, 0.5f, 0.612f));

    // Collect visible faces and sort them back-to-front using projected Z so the
    // nearer face covers the farther ones (painter's algorithm → looks *convex*).
    struct VisibleFace {
        const Face* face;
        float depth;   // larger = farther; we draw large first.
    };
    std::array<VisibleFace, 6> visible{};
    int visibleCount = 0;
    for (const auto& f : faces) {
        if (glm::dot(f.normal, cameraDir) <= 0.0f) continue;
        // Use face centroid for depth sort; stable for axis-aligned quads.
        glm::vec3 center = (f.v0 + f.v1 + f.v2 + f.v3) * 0.25f;
        glm::vec4 clip = mvp * glm::vec4(center, 1.0f);
        visible[visibleCount++] = {&f, clip.z};
    }
    std::sort(visible.begin(), visible.begin() + visibleCount,
              [](const VisibleFace& a, const VisibleFace& b) {
                  return a.depth > b.depth; // far first
              });

    for (int vi = 0; vi < visibleCount; ++vi) {
        const Face& f = *visible[vi].face;

        glm::vec4 uvRect = atlas.getTileUV(f.texId); // (u0,v0,u1,v1)
        glm::vec2 uv0(uvRect.x, uvRect.y);
        glm::vec2 uv1(uvRect.x, uvRect.w);
        glm::vec2 uv2(uvRect.z, uvRect.w);
        glm::vec2 uv3(uvRect.z, uvRect.y);

        glm::vec2 sv0 = toScreen(f.v0);
        glm::vec2 sv1 = toScreen(f.v1);
        glm::vec2 sv2 = toScreen(f.v2);
        glm::vec2 sv3 = toScreen(f.v3);

        glm::vec4 tint(f.brightness, f.brightness, f.brightness, 1.0f);
        UIVertex p0{sv0, uv0, tint};
        UIVertex p1{sv1, uv1, tint};
        UIVertex p2{sv2, uv2, tint};
        UIVertex p3{sv3, uv3, tint};

        // Quad → two triangles. UI pipeline disables back-face culling, so
        // winding order doesn't matter here.
        ui.drawTexturedTri(p0, p1, p2);
        ui.drawTexturedTri(p0, p2, p3);
    }
}
