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
    // Shutdown thread pool before cleanup (wait for in-flight tasks)
    chunkTaskMgr_.shutdown();

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
    mobRenderer_.destroy();
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
    MobRegistry::instance().registerDefaults();

    if (!engine_.init(1280, 720, "Mycraft")) {
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
            dayNightCycle_.setTotalTicks(static_cast<uint32_t>(totalTicks));
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
    mobRenderer_.init(&engine_);
    {
        std::string mobTexDir = std::string(ASSET_DIR) + "/textures/mobs";
        if (!mobRenderer_.loadMobTextures(engine_, mobTexDir)) {
            std::cerr << "Warning: failed to load mob textures from " << mobTexDir << "\n";
        }
    }

    hud_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    inventoryScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    craftingScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    furnaceScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    chestScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);

    registerBlockInteractions();

    // 初始化多线程区块任务管理器
    // numThreads=0 表示自动检测（物理核心数-1，至少2）
    chunkTaskMgr_.init(0, terrainGen_.get(), &saveManager_, &textureAtlas_);

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

    // 每帧轮询异步区块生成和 mesh 构建结果（不仅仅在 tick 时）
    // 这确保工作线程完成的任务能尽快被主线程处理
    pollChunkGenResults();
    submitPendingMeshTasks();
    pollMeshResults();

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
    // 昼夜循环天空颜色
    glm::vec3 skyCol = dayNightCycle_.getSkyColor();
    glm::vec3 fogCol = dayNightCycle_.getFogColor();
    ubo.fogColor = glm::vec4(fogCol, 1.0f);
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
        engine_.setClearColor(skyCol.r, skyCol.g, skyCol.b);
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
    mobRenderer_.buildFrame(entityManager_, partial, &dayNightCycle_);
    engine_.updateUniformBuffer(ubo);

    // 方块选择高亮 + 破坏裂纹覆盖层（game_highlight.cpp）
    updateBlockHighlight();
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
    if (player_.invulnerableTicks > 0) --player_.invulnerableTicks;

    // 昼夜循环 — 使用 tick clock 的总 tick 数驱动
    dayNightCycle_.setTime(static_cast<uint32_t>(tickClock_.getTotalTicks() % 24000));

    // 玩家生存系统（game_survival.cpp）
    tickFallDamage();
    tickVoidDamage();
    tickHunger();
    tickEating();
    tickBreathing();

    playerChunkX_ = blockToChunk(static_cast<int>(std::floor(player_.position.x)));
    playerChunkZ_ = blockToChunk(static_cast<int>(std::floor(player_.position.z)));

    updateChunks();
    unloadDistantChunks();

    blockInteraction_.tick(world_, player_, inventory_, entityManager_,
                            leftMouseHeld_, MAX_REACH);
    entityManager_.tick(world_, player_, inventory_);

    // 生物生成与消失
    mobSpawner_.tick(world_, player_, entityManager_, dayNightCycle_);

    // 玩家攻击生物
    tickPlayerAttack();

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

    // 调试快捷键（game_debug.cpp）
    handleDebugKeys();

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
    // Phase 1: 发现需要加载的区块，提交到工作线程
    for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; dx++) {
        for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; dz++) {
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) continue;

            int cx = playerChunkX_ + dx, cz = playerChunkZ_ + dz;
            auto& chunk = world_.getOrCreateChunk(cx, cz);

            // 只有 Empty 状态的区块才需要提交生成任务
            if (chunk.state() == ChunkState::Empty && !chunk.hasData()) {
                chunkTaskMgr_.submitGenTask(chunk);
            }
        }
    }

    // Phase 2: 轮询生成完成的结果
    pollChunkGenResults();

    // Phase 3: 提交需要 mesh 构建的区块
    submitPendingMeshTasks();

    // Phase 4: 轮询 mesh 构建完成的结果并上传 GPU
    pollMeshResults();
}

void Game::pollChunkGenResults() {
    std::vector<ChunkTaskManager::GenResult> results;
    chunkTaskMgr_.pollGenResults(results, 16);

    for (auto& r : results) {
        Chunk* chunk = world_.getChunk(r.cx, r.cz);
        if (!chunk) continue;

        // 数据已由工作线程直接写入 chunk 对象
        // 标记邻居 mesh dirty（边界面可能变化）
        chunk->setState(ChunkState::MeshPending);
        world_.markChunkDirty(r.cx - 1, r.cz);
        world_.markChunkDirty(r.cx + 1, r.cz);
        world_.markChunkDirty(r.cx, r.cz - 1);
        world_.markChunkDirty(r.cx, r.cz + 1);

        // 新生成的区块：放置初始被动生物
        if (!chunk->isModified()) {
            mobSpawner_.spawnInitialMobs(world_, entityManager_, r.cx, r.cz);
        }

        // 从 inflight 集合中移除（生成阶段完成）
        // 注意：mesh 构建阶段会重新加入 inflight
    }
}

