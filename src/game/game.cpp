#include "game.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>
#include <GLFW/glfw3.h>
#include "core/item.h"
#include "core/debug.h"
#include "core/serialization.h"
#include "world/light_engine.h"
#include <unordered_map>
#include <filesystem>

Game::Game() = default;

Game::~Game() {
    // Save all data before cleanup
    saveAll();

    for (auto& [key, chunk] : world_.chunks()) {
        if (chunk.hasMesh()) {
            engine_.destroyMesh(chunk.getMesh());
        }
        if (chunk.hasTransparentMesh()) {
            engine_.destroyMesh(chunk.getTransparentMesh());
        }
    }
    if (targetHighlight_.indexCount > 0) {
        engine_.destroyMesh(targetHighlight_);
    }
    if (breakOverlay_.indexCount > 0) {
        engine_.destroyMesh(breakOverlay_);
    }
    blockModel_.destroy(engine_);
    entityRenderer_.destroy();
    uiRenderer_.destroy();
    textureAtlas_.destroy(engine_);
    engine_.cleanup();
}

// ============================================================
// Screen management
// ============================================================

void Game::openScreen(ContainerScreen* screen) {
    if (activeScreen_) activeScreen_->close(inventory_);
    activeScreen_ = screen;
    activeScreen_->open();
    input_.setCursorLocked(false);
    blockInteraction_.reset();
    leftMouseHeld_ = false;
    if (player_.isEating) {
        player_.isEating = false;
        player_.eatingTicks = 0;
    }
}

void Game::closeActiveScreen() {
    if (!activeScreen_) return;
    activeScreen_->close(inventory_);
    activeScreen_ = nullptr;
    input_.setCursorLocked(true);
}

// ============================================================
// Block interaction registry
// ============================================================

void Game::registerBlockInteractions() {
    blockUseHandlers_[Block::CraftingTable] = [](Game& g, int, int, int) {
        g.openScreen(&g.craftingScreen_);
    };
    blockUseHandlers_[Block::Furnace] = [](Game& g, int bx, int by, int bz) {
        auto& data = g.furnaceManager_.getOrCreate(bx, by, bz);
        g.furnaceScreen_.setFurnaceData(&data);
        g.openScreen(&g.furnaceScreen_);
    };
    blockUseHandlers_[Block::Chest] = [](Game& g, int bx, int by, int bz) {
        auto& inv = g.chestManager_.getOrCreate(bx, by, bz);
        g.chestScreen_.setChestInventory(&inv);
        g.openScreen(&g.chestScreen_);
    };

    // When a container block is broken, drop all its contents as item entities
    blockInteraction_.onBlockBroken = [this](BlockId blockId, int bx, int by, int bz,
                                              EntityManager& entityMgr) {
        glm::vec3 centre(bx + 0.5f, by + 0.5f, bz + 0.5f);
        if (blockId == Block::Chest) {
            auto contents = chestManager_.remove(bx, by, bz);
            for (const auto& slot : contents) {
                if (!slot.isEmpty()) {
                    entityMgr.spawnItem(centre, slot);
                }
            }
        } else if (blockId == Block::Furnace) {
            // 如果正在查看这个熔炉，先关闭界面
            if (activeScreen_ == &furnaceScreen_) {
                closeActiveScreen();
            }
            auto contents = furnaceManager_.remove(bx, by, bz);
            if (!contents.inputSlot.isEmpty())
                entityMgr.spawnItem(centre, contents.inputSlot);
            if (!contents.fuelSlot.isEmpty())
                entityMgr.spawnItem(centre, contents.fuelSlot);
            if (!contents.outputSlot.isEmpty())
                entityMgr.spawnItem(centre, contents.outputSlot);
        }
    };
}

// ============================================================
// Init / Run
// ============================================================

