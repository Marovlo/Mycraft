// game_highlight.cpp — 方块选择高亮 + 破坏裂纹覆盖层
// 从 game.cpp 的 update() 中提取。
// 每帧调用 updateBlockHighlight()，只在目标方块或破坏阶段变化时重建 mesh。

#include "game.h"
#include "player/physics.h"
#include <algorithm>
#include <cmath>
#include <string>

void Game::updateBlockHighlight() {
    hasTarget_ = false;
    int curTargetX = INT_MIN, curTargetY = INT_MIN, curTargetZ = INT_MIN;
    int curBreakStage = -1;

    int bbx, bby, bbz;
    float bProg = -1.0f;
    blockInteraction_.getActiveBreak(bbx, bby, bbz, bProg);

    if (input_.isCursorLocked()) {
        RayHit hit = raycastWorld(world_, player_.getEyePosition(),
                                  player_.getForward(), MAX_REACH);
        if (hit.hit) {
            hasTarget_ = true;
            curTargetX = hit.blockX;
            curTargetY = hit.blockY;
            curTargetZ = hit.blockZ;

            if (bProg > 0.0f) {
                curBreakStage = std::clamp(static_cast<int>(bProg * 10.0f), 0, 9);
            }

            // 只在目标方块坐标变化时重建高亮 mesh
            bool targetChanged = (curTargetX != prevTargetX_ ||
                                  curTargetY != prevTargetY_ ||
                                  curTargetZ != prevTargetZ_);
            if (targetChanged) {
                if (targetHighlight_.indexCount > 0) {
                    engine_.destroyMesh(targetHighlight_);
                    targetHighlight_ = {};
                }

                float e = 0.002f;
                float bx = static_cast<float>(hit.blockX);
                float by = static_cast<float>(hit.blockY);
                float bz = static_cast<float>(hit.blockZ);

                std::vector<Vertex> verts;
                std::vector<uint32_t> idx;
                glm::vec3 n(0, 1, 0);
                glm::vec2 uv(0, 0);

                float highlightLight = -0.55f;

                auto addLine = [&](glm::vec3 a, glm::vec3 b, glm::vec3 offset) {
                    uint32_t base = static_cast<uint32_t>(verts.size());
                    verts.push_back({a - offset, n, uv, highlightLight});
                    verts.push_back({a + offset, n, uv, highlightLight});
                    verts.push_back({b + offset, n, uv, highlightLight});
                    verts.push_back({b - offset, n, uv, highlightLight});
                    idx.push_back(base); idx.push_back(base+1); idx.push_back(base+2);
                    idx.push_back(base); idx.push_back(base+2); idx.push_back(base+3);
                };

                float t = 0.005f;
                glm::vec3 corners[8] = {
                    {bx-e, by-e, bz-e}, {bx+1+e, by-e, bz-e},
                    {bx+1+e, by-e, bz+1+e}, {bx-e, by-e, bz+1+e},
                    {bx-e, by+1+e, bz-e}, {bx+1+e, by+1+e, bz-e},
                    {bx+1+e, by+1+e, bz+1+e}, {bx-e, by+1+e, bz+1+e},
                };
                addLine(corners[0], corners[1], glm::vec3(0, t, 0));
                addLine(corners[1], corners[2], glm::vec3(0, t, 0));
                addLine(corners[2], corners[3], glm::vec3(0, t, 0));
                addLine(corners[3], corners[0], glm::vec3(0, t, 0));
                addLine(corners[4], corners[5], glm::vec3(0, t, 0));
                addLine(corners[5], corners[6], glm::vec3(0, t, 0));
                addLine(corners[6], corners[7], glm::vec3(0, t, 0));
                addLine(corners[7], corners[4], glm::vec3(0, t, 0));
                addLine(corners[0], corners[4], glm::vec3(t, 0, 0));
                addLine(corners[1], corners[5], glm::vec3(t, 0, 0));
                addLine(corners[2], corners[6], glm::vec3(t, 0, 0));
                addLine(corners[3], corners[7], glm::vec3(t, 0, 0));

                targetHighlight_ = engine_.uploadMesh(verts, idx);
            }

            // 只在破坏阶段变化或目标变化时重建破坏覆盖层
            bool overlayChanged = targetChanged || (curBreakStage != prevBreakStage_);
            if (overlayChanged) {
                if (breakOverlay_.indexCount > 0) {
                    engine_.destroyMesh(breakOverlay_);
                    breakOverlay_ = {};
                }

                if (curBreakStage >= 0) {
                    float bx = static_cast<float>(hit.blockX);
                    float by = static_cast<float>(hit.blockY);
                    float bz = static_cast<float>(hit.blockZ);

                    std::string stageName = "destroy_stage_" + std::to_string(curBreakStage);
                    uint16_t tile = textureAtlas_.getTileIndex(stageName);
                    glm::vec4 uvRect = textureAtlas_.getTileUV(tile);
                    glm::vec2 uv0(uvRect.x, uvRect.y);
                    glm::vec2 uv1(uvRect.x, uvRect.w);
                    glm::vec2 uv2(uvRect.z, uvRect.w);
                    glm::vec2 uv3(uvRect.z, uvRect.y);

                    float oe = 0.001f;
                    float x0 = bx - oe, y0 = by - oe, z0 = bz - oe;
                    float x1 = bx + 1 + oe, y1 = by + 1 + oe, z1 = bz + 1 + oe;

                    std::vector<Vertex> ov;
                    std::vector<uint32_t> oi;
                    auto addFace = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 fn) {
                        uint32_t base = static_cast<uint32_t>(ov.size());
                        ov.push_back({a, fn, uv0, 1.0f});
                        ov.push_back({b, fn, uv1, 1.0f});
                        ov.push_back({c, fn, uv2, 1.0f});
                        ov.push_back({d, fn, uv3, 1.0f});
                        oi.push_back(base); oi.push_back(base+1); oi.push_back(base+2);
                        oi.push_back(base); oi.push_back(base+2); oi.push_back(base+3);
                    };

                    addFace({x1,y0,z0},{x1,y1,z0},{x1,y1,z1},{x1,y0,z1}, {1,0,0});
                    addFace({x0,y0,z1},{x0,y1,z1},{x0,y1,z0},{x0,y0,z0}, {-1,0,0});
                    addFace({x0,y1,z1},{x1,y1,z1},{x1,y1,z0},{x0,y1,z0}, {0,1,0});
                    addFace({x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1}, {0,-1,0});
                    addFace({x1,y0,z1},{x1,y1,z1},{x0,y1,z1},{x0,y0,z1}, {0,0,1});
                    addFace({x0,y0,z0},{x0,y1,z0},{x1,y1,z0},{x1,y0,z0}, {0,0,-1});

                    breakOverlay_ = engine_.uploadMesh(ov, oi);
                }
            }
        }
    }

    // 目标消失时清理缓存的 mesh
    if (!hasTarget_) {
        if (targetHighlight_.indexCount > 0) {
            engine_.destroyMesh(targetHighlight_);
            targetHighlight_ = {};
        }
        if (breakOverlay_.indexCount > 0) {
            engine_.destroyMesh(breakOverlay_);
            breakOverlay_ = {};
        }
        curTargetX = INT_MIN;
        curTargetY = INT_MIN;
        curTargetZ = INT_MIN;
        curBreakStage = -1;
    }

    prevTargetX_ = curTargetX;
    prevTargetY_ = curTargetY;
    prevTargetZ_ = curTargetZ;
    prevBreakStage_ = curBreakStage;
}