void Game::submitPendingMeshTasks() {
    int submitted = 0;
    constexpr int MAX_MESH_SUBMITS_PER_TICK = 8;

    for (auto& [key, chunk] : world_.chunks()) {
        if (submitted >= MAX_MESH_SUBMITS_PER_TICK) break;
        if (!chunk.isMeshDirty()) continue;

        // 只处理已有数据且不在处理中的区块
        if (!chunk.hasData()) continue;
        if (chunk.state() == ChunkState::MeshBuilding ||
            chunk.state() == ChunkState::Pending ||
            chunk.state() == ChunkState::Generating ||
            chunk.state() == ChunkState::DataReady) continue;

        int dx = chunk.chunkX() - playerChunkX_;
        int dz = chunk.chunkZ() - playerChunkZ_;
        if (dx * dx + dz * dz > (RENDER_DISTANCE + 1) * (RENDER_DISTANCE + 1)) continue;

        // 检查4个邻居是否都有数据
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

        chunkTaskMgr_.submitMeshTask(chunk, world_);
        chunk.clearMeshDirty();
        ++submitted;
    }
}

void Game::pollMeshResults() {
    std::vector<ChunkTaskManager::MeshResult> results;
    chunkTaskMgr_.pollMeshResults(results, 8);

    for (auto& r : results) {
        Chunk* chunk = world_.getChunk(r.cx, r.cz);
        if (!chunk) continue;

        // 销毁旧 mesh
        if (chunk->hasMesh()) {
            engine_.destroyMesh(chunk->getMesh());
        }
        if (chunk->hasTransparentMesh()) {
            engine_.destroyMesh(chunk->getTransparentMesh());
        }

        // 上传新 mesh 到 GPU（必须在主线程）
        if (!r.opaqueIndices.empty()) {
            chunk->setMesh(engine_.uploadMesh(r.opaqueVerts, r.opaqueIndices));
        } else {
            chunk->setMesh(Mesh{});
        }

        if (!r.transIndices.empty()) {
            chunk->setTransparentMesh(engine_.uploadMesh(r.transVerts, r.transIndices));
        } else {
            chunk->setTransparentMesh(Mesh{});
        }

        chunk->setState(ChunkState::Ready);
    }
}

void Game::buildMeshes() {
    // 多线程模式下，mesh 构建由 ChunkTaskManager 管理。
    // 这里只处理需要同步构建的紧急情况（如方块修改后的即时更新）。
    // 大部分 mesh 构建通过 submitPendingMeshTasks() + pollMeshResults() 异步完成。

    // 同步构建：处理因方块修改（setBlock）触发的 meshDirty 区块
    // 这些区块需要立即更新以保证视觉一致性
    int syncBuilds = 0;
    constexpr int MAX_SYNC_BUILDS = 2;  // 每帧最多同步构建2个（保持帧率）

    for (auto& [key, chunk] : world_.chunks()) {
        if (syncBuilds >= MAX_SYNC_BUILDS) break;
        if (!chunk.isMeshDirty()) continue;
        if (!chunk.hasData()) continue;

        // 只同步构建 Ready 状态的区块（已经有 mesh，因方块修改需要更新）
        // 其他状态的区块由异步管线处理
        if (chunk.state() != ChunkState::Ready) continue;

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

        // 方块修改后的即时 mesh 更新仍然同步执行
        // （保证玩家放置/破坏方块时立即看到效果）
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

        ++syncBuilds;
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
            // 不卸载正在工作线程中处理的区块
            ChunkState st = chunk.state();
            if (st == ChunkState::Pending || st == ChunkState::Generating ||
                st == ChunkState::MeshBuilding) {
                continue;
            }

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

    // Mob entities — 使用独立的 descriptor set 渲染，不影响方块纹理
    if (mobRenderer_.hasMobAtlas() && mobRenderer_.hasContent()) {
        // 切换到不透明管线渲染生物
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_.getPipeline());
        // MobRenderer::render 内部会绑定 mob 专用的 descriptor set
        mobRenderer_.render(cmd, engine_.getPipelineLayout());
        // 恢复方块纹理的 descriptor set
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            engine_.getPipelineLayout(), 0, 1, &frame.descriptorSet, 0, nullptr);
    }

    // === UI pass (engine switches to UI pipeline internally) ===
    uiRenderer_.flush(cmd, engine_.getWindowWidth(), engine_.getWindowHeight());
}