void Game::init() {
    BlockRegistry::instance().registerDefaults();
    ItemRegistry::instance().registerDefaults();
    RecipeRegistry::instance().registerDefaults();
    SmeltingRegistry::instance().registerDefaults();

    if (!engine_.init(1280, 720, "VoxelCraft")) {
        throw std::runtime_error("Failed to init engine");
    }

    input_.init(engine_.getWindow());

    // Set up save directory (creates saves/Default World/ if needed)
    saveManager_.setWorld("Default World");

    // Try loading existing level data (seed, ticks, spawn)
    bool hasExistingSave = false;
    {
        int64_t seed = worldSeed_;
        uint64_t totalTicks = 0;
        float spawnX = 0.5f, spawnY = 100.0f, spawnZ = 0.5f;
        std::string name;
        if (saveManager_.loadLevelData(seed, totalTicks, spawnX, spawnY, spawnZ, name)) {
            worldSeed_ = seed;
            player_.spawnPoint = glm::vec3(spawnX, spawnY, spawnZ);
            hasExistingSave = true;
            std::cout << "[Save] Loaded world \"" << name << "\" (seed=" << seed
                      << ", ticks=" << totalTicks << ")\n";
        }
    }

    terrainGen_ = std::make_unique<OverworldGenerator>(static_cast<int>(worldSeed_));

    loadTextureAtlas();
    uiRenderer_.init(&engine_);
    uiRenderer_.setAtlas(&textureAtlas_);
    blockModel_.init(engine_, textureAtlas_);
    entityRenderer_.init(&engine_, &textureAtlas_);

    hud_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    inventoryScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    craftingScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    furnaceScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    chestScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);

    registerBlockInteractions();

    // Try loading player data from existing save
    bool playerLoaded = false;
    if (hasExistingSave) {
        playerLoaded = saveManager_.loadPlayer(player_, inventory_);
        if (playerLoaded) {
            prevPlayerPos_ = player_.position;
            std::cout << "[Save] Player loaded at ("
                      << player_.position.x << ", "
                      << player_.position.y << ", "
                      << player_.position.z << ")\n";
        }
        // Restore dropped item entities
        if (saveManager_.loadEntities(entityManager_)) {
            std::cout << "[Save] Loaded " << entityManager_.count() << " entities.\n";
        }
        // Restore chest contents
        {
            std::string chestPath = saveManager_.getWorldDir() + "/chests.dat";
            if (SaveManager::fileExists(chestPath)) {
                BinaryReader r(chestPath);
                if (r.isValid()) {
                    uint32_t magic = r.readU32();
                    uint16_t ver = r.readU16();
                    if (magic == 0x43485354 && ver <= 1) {
                        chestManager_.deserialize(r);
                        std::cout << "[Save] Loaded " << chestManager_.count() << " chests.\n";
                    }
                }
            }
        }
        // Restore furnace contents
        {
            std::string furnacePath = saveManager_.getWorldDir() + "/furnaces.dat";
            if (SaveManager::fileExists(furnacePath)) {
                BinaryReader r(furnacePath);
                if (r.isValid()) {
                    uint32_t magic = r.readU32();
                    uint16_t ver = r.readU16();
                    if (magic == 0x46524E43 && ver <= 1) {
                        furnaceManager_.deserialize(r);
                        std::cout << "[Save] Loaded " << furnaceManager_.count() << " furnaces.\n";
                    }
                }
            }
        }
    }

    // New world: compute spawn point and give starter items
    if (!playerLoaded) {
        auto* gen = dynamic_cast<OverworldGenerator*>(terrainGen_.get());
        int surfaceY = gen ? gen->getTerrainHeight(0, 0) : SEA_LEVEL;
        int spawnY = std::max(surfaceY, SEA_LEVEL) + 1;
        player_.position   = glm::vec3(0.5f, static_cast<float>(spawnY), 0.5f);
        player_.spawnPoint = player_.position;
        player_.fallStartY = player_.position.y;
        prevPlayerPos_     = player_.position;

        inventory_.getSlot(0) = {Item::IronPickaxe,  1, 0};
        inventory_.getSlot(1) = {Item::WoodenAxe,     1, 0};
        inventory_.getSlot(2) = {Item::WoodenShovel,  1, 0};
        inventory_.getSlot(3) = {Item::WoodenPickaxe,  1, 0};
        inventory_.getSlot(4) = {Item::Torch,          64, 0};
    }

engine_.onUpdate = [this](float dt) { update(dt); };
    engine_.onRender = [this](VkCommandBuffer cmd, uint32_t) { render(cmd); };
}

void Game::run() {
    engine_.run();
}

// ============================================================
// Save / Load
// ============================================================

void Game::saveAll() {
    // Save player state + inventory
    saveManager_.savePlayer(player_, inventory_);

    // Save dropped item entities
    saveManager_.saveEntities(entityManager_);

    // Save chest contents
    {
        std::string chestPath = saveManager_.getWorldDir() + "/chests.dat";
        std::string tmpPath = chestPath + ".tmp";
        BinaryWriter w(tmpPath);
        if (w.isValid()) {
            w.writeU32(0x43485354); // "CHST" magic
            w.writeU16(1);          // version
            chestManager_.serialize(w);
            w.close();
            std::error_code ec;
            std::filesystem::rename(tmpPath, chestPath, ec);
            if (ec) std::filesystem::remove(tmpPath, ec);
        }
    }

    // Save furnace contents
    {
        std::string furnacePath = saveManager_.getWorldDir() + "/furnaces.dat";
        std::string tmpPath = furnacePath + ".tmp";
        BinaryWriter w(tmpPath);
        if (w.isValid()) {
            w.writeU32(0x46524E43); // "FRNC" magic
            w.writeU16(1);          // version
            furnaceManager_.serialize(w);
            w.close();
            std::error_code ec;
            std::filesystem::rename(tmpPath, furnacePath, ec);
            if (ec) std::filesystem::remove(tmpPath, ec);
        }
    }

    // Save all modified chunks
    int chunksSaved = saveManager_.saveAllDirtyChunks(world_);

    // Save level metadata
    saveManager_.saveLevelData(
        worldSeed_,
        tickClock_.getTotalTicks(),
        player_.spawnPoint.x, player_.spawnPoint.y, player_.spawnPoint.z,
        saveManager_.getWorldName()
    );

    // Flush region file caches
    saveManager_.closeAllRegions();

    if (chunksSaved > 0) {
        std::cout << "[Save] World saved (" << chunksSaved << " chunks).\n";
    } else {
        std::cout << "[Save] World saved.\n";
    }
}

void Game::loadWorld() {
    saveManager_.loadPlayer(player_, inventory_);
}

void Game::loadTextureAtlas() {
    std::string texDir = std::string(ASSET_DIR) + "/textures/blocks";

    if (!textureAtlas_.build(engine_, texDir, 16)) {
        throw std::runtime_error("Failed to build texture atlas from: " + texDir);
    }

    std::unordered_map<std::string, uint16_t> nameMap;
    auto& registry = BlockRegistry::instance();
    for (uint16_t id = 0; id < registry.blockCount(); id++) {
        const auto& props = registry.get(id);
        auto addName = [&](const std::string& n) {
            if (!n.empty() && nameMap.find(n) == nameMap.end()) {
                nameMap[n] = textureAtlas_.getTileIndex(n);
            }
        };
        addName(props.textureNames.top);
        addName(props.textureNames.bottom);
        addName(props.textureNames.north);
        addName(props.textureNames.south);
        addName(props.textureNames.east);
        addName(props.textureNames.west);
    }
    registry.resolveTextures(nameMap);

    engine_.updateTextureDescriptor(textureAtlas_.getImage().imageView, engine_.getDefaultSampler());
    meshBuilder_.setAtlas(&textureAtlas_);
}

// ============================================================
// Update / Tick
// ============================================================

void Game::update(float dt) {
    double now = glfwGetTime();
    int ticks = tickClock_.advance(now);

    // FPS 计数器：每秒更新一次
    fpsFrameCount_++;
    fpsTimer_ += static_cast<double>(dt);
    if (fpsTimer_ >= 1.0) {
        fps_ = fpsFrameCount_;
        fpsFrameCount_ = 0;
        fpsTimer_ -= 1.0;
    }

    handleFrameInput();

    for (int i = 0; i < ticks; i++) {
        gameTick();
    }

    input_.update();
    input_.postUpdate();

    buildMeshes();

    float partial = tickClock_.getPartialTick();
    glm::vec3 renderPos = glm::mix(prevPlayerPos_, player_.position, partial);
    float eyeH = player_.sneaking ? SNEAK_EYE_HEIGHT : PLAYER_EYE_HEIGHT;
    glm::vec3 renderEye = renderPos + glm::vec3(0.0f, eyeH, 0.0f);

    // Camera shake on damage
    if (player_.hurtTicks > 0) {
        float intensity = static_cast<float>(player_.hurtTicks) / 10.0f * 0.15f;
        float t = static_cast<float>(tickClock_.getTotalTicks()) * 17.3f;
        renderEye.x += std::sin(t) * intensity;
        renderEye.y += std::cos(t * 1.3f) * intensity * 0.7f;
    }

    float aspect = static_cast<float>(engine_.getWindowWidth()) /
                   static_cast<float>(engine_.getWindowHeight());

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view  = glm::lookAt(renderEye, renderEye + player_.getForward(), glm::vec3(0, 1, 0));
    ubo.proj  = player_.getProjectionMatrix(aspect);
    ubo.fogColor = glm::vec4(0.53f, 0.81f, 0.92f, 1.0f);
    ubo.viewPos  = glm::vec4(renderEye, 1.0f);
    float fogStart = static_cast<float>((RENDER_DISTANCE - 2) * CHUNK_SIZE);
    float fogEnd   = static_cast<float>(RENDER_DISTANCE * CHUNK_SIZE);
    ubo.fogRange = glm::vec2(fogStart, fogEnd);

    // Detect if camera eye is submerged in water
    glm::vec3 eye = player_.getEyePosition();
    int eyeBx = static_cast<int>(std::floor(eye.x));
    int eyeBy = static_cast<int>(std::floor(eye.y));
    int eyeBz = static_cast<int>(std::floor(eye.z));
    bool eyeInWater = BlockRegistry::instance().isLiquid(world_.getBlock(eyeBx, eyeBy, eyeBz));
    ubo.underwater = eyeInWater ? 1.0f : 0.0f;
    ubo.waterSurfaceY = static_cast<float>(SEA_LEVEL);

    // Match clear color (sky) to fog color — underwater uses deep blue
    if (eyeInWater) {
        ubo.fogColor = glm::vec4(0.02f, 0.06f, 0.22f, 1.0f);
        engine_.setClearColor(0.02f, 0.06f, 0.22f);
    } else {
        engine_.setClearColor(0.53f, 0.81f, 0.92f);
    }

    float sw = static_cast<float>(engine_.getWindowWidth());
    float sh = static_cast<float>(engine_.getWindowHeight());
    int   bbx, bby, bbz;
    float bProg = -1.0f;
    blockInteraction_.getActiveBreak(bbx, bby, bbz, bProg);
    hud_.draw(sw, sh, inventory_, bProg, tickClock_.getTotalTicks(),
              player_.hp, player_.maxHp, player_.hunger, player_.maxHunger,
              player_.dead, player_.isEating, player_.air, player_.maxAir,
              player_.hurtTicks);

    // FPS 显示（F3 切换）
    if (showFps_) {
        int scale = std::clamp(static_cast<int>(sh / 240.0f), 2, 4);
        float glyphH = 7.0f * scale;
        float pad = 4.0f * scale;
        std::string fpsStr = std::to_string(fps_) + " FPS";
        // 半透明黑色背景
        float bgW = fpsStr.size() * (glyphH * 0.6f + glyphH * 0.1f) + pad;
        float bgH = glyphH + pad;
        uiRenderer_.drawRect(0, 0, bgW, bgH, glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
        uiRenderer_.drawTextLeft(fpsStr, pad * 0.5f, pad * 0.5f, glyphH,
                                 glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));
    }

    if (activeScreen_) {
        activeScreen_->draw(sw, sh, inventory_);
    }

    entityRenderer_.buildFrame(entityManager_, partial);
    engine_.updateUniformBuffer(ubo);

    // Build target block highlight (缓存：只在目标方块变化时重建)
    hasTarget_ = false;
    int curTargetX = INT_MIN, curTargetY = INT_MIN, curTargetZ = INT_MIN;
    int curBreakStage = -1;

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

void Game::gameTick() {
    const float dt = static_cast<float>(TickClock::TICK_DURATION);
    prevPlayerPos_ = player_.position;

    handleTickInput();
    if (!player_.dead) {
        Physics::update(player_, world_, dt);
    }

    if (player_.attackCooldownTicks > 0) --player_.attackCooldownTicks;
    if (player_.hurtTicks > 0) --player_.hurtTicks;

    // Fall damage
    if (!player_.onGround && !player_.wasFalling) {
        player_.fallStartY = player_.position.y;
        player_.wasFalling = true;
    }
    if (!player_.onGround && player_.position.y > player_.fallStartY) {
        player_.fallStartY = player_.position.y;
    }
    if (player_.onGround && player_.wasFalling) {
        float fallDist = player_.fallStartY - player_.position.y;
        if (fallDist > 3.0f) {
            int dmg = static_cast<int>(fallDist - 3.0f);
            player_.takeDamage(dmg);
            VLOG(DebugCat::Physics, "fall damage: dist=%.1f dmg=%d hp=%d", fallDist, dmg, player_.hp);
        }
        player_.wasFalling = false;
    }

    // Void damage
    if (player_.position.y < -64.0f && !player_.dead) {
        if (tickClock_.getTotalTicks() % 10 == 0) {
            player_.takeDamage(4);
            VLOG(DebugCat::Physics, "void damage: hp=%d", player_.hp);
        }
    }

    // Hunger / saturation
    if (player_.sprinting && glm::length(player_.velocity) > 0.01f) {
        player_.saturation -= 0.1f;
    }
    if (player_.saturation < 0.0f) {
        player_.hunger += static_cast<int>(player_.saturation);
        player_.saturation = 0.0f;
        if (player_.hunger < 0) player_.hunger = 0;
    }
    if (player_.saturation > static_cast<float>(player_.hunger)) {
        player_.saturation = static_cast<float>(player_.hunger);
    }

    ++player_.hungerTickTimer;

    if (player_.hunger >= 18 && player_.hp < player_.maxHp && player_.saturation > 0.0f) {
        if (player_.hungerTickTimer % 10 == 0) {
            player_.hp = std::min(player_.hp + 1, player_.maxHp);
            player_.saturation -= 1.5f;
            if (player_.saturation < 0.0f) {
                player_.hunger = std::max(0, player_.hunger - 1);
                player_.saturation = 0.0f;
            }
        }
    } else if (player_.hungerTickTimer >= 80) {
        player_.hungerTickTimer = 0;
        if (player_.hunger >= 18 && player_.hp < player_.maxHp) {
            player_.hp = std::min(player_.hp + 1, player_.maxHp);
            player_.hunger = std::max(0, player_.hunger - 1);
        } else if (player_.hunger <= 0 && player_.hp > 1) {
            player_.takeDamage(1);
        }
    }

    // Eating
    if (player_.isEating) {
        ++player_.eatingTicks;
        if (player_.eatingTicks >= 32) {
            ItemStack& held = inventory_.getHeldItem();
            if (!held.isEmpty()) {
                const auto& fp = ItemRegistry::instance().get(held.id);
                if (fp.type == ItemType::Food) {
                    player_.hunger = std::min(player_.hunger + fp.hungerRestore, player_.maxHunger);
                    player_.saturation = std::min(player_.saturation + fp.saturationRestore,
                                                  static_cast<float>(player_.hunger));
                    inventory_.consumeHeldItem(1);
                    VLOG(DebugCat::Input, "ate %s hunger=%d sat=%.1f",
                         fp.displayName.c_str(), player_.hunger, player_.saturation);
                }
            }
            player_.eatingTicks = 0;
            player_.isEating = false;
        }
    }

    // --- Water / breathing ---
    // Check if player's head (eye position) is submerged in water.
    {
        glm::vec3 eye = player_.getEyePosition();
        int ex = static_cast<int>(std::floor(eye.x));
        int ey = static_cast<int>(std::floor(eye.y));
        int ez = static_cast<int>(std::floor(eye.z));
        BlockId headBlock = world_.getBlock(ex, ey, ez);
        player_.inWater = (headBlock == Block::Water);

        if (player_.inWater) {
            // Drain air: 1 per tick → 300 ticks = 15 seconds
            if (player_.air > 0) {
                --player_.air;
            } else {
                // Drowning: 2 HP damage every 20 ticks (1 second)
                if (tickClock_.getTotalTicks() % 20 == 0) {
                    player_.takeDamage(2);
                    VLOG(DebugCat::Physics, "drowning damage: hp=%d", player_.hp);
                }
            }
        } else {
            // Restore air: 5 per tick when above water → refill in 3 seconds
            if (player_.air < player_.maxAir) {
                player_.air = std::min(player_.air + 5, player_.maxAir);
            }
        }
    }

    playerChunkX_ = blockToChunk(static_cast<int>(std::floor(player_.position.x)));
    playerChunkZ_ = blockToChunk(static_cast<int>(std::floor(player_.position.z)));

    updateChunks();
    unloadDistantChunks();

    blockInteraction_.tick(world_, player_, inventory_, entityManager_,
                            leftMouseHeld_, MAX_REACH);
    entityManager_.tick(world_, player_, inventory_);

    // Tick all furnaces (smelting progress, fuel consumption)
    furnaceManager_.tick();

    // --- Auto-save ---
    uint64_t ticks = tickClock_.getTotalTicks();
    if (ticks - lastAutoSaveTick_ >= AUTOSAVE_INTERVAL_TICKS && ticks > 0) {
        lastAutoSaveTick_ = ticks;
        saveManager_.savePlayer(player_, inventory_);
        saveManager_.saveLevelData(worldSeed_, ticks,
            player_.spawnPoint.x, player_.spawnPoint.y, player_.spawnPoint.z,
            saveManager_.getWorldName());
        VLOG(DebugCat::Save, "Auto-save at tick %llu", (unsigned long long)ticks);
    }

    // --- Incremental chunk save: spread IO across ticks ---
    {
        int saved = 0;
        for (auto& [key, chunk] : world_.chunks()) {
            if (saved >= INCREMENTAL_SAVE_PER_TICK) break;
            if (!chunk.isModified()) continue;
            saveManager_.saveChunk(chunk);
            chunk.clearModified();
            ++saved;
        }
    }
}

// ============================================================
// Input — split into focused sub-functions
// ============================================================

void Game::handleFrameInput() {
    // Dead: only R to respawn
    if (player_.dead) {
        if (input_.isKeyPressed(GLFW_KEY_R)) {
            player_.respawn();
            blockInteraction_.reset();
        }
        return;
    }

    // E key: toggle inventory / close any open screen
    if (input_.isKeyPressed(GLFW_KEY_E)) {
        if (activeScreen_) {
            closeActiveScreen();
        } else {
            openScreen(&inventoryScreen_);
        }
        return;
    }

    // When a screen is open, route input to it
    if (activeScreen_) {
        if (input_.isKeyPressed(GLFW_KEY_ESCAPE)) {
            closeActiveScreen();
            return;
        }
        float sw = static_cast<float>(engine_.getWindowWidth());
        float sh = static_cast<float>(engine_.getWindowHeight());
        activeScreen_->handleInput(inventory_, sw, sh,
                                   input_.getMouseX(), input_.getMouseY(),
                                   input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT),
                                   input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT),
                                   input_.isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT),
                                   input_.isKeyDown(GLFW_KEY_LEFT_SHIFT) || input_.isKeyDown(GLFW_KEY_RIGHT_SHIFT),
                                   input_.isKeyPressed(GLFW_KEY_R));
        return;
    }

    // No screen open — normal gameplay input
    handleGameplayInput();
}

void Game::handleGameplayInput() {
    // ESC
    if (input_.isKeyPressed(GLFW_KEY_ESCAPE)) {
        if (input_.isCursorLocked()) {
            input_.setCursorLocked(false);
        } else {
            glfwSetWindowShouldClose(engine_.getWindow(), GLFW_TRUE);
        }
    }

    // Click to re-lock
    if (!input_.isCursorLocked() && input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        input_.setCursorLocked(true);
    }

    // F3: 切换 FPS 显示
    if (input_.isKeyPressed(GLFW_KEY_F3)) {
        showFps_ = !showFps_;
    }

    // F2: Screenshot
    if (input_.isKeyPressed(GLFW_KEY_F2)) {
        std::string dir = std::string(ASSET_DIR) + "/../debug_output";
        std::string path = dir + "/screenshot_" + std::to_string(tickClock_.getTotalTicks()) + ".png";
        engine_.requestScreenshot(path);
    }

    // F4: Debug
    if (input_.isKeyPressed(GLFW_KEY_F4)) {
        std::cout << "\n=== DEBUG STATE (tick " << tickClock_.getTotalTicks() << ") ===\n";
        std::cout << "Player pos: " << player_.position.x << ", " << player_.position.y << ", " << player_.position.z << "\n";
        std::cout << "Player yaw/pitch: " << player_.yaw << " / " << player_.pitch << "\n";
        std::cout << "Chunk pos: " << playerChunkX_ << ", " << playerChunkZ_ << "\n";
        std::cout << "Loaded chunks: " << world_.chunks().size() << "\n";
        std::cout << "Selected slot: " << inventory_.getSelectedSlot();
        const auto& held = inventory_.getHeldItem();
        if (!held.isEmpty()) {
            std::cout << " -> ItemId " << held.id << " x" << held.count
                      << " (" << ItemRegistry::instance().get(held.id).displayName << ")";
        }
        std::cout << "\nWindow: " << engine_.getWindowWidth() << "x" << engine_.getWindowHeight() << "\n";
        std::cout << "=== END ===\n\n";
    }

    // F5: Teleport to surface at current XZ (useful for getting out of caves)
    if (input_.isKeyPressed(GLFW_KEY_F5)) {
        auto* gen = dynamic_cast<OverworldGenerator*>(terrainGen_.get());
        int px = static_cast<int>(std::floor(player_.position.x));
        int pz = static_cast<int>(std::floor(player_.position.z));
        int surfY = gen ? gen->getTerrainHeight(px, pz) : 80;
        surfY = std::max(surfY, SEA_LEVEL) + 1;
        player_.position.y = static_cast<float>(surfY);
        player_.velocity = glm::vec3(0);
        player_.fallStartY = player_.position.y;
        player_.wasFalling = false;
        std::cout << "[TP] Teleported to surface Y=" << surfY << "\n";
    }

    // F6: Locate all biomes (print coords of nearest example of each biome type)
    if (input_.isKeyPressed(GLFW_KEY_F6)) {
        auto* gen = dynamic_cast<OverworldGenerator*>(terrainGen_.get());
        if (gen) {
            int cx = static_cast<int>(std::floor(player_.position.x));
            int cz = static_cast<int>(std::floor(player_.position.z));
            const char* biomeNames[] = {"Plains", "Forest", "Desert", "Snowy"};
            std::cout << "\n=== LOCATE BIOMES (from " << cx << "," << cz << ") ===\n";
            // Search in expanding rings
            for (int biome = 0; biome < 4; ++biome) {
                bool found = false;
                for (int r = 0; r < 500 && !found; r += 16) {
                    for (int dx = -r; dx <= r && !found; dx += 16) {
                        for (int dz = -r; dz <= r && !found; dz += 16) {
                            if (std::abs(dx) != r && std::abs(dz) != r) continue;
                            auto b = gen->getBiome(cx+dx, cz+dz);
                            if (static_cast<int>(b) == biome) {
                                std::cout << "  " << biomeNames[biome] << ": /tp "
                                          << (cx+dx) << " 100 " << (cz+dz) << "\n";
                                found = true;
                            }
                        }
                    }
                }
                if (!found) std::cout << "  " << biomeNames[biome] << ": not found within 500 blocks\n";
            }
            std::cout << "=== END ===\n\n";
        }
    }

    // F7: Teleport by typing coords in console (non-blocking — reads last line from stdin if available)
    // For simplicity: just tp to 0,100,0 as a "home" button. Use F6 output to find coords.
    if (input_.isKeyPressed(GLFW_KEY_F7)) {
        player_.position = glm::vec3(0.5f, 100.0f, 0.5f);
        player_.velocity = glm::vec3(0);
        player_.fallStartY = player_.position.y;
        player_.wasFalling = false;
        std::cout << "[TP] Teleported to spawn (0, 100, 0)\n";
    }

    // F8: Show world save info
    if (input_.isKeyPressed(GLFW_KEY_F8)) {
        std::cout << "\n=== WORLD INFO ===\n";
        std::cout << "World: " << saveManager_.getWorldName() << "\n";
        std::cout << "Seed: " << worldSeed_ << "\n";
        std::cout << "Ticks: " << tickClock_.getTotalTicks() << "\n";
        std::cout << "Save dir: " << saveManager_.getWorldDir() << "\n";
        int modifiedChunks = 0;
        for (const auto& [key, chunk] : world_.chunks()) {
            if (chunk.isModified()) ++modifiedChunks;
        }
        std::cout << "Loaded chunks: " << world_.chunks().size()
                  << " (modified: " << modifiedChunks << ")\n";
        std::cout << "==================\n";
    }

    // Mouse look
    if (input_.isCursorLocked()) {
        player_.look(input_.getMouseDeltaX(), input_.getMouseDeltaY());
    }

    // Double-tap W to sprint (MC behavior)
    if (input_.isKeyPressed(GLFW_KEY_W) && input_.isCursorLocked()) {
        double now = glfwGetTime();
        if (now - lastWPressTime_ < 0.3) {
            sprintToggled_ = true;
        }
        lastWPressTime_ = now;
    }
    // Stop sprinting when W is released, or when sneaking, or when not moving forward
    if (!input_.isKeyDown(GLFW_KEY_W) || input_.isKeyDown(GLFW_KEY_LEFT_SHIFT)) {
        sprintToggled_ = false;
    }

    // Hotbar 1-9
    for (int i = 0; i < 9; i++) {
        if (input_.isKeyPressed(GLFW_KEY_1 + i)) {
            if (inventory_.getSelectedSlot() != i) {
                inventory_.setSelectedSlot(i);
                blockInteraction_.reset();
            }
        }
    }

    // Scroll wheel
    double scroll = input_.getScrollDelta();
    if (scroll != 0.0) {
        scrollAccum_ += scroll;
        int steps = 0;
        while (scrollAccum_ >= 1.0) { steps -= 1; scrollAccum_ -= 1.0; }
        while (scrollAccum_ <= -1.0) { steps += 1; scrollAccum_ += 1.0; }
        if (steps != 0) {
            int slot = inventory_.getSelectedSlot() + steps;
            slot %= Inventory::HOTBAR_SIZE;
            if (slot < 0) slot += Inventory::HOTBAR_SIZE;
            if (slot != inventory_.getSelectedSlot()) {
                inventory_.setSelectedSlot(slot);
                blockInteraction_.reset();
            }
        }
    }

    // Left mouse
    leftMouseHeld_ = input_.isCursorLocked() && input_.isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
    if (input_.isCursorLocked() && input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        player_.attackCooldownTicks = player_.attackCooldownMax;
    }

    // Q: drop
    if (input_.isKeyPressed(GLFW_KEY_Q) && input_.isCursorLocked()) {
        ItemStack& held = inventory_.getHeldItem();
        if (!held.isEmpty()) {
            ItemStack toDrop{held.id, 1, held.durability};
            glm::vec3 eye = player_.getEyePosition();
            glm::vec3 fwd = player_.getForward();
            glm::vec3 throwVel = fwd * 5.0f + glm::vec3(0.0f, 3.0f, 0.0f);
            entityManager_.spawnItem(eye + fwd * 0.5f, toDrop, throwVel);
            if (held.count <= 1) held.clear();
            else held.count -= 1;
            VLOG(DebugCat::Input, "Q drop itemId=%u, remaining=%u", toDrop.id, held.count);
        }
    }

    // C: auto-craft
    if (input_.isKeyPressed(GLFW_KEY_C) && input_.isCursorLocked()) {
        std::vector<std::pair<ItemId,int>> bag;
        for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i) {
            const auto& s = inventory_.getSlot(i);
            if (s.isEmpty()) continue;
            bool found = false;
            for (auto& [id, cnt] : bag) {
                if (id == s.id) { cnt += s.count; found = true; break; }
            }
            if (!found) bag.push_back({s.id, s.count});
        }
        const Recipe* r = RecipeRegistry::instance().findShapelessMatch(bag);
        if (r) {
            std::unordered_map<ItemId, int> need;
            for (auto id : r->grid) {
                if (id != Item::None) need[id]++;
            }
            for (auto& [id, cnt] : need) {
                inventory_.consumeItem(id, static_cast<uint16_t>(cnt));
            }
            inventory_.addItem(r->output);
            VLOG(DebugCat::Input, "crafted %s x%u",
                 ItemRegistry::instance().get(r->output.id).displayName.c_str(),
                 r->output.count);
        }
    }

    // Right click
    if (input_.isCursorLocked() && input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
        handleRightClick();
    }

    // Cancel eating on release
    if (player_.isEating && !input_.isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
        player_.isEating = false;
        player_.eatingTicks = 0;
    }
}

void Game::handleRightClick() {
    RayHit hit = raycastWorld(world_, player_.getEyePosition(),
                              player_.getForward(), MAX_REACH);

    // Check interactable block first (data-driven via blockUseHandlers_)
    if (hit.hit) {
        BlockId targetBlock = world_.getBlock(hit.blockX, hit.blockY, hit.blockZ);
        auto it = blockUseHandlers_.find(targetBlock);
        if (it != blockUseHandlers_.end()) {
            it->second(*this, hit.blockX, hit.blockY, hit.blockZ);
            return;
        }
    }

    // Eat or place
    const ItemStack& held = inventory_.getHeldItem();
    if (held.isEmpty()) return;

    const auto& itemProps = ItemRegistry::instance().get(held.id);
    if (itemProps.type == ItemType::Food && player_.hunger < player_.maxHunger) {
        player_.isEating = true;
        player_.eatingTicks = 0;
    } else if (itemProps.type == ItemType::Block && itemProps.blockId > 0 && hit.hit) {
        int px = hit.prevX, py = hit.prevY, pz = hit.prevZ;
        bool cellEmpty = world_.getBlock(px, py, pz) == Block::Air;
        bool intoSelf  = Physics::playerIntersectsBlock(player_, px, py, pz);
        if (cellEmpty && !intoSelf) {
            world_.setBlock(px, py, pz, itemProps.blockId);
            inventory_.consumeHeldItem(1);
        }
    }
}

void Game::handleTickInput() {
    if (activeScreen_) {
        player_.velocity.x = 0.0f;
        player_.velocity.z = 0.0f;
        player_.sprinting = false;
        return;
    }

    glm::vec3 move(0.0f);
    if (input_.isKeyDown(GLFW_KEY_W)) move += player_.getFlatForward();
    if (input_.isKeyDown(GLFW_KEY_S)) move -= player_.getFlatForward();
    if (input_.isKeyDown(GLFW_KEY_A)) move -= player_.getRight();
    if (input_.isKeyDown(GLFW_KEY_D)) move += player_.getRight();

    if (glm::length(move) > 0.01f) move = glm::normalize(move);

    // Sneaking: Shift key — slower, can't fall off edges
    player_.sneaking = input_.isKeyDown(GLFW_KEY_LEFT_SHIFT) && player_.onGround;

    // Sprinting: double-tap W or Ctrl (can't sprint while sneaking or hungry)
    player_.sprinting = !player_.sneaking && player_.hunger > 6 &&
                        (sprintToggled_ || input_.isKeyDown(GLFW_KEY_LEFT_CONTROL));

    float speed;
    if (player_.sneaking) {
        speed = SNEAK_SPEED;
    } else if (player_.sprinting) {
        speed = SPRINT_SPEED;
    } else {
        speed = MOVE_SPEED;
    }

    // Check if in water for swimming
    int footBx = static_cast<int>(std::floor(player_.position.x));
    int footBy = static_cast<int>(std::floor(player_.position.y));
    int footBz = static_cast<int>(std::floor(player_.position.z));
    bool feetWet = BlockRegistry::instance().isLiquid(world_.getBlock(footBx, footBy, footBz));

    if (feetWet) {
        // Water: slower horizontal, space = swim up
        float waterSpeed = MOVE_SPEED * 0.5f;
        player_.velocity.x = move.x * waterSpeed;
        player_.velocity.z = move.z * waterSpeed;

        if (input_.isKeyDown(GLFW_KEY_SPACE)) {
            player_.velocity.y = 4.5f;  // gentle upward swim
        }
    } else {
        player_.velocity.x = move.x * speed;
        player_.velocity.z = move.z * speed;

        if (input_.isKeyDown(GLFW_KEY_SPACE) && player_.onGround) {
            player_.velocity.y = JUMP_FORCE;
            player_.onGround = false;
        }
    }
}

// ============================================================
// Chunk management
// ============================================================

void Game::updateChunks() {
    for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; dx++) {
        for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; dz++) {
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) continue;

            int cx = playerChunkX_ + dx, cz = playerChunkZ_ + dz;
            auto& chunk = world_.getOrCreateChunk(cx, cz);

            if (!chunk.hasData()) {
                // Try loading from disk first (modified chunks saved previously)
                bool loaded = saveManager_.loadChunk(cx, cz, chunk);
                if (loaded) {
                    // Recompute lighting (not persisted)
                    chunk.updateHeightMap();
                    LightEngine::initSkyLight(chunk);
                    LightEngine::initBlockLight(chunk);
                } else {
                    // Generate from seed
                    terrainGen_->generate(chunk);
                }
                world_.markChunkDirty(cx - 1, cz);
                world_.markChunkDirty(cx + 1, cz);
                world_.markChunkDirty(cx, cz - 1);
                world_.markChunkDirty(cx, cz + 1);
            }
        }
    }
}

void Game::buildMeshes() {
    int meshBuilds = 0;
    for (auto& [key, chunk] : world_.chunks()) {
        if (!chunk.isMeshDirty()) continue;

        int dx = chunk.chunkX() - playerChunkX_;
        int dz = chunk.chunkZ() - playerChunkZ_;
        if (dx * dx + dz * dz > (RENDER_DISTANCE + 1) * (RENDER_DISTANCE + 1)) continue;

        int cx = chunk.chunkX();
        int cz = chunk.chunkZ();
        const Chunk* nxp = world_.getChunk(cx + 1, cz);
        const Chunk* nxn = world_.getChunk(cx - 1, cz);
        const Chunk* nzp = world_.getChunk(cx, cz + 1);
        const Chunk* nzn = world_.getChunk(cx, cz - 1);
        if ((!nxp || !nxp->hasData()) || (!nxn || !nxn->hasData()) ||
            (!nzp || !nzp->hasData()) || (!nzn || !nzn->hasData())) {
            continue;
        }

        meshBuilder_.build(world_, chunk);

        if (chunk.hasMesh()) {
            engine_.destroyMesh(chunk.getMesh());
        }
        if (chunk.hasTransparentMesh()) {
            engine_.destroyMesh(chunk.getTransparentMesh());
        }

        if (!meshBuilder_.isEmpty()) {
            chunk.setMesh(engine_.uploadMesh(meshBuilder_.getVertices(), meshBuilder_.getIndices()));
        } else {
            chunk.setMesh(Mesh{});
        }

        if (!meshBuilder_.isTransparentEmpty()) {
            chunk.setTransparentMesh(engine_.uploadMesh(meshBuilder_.getTransparentVertices(),
                                                         meshBuilder_.getTransparentIndices()));
        } else {
            chunk.setTransparentMesh(Mesh{});
        }
        chunk.clearMeshDirty();

        if (++meshBuilds >= 2) break;
    }
}

void Game::unloadDistantChunks() {
    int unloadDist = RENDER_DISTANCE + 3;
    int unloadDistSq = unloadDist * unloadDist;

    chunksToRemove_.clear();
    for (auto& [key, chunk] : world_.chunks()) {
        int dx = chunk.chunkX() - playerChunkX_;
        int dz = chunk.chunkZ() - playerChunkZ_;
        if (dx * dx + dz * dz > unloadDistSq) {
            // Save modified chunks before unloading
            if (chunk.isModified()) {
                saveManager_.saveChunk(chunk);
                chunk.clearModified();
            }
            if (chunk.hasMesh()) engine_.destroyMesh(chunk.getMesh());
            if (chunk.hasTransparentMesh()) engine_.destroyMesh(chunk.getTransparentMesh());
            chunksToRemove_.push_back(key);
        }
    }
    for (auto& k : chunksToRemove_) {
        world_.removeChunk(k.x, k.z);
    }
}

// ============================================================
// Render
// ============================================================

void Game::render(VkCommandBuffer cmd) {
    // === Pass 1: Opaque geometry (pipeline already bound by engine) ===
    for (auto& [key, chunk] : world_.chunks()) {
        if (!chunk.hasMesh()) continue;
        const auto& mesh = chunk.getMesh();
        VkBuffer vb[] = {mesh.vertexBuffer.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
        vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }

    // === Pass 2: Transparent geometry (alpha blend, no depth write) ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_.getTransparentPipeline());
    // Re-bind descriptor set for the transparent pipeline (same layout)
    auto& frame = engine_.getCurrentFrame();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine_.getPipelineLayout(), 0, 1, &frame.descriptorSet, 0, nullptr);

    // Block selection highlight (uses negative light → solid color with alpha)
    if (hasTarget_ && targetHighlight_.indexCount > 0) {
        VkBuffer vb[] = {targetHighlight_.vertexBuffer.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
        vkCmdBindIndexBuffer(cmd, targetHighlight_.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, targetHighlight_.indexCount, 1, 0, 0, 0);
    }

    // Block break overlay (crack texture on all 6 faces)
    if (breakOverlay_.indexCount > 0) {
        VkBuffer vb[] = {breakOverlay_.vertexBuffer.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
        vkCmdBindIndexBuffer(cmd, breakOverlay_.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, breakOverlay_.indexCount, 1, 0, 0, 0);
    }

    // Transparent chunk meshes (water, glass) — rough back-to-front by chunk distance
    {
        glm::vec3 camPos = player_.getEyePosition();
        struct ChunkDist { Chunk* chunk; float distSq; };
        std::vector<ChunkDist> transChunks;
        for (auto& [key, chunk] : world_.chunks()) {
            if (!chunk.hasTransparentMesh()) continue;
            float cx = static_cast<float>(chunk.worldX()) + CHUNK_SIZE * 0.5f;
            float cz = static_cast<float>(chunk.worldZ()) + CHUNK_SIZE * 0.5f;
            float dx = cx - camPos.x;
            float dz = cz - camPos.z;
            transChunks.push_back({&chunk, dx * dx + dz * dz});
        }
        // Sort back-to-front (farthest first)
        std::sort(transChunks.begin(), transChunks.end(),
                  [](const ChunkDist& a, const ChunkDist& b) { return a.distSq > b.distSq; });

        for (auto& cd : transChunks) {
            const auto& mesh = cd.chunk->getTransparentMesh();
            VkBuffer vb[] = {mesh.vertexBuffer.buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
            vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
        }
    }

    // Entities (dropped items) — also through transparent pipeline for bobbing alpha
    entityRenderer_.render(cmd);

    // === UI pass (engine switches to UI pipeline internally) ===
    uiRenderer_.flush(cmd, engine_.getWindowWidth(), engine_.getWindowHeight());
}
