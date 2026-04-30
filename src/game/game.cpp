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
#include "entity/mob_entity.h"
#include "world/light_engine.h"
#include <unordered_map>
#include <filesystem>
#include <random>

Game::Game() = default;

Game::~Game() {
    // Shutdown thread pool before cleanup (wait for in-flight tasks)
    chunkTaskMgr_.shutdown();

    // 关闭音效系统（在其他清理之前，避免音效回调访问已释放的资源）
    musicManager_.shutdown();
    getSoundEngine().shutdown();

    // 关闭网络连接
    if (integratedServer_) {
        std::cout << "[DEBUG] ~Game() calling integratedServer_->stop()" << std::endl;
        integratedServer_->stop();
        integratedServer_.reset();
    }
    if (clientConnection_) {
        clientConnection_->disconnect();
        clientConnection_.reset();
    }
    NetworkManager::deinitENet();

    // Save all data before cleanup
    if (worldLoaded_) {
        saveAll();
    }

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
    particleSystem_.destroy();
    mobRenderer_.destroy();
    playerRenderer_.destroy();
    remotePlayerRenderer_.destroy();
    uiRenderer_.destroy();
    guiAtlas_.destroy(engine_);
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

        // 生成方块破坏粒子
        const auto& blockDef = BlockRegistry::instance().get(blockId);
        uint16_t tileIdx = blockDef.textures.top; // 使用顶面纹理作为碎片
        particleSystem_.spawnBlockBreak(glm::vec3(bx, by, bz), tileIdx);

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

    // 设置 saves 基础目录
    savesBasePath_ = "saves";
    std::error_code ec;
    std::filesystem::create_directories(savesBasePath_, ec);

    // 初始化渲染系统（这些与世界无关，始终需要）
    loadTextureAtlas();
    uiRenderer_.init(&engine_);
    uiRenderer_.setAtlas(&textureAtlas_);

    // 构建 GUI 纹理图集（原版 MC GUI 精灵图）
    {
        std::string assetDir = std::string(ASSET_DIR);
        if (guiAtlas_.build(engine_, assetDir)) {
            uiRenderer_.setGuiAtlas(&guiAtlas_);
        } else {
            std::cerr << "Warning: failed to build GUI atlas\n";
        }
    }

    blockModel_.init(engine_, textureAtlas_);
    entityRenderer_.init(&engine_, &textureAtlas_);
    particleSystem_.init(&engine_, &textureAtlas_);
    mobRenderer_.init(&engine_);
    {
        std::string mobTexDir = std::string(ASSET_DIR) + "/textures/mobs";
        if (!mobRenderer_.loadMobTextures(engine_, mobTexDir)) {
            std::cerr << "Warning: failed to load mob textures from " << mobTexDir << "\n";
        }
    }

    // 初始化第一人称 viewmodel 渲染器
    playerRenderer_.init(&engine_, &textureAtlas_);
    {
        std::string skinPath = std::string(ASSET_DIR) + "/minecraft_vanilla/textures/entity/player/wide/steve.png";
        if (!playerRenderer_.loadSkinTexture(engine_, skinPath)) {
            std::cerr << "Warning: failed to load player skin from " << skinPath << "\n";
        }
    }

    // 初始化第三人称远程玩家渲染器
    remotePlayerRenderer_.init(&engine_);
    {
        std::string skinPath = std::string(ASSET_DIR) + "/minecraft_vanilla/textures/entity/player/wide/steve.png";
        if (!remotePlayerRenderer_.loadSkinTexture(engine_, skinPath)) {
            std::cerr << "Warning: failed to load remote player skin from " << skinPath << "\n";
        }
    }

    hud_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    inventoryScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    craftingScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    furnaceScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);
    chestScreen_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);

    registerBlockInteractions();

    // 初始化菜单界面
    mainMenuScreen_.init(&uiRenderer_, &textureAtlas_);
    worldSelectScreen_.init(&uiRenderer_, &textureAtlas_);
    createWorldScreen_.init(&uiRenderer_, &textureAtlas_);
    serverConnectScreen_.init(&uiRenderer_, &textureAtlas_);

    // 初始化游戏内控制台
    console_.init(&uiRenderer_);
    registerConsoleCommands();

    // ===== 初始化音效系统 =====
    {
        std::string soundsPath = std::string(ASSET_DIR) + "/sounds/";
        if (!getSoundEngine().init(soundsPath)) {
            std::cerr << "Warning: failed to init sound engine\n";
        }
        BlockSoundMap::instance().init();

        // 初始化背景音乐管理器（扫描 music/ 目录）
        std::string musicPath = soundsPath + "music/";
        if (getSoundEngine().isInitialized()) {
            musicManager_.init(getSoundEngine().getEngine(), musicPath);
        }
        // 当前无版权音乐文件，MusicManager 会自动检测并跳过播放
    }

    // 初始化 ENet 网络库
    NetworkManager::initENet();

    // 设置引擎回调
    engine_.onUpdate = [this](float dt) { update(dt); };
    engine_.onRender = [this](VkCommandBuffer cmd, uint32_t) { render(cmd); };

    // 启动时进入主菜单，解锁鼠标
    gameState_ = GameState::MainMenu;
    input_.setCursorLocked(false);
}

// ============================================================
// 世界管理：进入/离开世界
// ============================================================

void Game::enterWorld(const std::string& worldName, int64_t seed) {
    std::cout << "[Game] Entering world: " << worldName << " (seed=" << seed << ")\n";

    worldSeed_ = seed;
    isMultiplayer_ = false;

    // === 启动 IntegratedServer（MC 原版架构） ===
    integratedServer_ = std::make_unique<IntegratedServer>();
    std::string worldPath = savesBasePath_ + "/" + worldName;
    if (!integratedServer_->startAndConnect(worldPath, seed, "Player")) {
        std::cerr << "[Game] Failed to start integrated server\n";
        integratedServer_.reset();
        return;
    }

    // 获取客户端连接引用
    auto& conn = integratedServer_->getConnection();

    // 设置方块变更回调：服务器发来的方块变更应用到本地 World
    conn.setOnBlockChange([this](int x, int y, int z, uint8_t blockId) {
        world_.setBlock(x, y, z, blockId);
    });

    // 设置聊天回调
    conn.setOnChat([](const std::string& sender, const std::string& msg) {
        std::cout << "[Chat] <" << sender << "> " << msg << std::endl;
    });

    // 设置断开回调
    conn.setOnDisconnect([this](const std::string& reason) {
        std::cout << "[Game] Disconnected: " << reason << std::endl;
        // 将在下一帧处理返回菜单
    });

    // 设置存档目录
    saveManager_.setWorld(worldName, savesBasePath_);

    // 尝试加载已有存档
    bool hasExistingSave = false;
    {
        int64_t loadedSeed = worldSeed_;
        uint64_t totalTicks = 0;
        float spawnX = 0.5f, spawnY = 100.0f, spawnZ = 0.5f;
        std::string name;
        if (saveManager_.loadLevelData(loadedSeed, totalTicks, spawnX, spawnY, spawnZ, name)) {
            worldSeed_ = loadedSeed;
            player_.spawnPoint = glm::vec3(spawnX, spawnY, spawnZ);
            dayNightCycle_.setTotalTicks(static_cast<uint32_t>(totalTicks));
            hasExistingSave = true;
            std::cout << "[Save] Loaded world \"" << name << "\" (seed=" << loadedSeed
                      << ", ticks=" << totalTicks << ")\n";
        }
    }

    terrainGen_ = std::make_unique<OverworldGenerator>(static_cast<int>(worldSeed_));

    // 初始化多线程区块任务管理器
    chunkTaskMgr_.init(0, terrainGen_.get(), &saveManager_, &textureAtlas_);

    // 初始化方块更新系统
    blockUpdateSystem_.init();

    // 设置方块变更回调
    world_.setBlockChangeCallback([this](int x, int y, int z, BlockId oldId, BlockId newId) {
        blockUpdateSystem_.notifyNeighbors(world_, x, y, z, tickClock_.getTotalTicks());
    });

    // 加载玩家数据
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

    // 新世界：计算出生点并给予初始物品
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

    // 重置行走音效计时器
    stepSoundDistance_ = 0.0f;
    digSoundTimer_ = 0;
    positionSendTimer_ = 0;

    // 切换到游戏状态
    gameState_ = GameState::Playing;
    worldLoaded_ = true;
    input_.setCursorLocked(true);
    input_.enableTextInput(false);

    // 重置 tick 时钟（避免菜单时间累积导致进入世界后爆发大量 tick）
    tickClock_.reset(glfwGetTime());
    lastAutoSaveTick_ = tickClock_.getTotalTicks();

    std::cout << "[Game] World loaded successfully.\n";
}

void Game::leaveWorld() {
    if (!worldLoaded_) return;

    std::cout << "[Game] Leaving world...\n";

    // 关闭所有打开的界面
    if (activeScreen_) {
        closeActiveScreen();
    }

    // 保存所有数据（仅单人模式）
    if (!isMultiplayer_) {
        saveAll();
    }

    // 停止多线程任务
    chunkTaskMgr_.shutdown();

    // 关闭网络连接
    if (integratedServer_) {
        std::cout << "[DEBUG] leaveWorld() integratedServer_->stop()" << std::endl;
        integratedServer_->stop();
        integratedServer_.reset();
    }
    if (clientConnection_) {
        clientConnection_->disconnect();
        clientConnection_.reset();
    }
    isMultiplayer_ = false;

    // 清理世界数据
    for (auto& [key, chunk] : world_.chunks()) {
        if (chunk.hasMesh()) {
            engine_.destroyMesh(chunk.getMesh());
        }
        if (chunk.hasTransparentMesh()) {
            engine_.destroyMesh(chunk.getTransparentMesh());
        }
    }
    world_.clear();

    // 清理高亮和破坏覆盖层 mesh
    if (targetHighlight_.indexCount > 0) {
        engine_.destroyMesh(targetHighlight_);
        targetHighlight_ = {};
    }
    if (breakOverlay_.indexCount > 0) {
        engine_.destroyMesh(breakOverlay_);
        breakOverlay_ = {};
    }

    // 重置实体管理器
    entityManager_.clear();

    // 重置家具管理器
    chestManager_.clear();
    furnaceManager_.clear();

    // 重置玩家状态
    player_ = Player();
    inventory_ = Inventory();

    // 重置其他状态
    terrainGen_.reset();
    hasTarget_ = false;
    targetingMob_ = false;
    prevTargetX_ = INT_MIN;
    prevTargetY_ = INT_MIN;
    prevTargetZ_ = INT_MIN;
    prevBreakStage_ = -1;
    blockInteraction_.reset();
    leftMouseHeld_ = false;
    dayNightCycle_ = DayNightCycle();

    worldLoaded_ = false;

    // 返回世界选择界面
    gameState_ = GameState::WorldSelect;
    input_.setCursorLocked(false);
    worldSelectScreen_.refreshWorldList(savesBasePath_);

    std::cout << "[Game] Returned to world select.\n";
}

// ============================================================
// 多人模式：连接远程服务器
// ============================================================

void Game::connectToServer(const std::string& host, uint16_t port, const std::string& playerName) {
    std::cout << "[Game] Connecting to " << host << ":" << port << " as '" << playerName << "'\n";

    isMultiplayer_ = true;
    connectStatus_ = "Connecting...";

    clientConnection_ = std::make_unique<ClientConnection>();

    // 设置回调
    clientConnection_->setOnBlockChange([this](int x, int y, int z, uint8_t blockId) {
        world_.setBlock(x, y, z, blockId);
    });
    clientConnection_->setOnChat([](const std::string& sender, const std::string& msg) {
        std::cout << "[Chat] <" << sender << "> " << msg << std::endl;
    });
    clientConnection_->setOnDisconnect([this](const std::string& reason) {
        std::cout << "[Game] Disconnected: " << reason << std::endl;
        connectStatus_ = "Disconnected: " + reason;
    });

    if (!clientConnection_->connect(host, port, playerName)) {
        std::cerr << "[Game] Failed to connect to server\n";
        connectStatus_ = "Connection failed";
        clientConnection_.reset();
        isMultiplayer_ = false;
        gameState_ = GameState::ServerConnect;
        return;
    }

    // 连接成功，初始化客户端世界
    worldSeed_ = clientConnection_->getWorldSeed();
    terrainGen_ = std::make_unique<OverworldGenerator>(static_cast<int>(worldSeed_));

    // 初始化多线程区块任务管理器（多人模式不需要 SaveManager）
    chunkTaskMgr_.init(0, terrainGen_.get(), nullptr, &textureAtlas_);

    blockUpdateSystem_.init();

    // 设置方块变更回调
    world_.setBlockChangeCallback([this](int x, int y, int z, BlockId oldId, BlockId newId) {
        blockUpdateSystem_.notifyNeighbors(world_, x, y, z, tickClock_.getTotalTicks());
    });

    // 默认出生点
    player_.position = glm::vec3(0.5f, 80.0f, 0.5f);
    player_.spawnPoint = player_.position;
    prevPlayerPos_ = player_.position;

    stepSoundDistance_ = 0.0f;
    digSoundTimer_ = 0;
    positionSendTimer_ = 0;

    gameState_ = GameState::Playing;
    worldLoaded_ = true;
    input_.setCursorLocked(true);
    input_.enableTextInput(false);

    tickClock_.reset(glfwGetTime());
    lastAutoSaveTick_ = tickClock_.getTotalTicks();

    std::cout << "[Game] Connected to server, entering world.\n";
}

void Game::disconnectFromServer() {
    if (clientConnection_) {
        clientConnection_->disconnect();
        clientConnection_.reset();
    }
    isMultiplayer_ = false;
    leaveWorld();
}

// ============================================================
// 网络同步
// ============================================================

void Game::processNetworkSync() {
    // 更新网络连接
    ClientConnection* conn = nullptr;
    if (integratedServer_) {
        conn = &integratedServer_->getConnection();
    } else if (clientConnection_) {
        conn = clientConnection_.get();
    }

    if (!conn) return;

    conn->update();

    // 检查连接状态
    if (!conn->isConnected()) {
        std::printf("[DEBUG] processNetworkSync: connection lost! server running=%d\n",
                    integratedServer_ ? integratedServer_->isRunning() : -1);
        leaveWorld();
        return;
    }

    // 应用接收到的区块数据
    applyReceivedChunks();

    // 发送玩家位置
    positionSendTimer_++;
    if (positionSendTimer_ >= POSITION_SEND_INTERVAL) {
        positionSendTimer_ = 0;
        conn->sendPosition(player_.position, player_.yaw, player_.pitch, player_.onGround);
    }
}

void Game::applyReceivedChunks() {
    ClientConnection* conn = nullptr;
    if (integratedServer_) {
        conn = &integratedServer_->getConnection();
    } else if (clientConnection_) {
        conn = clientConnection_.get();
    }
    if (!conn) return;

    auto chunks = conn->drainChunkData();
    for (auto& chunkData : chunks) {
        auto& chunk = world_.getOrCreateChunk(chunkData.cx, chunkData.cz);

        // 将接收到的方块数据写入区块
        size_t expectedSize = Chunk::blockCount() * sizeof(BlockId);
        if (chunkData.blocks.size() >= expectedSize) {
            std::memcpy(chunk.blocksData(), chunkData.blocks.data(), expectedSize);
            chunk.markHasData();
            chunk.setState(ChunkState::MeshPending);
            chunk.markMeshDirty();

            // 标记邻居 mesh dirty
            world_.markChunkDirty(chunkData.cx - 1, chunkData.cz);
            world_.markChunkDirty(chunkData.cx + 1, chunkData.cz);
            world_.markChunkDirty(chunkData.cx, chunkData.cz - 1);
            world_.markChunkDirty(chunkData.cx, chunkData.cz + 1);
        }
    }
}

void Game::renderRemotePlayers(VkCommandBuffer cmd) {
    // 远程玩家渲染已移至 render() 中通过 remotePlayerRenderer_ 实现
    (void)cmd;
}

void Game::deleteWorld(const std::string& worldName) {
    std::string worldDir = savesBasePath_ + "/" + worldName;
    std::error_code ec;
    std::filesystem::remove_all(worldDir, ec);
    if (ec) {
        std::cerr << "[Game] Failed to delete world: " << ec.message() << "\n";
    } else {
        std::cout << "[Game] Deleted world: " << worldName << "\n";
    }
    // 刷新世界列表
    worldSelectScreen_.refreshWorldList(savesBasePath_);
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

    // 初始化纹理动画（水、岩浆等）
    std::string vanillaBlockDir = std::string(ASSET_DIR) + "/minecraft_vanilla/textures/block";
    textureAnimator_.init(vanillaBlockDir, textureAtlas_, 16);
}

// ============================================================
// Update / Tick
// ============================================================

void Game::update(float dt) {
    // FPS 计数器：每秒更新一次（所有状态都需要）
    fpsFrameCount_++;
    fpsTimer_ += static_cast<double>(dt);
    if (fpsTimer_ >= 1.0) {
        fps_ = fpsFrameCount_;
        fpsFrameCount_ = 0;
        fpsTimer_ -= 1.0;
    }

    // 菜单状态：只处理菜单逻辑
    if (gameState_ != GameState::Playing) {
        updateMenu(dt);
        input_.update();
        input_.postUpdate();
        return;
    }

    double now = glfwGetTime();
    int ticks = tickClock_.advance(now);

    handleFrameInput();

    // handleFrameInput 可能改变 gameState（比如 ESC 返回菜单）
    if (gameState_ != GameState::Playing) {
        input_.update();
        input_.postUpdate();
        return;
    }

    for (int i = 0; i < ticks; i++) {
        gameTick();
    }

    processNetworkSync();

    // 调试：检测卡住问题
    static int dbgFrame = 0;
    dbgFrame++;
    if (dbgFrame <= 30 || dbgFrame % 60 == 0) {
        std::cout << "[F" << dbgFrame << "] ticks=" << ticks
                  << " gs=" << static_cast<int>(gameState_)
                  << " conn=" << (integratedServer_ ? integratedServer_->getConnection().isConnected() : false)
                  << std::endl;
    }

    // processNetworkSync 可能调用 leaveWorld 改变 gameState
    if (gameState_ != GameState::Playing) {
        input_.update();
        input_.postUpdate();
        return;
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

    // === F5 视角切换：调整相机位置 ===
    glm::vec3 cameraEye = renderEye;
    glm::vec3 cameraTarget = renderEye + player_.getForward();

    if (cameraMode_ == CameraMode::ThirdPersonBack) {
        // 第三人称背面：相机在玩家身后
        glm::vec3 backward = -player_.getForward();
        cameraEye = renderEye + backward * THIRD_PERSON_DISTANCE;
        cameraTarget = renderEye;  // 看向玩家
    } else if (cameraMode_ == CameraMode::ThirdPersonFront) {
        // 第三人称正面（第二人称）：相机在玩家前方，面向玩家
        glm::vec3 forward = player_.getForward();
        cameraEye = renderEye + forward * THIRD_PERSON_DISTANCE;
        cameraTarget = renderEye;  // 看向玩家
    }

    ubo.view  = glm::lookAt(cameraEye, cameraTarget, glm::vec3(0, 1, 0));
    ubo.proj  = player_.getProjectionMatrix(aspect);
    // 昼夜循环天空颜色
    glm::vec3 skyCol = dayNightCycle_.getSkyColor();
    glm::vec3 fogCol = dayNightCycle_.getFogColor();
    ubo.fogColor = glm::vec4(fogCol, 1.0f);
    ubo.viewPos  = glm::vec4(cameraEye, 1.0f);
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

    float sw = static_cast<float>(engine_.getScreenCoordWidth());
    float sh = static_cast<float>(engine_.getScreenCoordHeight());
    int   bbx, bby, bbz;
    float bProg = -1.0f;
    blockInteraction_.getActiveBreak(bbx, bby, bbz, bProg);

    // 检测准星是否指向生物（用于显示攻击标识）
    // 优化：先用距离平方预筛选，避免对远处生物做 AABB 射线检测
    targetingMob_ = false;
    if (!player_.dead && input_.isCursorLocked()) {
        glm::vec3 eye = player_.getEyePosition();
        glm::vec3 fwd = player_.getForward();
        float closestT = MAX_REACH;
        for (const auto& e : entityManager_.entities()) {
            if (!e || !e->alive || e->kind() != EntityKind::Mob) continue;
            auto& mob = static_cast<const MobEntity&>(*e);
            if (mob.isDying) continue;

            // 距离平方预筛选：超过 MAX_REACH + 生物半径 的生物不可能被命中
            glm::vec3 toMob = mob.position - eye;
            float distSq = glm::dot(toMob, toMob);
            float maxCheckDist = MAX_REACH + mob.mobWidth;
            if (distSq > maxCheckDist * maxCheckDist) continue;

            glm::vec3 minB = mob.getHitboxMin();
            glm::vec3 maxB = mob.getHitboxMax();
            float tmin = 0.0f, tmax = MAX_REACH;
            bool hit = true;
            for (int i = 0; i < 3; i++) {
                if (std::abs(fwd[i]) < 1e-6f) {
                    if (eye[i] < minB[i] || eye[i] > maxB[i]) { hit = false; break; }
                } else {
                    float invD = 1.0f / fwd[i];
                    float t1 = (minB[i] - eye[i]) * invD;
                    float t2 = (maxB[i] - eye[i]) * invD;
                    if (t1 > t2) std::swap(t1, t2);
                    tmin = std::max(tmin, t1);
                    tmax = std::min(tmax, t2);
                    if (tmin > tmax) { hit = false; break; }
                }
            }
            if (hit && tmin < closestT) {
                targetingMob_ = true;
                closestT = tmin;
                // 找到最近的即可，不需要继续遍历所有生物
                // （但为了精确性仍然继续检查更近的）
            }
        }
    }

    hud_.draw(sw, sh, inventory_, bProg, tickClock_.getTotalTicks(),
              player_.hp, player_.maxHp, player_.hunger, player_.maxHunger,
              player_.dead, player_.isEating, player_.air, player_.maxAir,
              player_.hurtTicks, targetingMob_,
              player_.xpLevel, player_.xpProgress);

    // F3 调试屏幕
    if (showDebug_) {
        drawDebugScreen(sw, sh);
    }

    if (activeScreen_) {
        activeScreen_->draw(sw, sh, inventory_);
    }

    // 控制台渲染（在所有 UI 之上）
    if (console_.isOpen()) {
        console_.draw(sw, sh);
    }

    entityRenderer_.buildFrame(entityManager_, partial);
    mobRenderer_.buildFrame(entityManager_, partial, &dayNightCycle_);
    playerRenderer_.buildFrame(player_, inventory_, partial, &dayNightCycle_);

    // 构建远程玩家第三人称模型 mesh + 本地玩家第三人称模型（F5 视角）
    {
        ClientConnection* conn = nullptr;
        if (integratedServer_) {
            conn = &integratedServer_->getConnection();
        } else if (clientConnection_) {
            conn = clientConnection_.get();
        }

        bool hasRemotePlayers = conn && !conn->getRemotePlayers().empty();
        bool needLocalThirdPerson = (cameraMode_ != CameraMode::FirstPerson);

        if (hasRemotePlayers || needLocalThirdPerson) {
            float skyLight = dayNightCycle_.getSkyLightFactor();

            // 先构建远程玩家
            if (hasRemotePlayers) {
                remotePlayerRenderer_.buildFrame(
                    conn->getRemotePlayers(), player_.position, partial, &dayNightCycle_);
            } else {
                // 没有远程玩家时也需要初始化帧（清空上一帧数据）
                std::unordered_map<uint32_t, RemotePlayer> empty;
                remotePlayerRenderer_.buildFrame(empty, player_.position, partial, &dayNightCycle_);
            }

            // 第三人称模式：追加本地玩家模型
            if (needLocalThirdPerson && remotePlayerRenderer_.hasSkinTexture()) {
                remotePlayerRenderer_.appendLocalPlayer(
                    player_.position, prevPlayerPos_,
                    player_.yaw, player_.pitch, player_.sneaking,
                    player_.swinging, player_.swingTicks,
                    player_.isChargingBow, player_.bowChargeTicks,
                    player_.isEating, player_.eatingTicks,
                    partial, skyLight);
            }
        }
    }

    // 粒子系统：更新物理 + 构建渲染 mesh
    {
        float dt = static_cast<float>(TickClock::TICK_DURATION);
        particleSystem_.update(dt);
        glm::vec3 camPos = player_.getEyePosition();
        glm::vec3 camFront = player_.getForward();
        glm::vec3 camRight = glm::normalize(glm::cross(camFront, glm::vec3(0, 1, 0)));
        glm::vec3 camUp = glm::normalize(glm::cross(camRight, camFront));
        particleSystem_.buildFrame(camPos, camRight, camUp);
    }
    engine_.updateUniformBuffer(ubo);

    // 方块选择高亮 + 破坏裂纹覆盖层（game_highlight.cpp）
    updateBlockHighlight();

    // ===== 音效系统每帧更新 =====
    {
        // 更新听者位置（玩家摄像机）
        glm::vec3 listenerPos = player_.getEyePosition();
        glm::vec3 listenerFwd = player_.getForward();
        getSoundEngine().setListenerPosition(listenerPos, listenerFwd, glm::vec3(0, 1, 0));

        // 回收已播放完毕的音效实例
        getSoundEngine().update();

        // 背景音乐更新
        musicManager_.update(dt);
    }
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

    // 第一人称手臂挥动动画 tick
    player_.tickSwing();
    // 弓蓄力 tick
    player_.tickBowCharge();

    // 昼夜循环 — 使用 tick clock 的总 tick 数驱动
    dayNightCycle_.setTime(static_cast<uint32_t>(tickClock_.getTotalTicks() % 24000));

    // 让生物系统能访问昼夜循环（用于阳光燃烧判定等）
    MobEntity::sDayNight = &dayNightCycle_;

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

    // 挖掘时持续触发挥动动画 + 挖掘音效（MC 原版行为）
    {
        int bx, by, bz;
        float prog;
        if (blockInteraction_.getActiveBreak(bx, by, bz, prog)) {
            player_.startSwing();

            // MC 原版：挖掘过程中每 4 tick 播放一次挖掘音效
            digSoundTimer_++;
            if (digSoundTimer_ >= 4) {
                digSoundTimer_ = 0;
                BlockId bid = world_.getBlock(bx, by, bz);
                SoundMaterial mat = BlockSoundMap::instance().getMaterial(bid);
                glm::vec3 blockCenter(bx + 0.5f, by + 0.5f, bz + 0.5f);
                getSoundEngine().play(SoundEngine::getDigEvent(mat), blockCenter, 0.25f, 0.5f);
            }
        } else {
            digSoundTimer_ = 0;
        }
    }

    // ===== 行走音效 =====
    // MC 原版：玩家在地面移动时，每走一定距离播放脚步声
    if (player_.onGround && !player_.dead) {
        glm::vec3 delta = player_.position - prevPlayerPos_;
        delta.y = 0.0f; // 只计算水平移动
        float dist = glm::length(delta);
        if (dist > 0.001f) {
            stepSoundDistance_ += dist;
            // MC 原版：行走约 0.6 格播放一次，冲刺约 0.35 格
            float threshold = player_.sprinting ? 0.35f : 0.6f;
            if (stepSoundDistance_ >= threshold) {
                stepSoundDistance_ = 0.0f;
                // 获取脚下方块的音效材质
                int footX = static_cast<int>(std::floor(player_.position.x));
                int footY = static_cast<int>(std::floor(player_.position.y)) - 1;
                int footZ = static_cast<int>(std::floor(player_.position.z));
                BlockId footBlock = world_.getBlock(footX, footY, footZ);
                if (footBlock != Block::Air) {
                    SoundMaterial mat = BlockSoundMap::instance().getMaterial(footBlock);
                    getSoundEngine().playStep(mat, player_.position);
                }
            }
        }
    } else {
        stepSoundDistance_ = 0.0f;
    }
    entityManager_.tick(world_, player_, inventory_);

    // 生物生成与消失
    mobSpawner_.tick(world_, player_, entityManager_, dayNightCycle_);

    // 玩家攻击生物
    tickPlayerAttack();

    // Tick all furnaces (smelting progress, fuel consumption)
    furnaceManager_.tick();

    // 方块更新系统（沙子下落、水流动等计划刻）
    blockUpdateSystem_.tick(world_, entityManager_, player_, tickClock_.getTotalTicks());

    // 纹理动画（水、岩浆等帧动画）
    static int texAnimDbg = 0;
    texAnimDbg++;
    if (texAnimDbg <= 10) {
        std::cout << "[TICK" << texAnimDbg << "] texAnim begin..." << std::flush;
    }
    textureAnimator_.tick(engine_, textureAtlas_);
    if (texAnimDbg <= 10) {
        std::cout << "done" << std::endl;
    }

    // ===== 环境音效 =====
    tickCaveAmbient();
    tickWeatherAmbient();

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
    // 优化：每 10 tick 检查一次（降低全量遍历频率，IO 操作本身就不需要每 tick）
    if (tickClock_.getTotalTicks() % 10 == 0) {
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
    // 控制台打开时，优先处理控制台输入
    if (console_.isOpen()) {
        if (console_.handleInput(input_)) {
            // 控制台关闭后恢复游戏状态
            if (!console_.isOpen()) {
                input_.enableTextInput(false);
                input_.setCursorLocked(true);
            }
            return;
        }
    }

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
        float sw = static_cast<float>(engine_.getScreenCoordWidth());
        float sh = static_cast<float>(engine_.getScreenCoordHeight());
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
    // 返回主菜单（保存并离开世界）
            if (isMultiplayer_) {
                disconnectFromServer();
            } else {
                leaveWorld();
            }
            return;
        }
    }

    // Click to re-lock
    if (!input_.isCursorLocked() && input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        input_.setCursorLocked(true);
    }

    // 调试快捷键（game_debug.cpp）
    handleDebugKeys();

    // / 键：打开控制台（仅在游戏中，光标锁定时）
    if (input_.isKeyPressed(GLFW_KEY_SLASH) && input_.isCursorLocked()) {
        console_.open();
        input_.enableTextInput(true);
        input_.setCursorLocked(false);
        return;
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

    // MC 原版行为：左键点击时无论是否命中都触发挥动动画（空挥也有动画）
    if (input_.isCursorLocked() && input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        player_.startSwing();
        sendNetworkAction(PlayerActionType::SwingArm);
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

    // 弓蓄力：松开右键时射箭
    if (player_.isChargingBow && !input_.isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
        releaseBow();
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

    // Eat, use bow, or place
    const ItemStack& held = inventory_.getHeldItem();
    if (held.isEmpty()) return;

    const auto& itemProps = ItemRegistry::instance().get(held.id);

    // MC 原版：右键持弓开始蓄力（需要背包中有箭矢）
    if (held.id == Item::Bow && !player_.isChargingBow) {
        // 检查背包中是否有箭矢
        bool hasArrow = false;
        for (int i = 0; i < Inventory::TOTAL_SLOTS; ++i) {
            if (inventory_.getSlot(i).id == Item::Arrow && inventory_.getSlot(i).count > 0) {
                hasArrow = true;
                break;
            }
        }
        if (hasArrow) {
            player_.isChargingBow = true;
            player_.bowChargeTicks = 0;
            sendNetworkAction(PlayerActionType::BowDraw);
        }
        return;
    }

    if (itemProps.type == ItemType::Food && player_.hunger < player_.maxHunger) {
        player_.isEating = true;
        player_.eatingTicks = 0;
        sendNetworkAction(PlayerActionType::EatStart);
    } else if (itemProps.type == ItemType::Block && itemProps.blockId > 0 && hit.hit) {
        int px = hit.prevX, py = hit.prevY, pz = hit.prevZ;
        bool cellEmpty = world_.getBlock(px, py, pz) == Block::Air;
        bool intoSelf  = Physics::playerIntersectsBlock(player_, px, py, pz);
        if (cellEmpty && !intoSelf) {
            world_.setBlock(px, py, pz, itemProps.blockId);
            inventory_.consumeHeldItem(1);
            player_.startSwing();  // 放置方块时触发挥动动画

            // 播放方块放置音效
            SoundMaterial mat = BlockSoundMap::instance().getMaterial(itemProps.blockId);
            glm::vec3 blockCenter(px + 0.5f, py + 0.5f, pz + 0.5f);
            getSoundEngine().playBlockPlace(mat, blockCenter);
        }
    }
}

void Game::releaseBow() {
    // MC 原版：松开右键时射出箭矢
    player_.isChargingBow = false;
    sendNetworkAction(PlayerActionType::BowRelease);

    if (!player_.canReleaseBow()) {
        // 蓄力不足 3 tick，不射箭
        player_.bowChargeTicks = 0;
        return;
    }

    // 计算箭矢参数（MC 原版公式）
    float arrowSpeed = player_.getBowArrowSpeed();
    int arrowDamage = player_.getBowArrowDamage();
    float chargeRatio = player_.getBowChargeRatio();

    // 消耗一支箭矢
    inventory_.consumeItem(Item::Arrow, 1);

    // 从玩家眼睛位置沿视线方向射出
    glm::vec3 eyePos = player_.getEyePosition();
    glm::vec3 dir = player_.getForward();

    // MC 原版：箭矢有轻微随机偏移（非满蓄力时偏移更大）
    // 满蓄力时精准无偏移
    if (chargeRatio < 1.0f) {
        // 非满蓄力：添加少量随机散布
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> spread(-0.0075f, 0.0075f);
        float spreadFactor = (1.0f - chargeRatio) * 2.0f;
        dir.x += spread(rng) * spreadFactor;
        dir.y += spread(rng) * spreadFactor;
        dir.z += spread(rng) * spreadFactor;
        dir = glm::normalize(dir);
    }

    // 生成箭矢实体
    entityManager_.spawnArrow(eyePos, dir, arrowSpeed, arrowDamage, true);

    // MC 原版：满蓄力箭矢为暴击箭（粒子效果 + 额外伤害在 ArrowEntity 中处理）
    // 这里通过 speed >= 3.0 来标识暴击

    // 消耗弓耐久
    inventory_.getHeldItem().useDurability(1, 384);
    if (inventory_.getHeldItem().durability == 0 && inventory_.getHeldItem().id == Item::Bow) {
        // 弓损坏
        inventory_.consumeHeldItem(1);
        getSoundEngine().play(SoundEventId::GlassBreak, player_.position, 0.8f);
    }

    player_.bowChargeTicks = 0;
}

void Game::sendNetworkAction(PlayerActionType action) {
    ClientConnection* conn = nullptr;
    if (integratedServer_) {
        conn = &integratedServer_->getConnection();
    } else if (clientConnection_) {
        conn = clientConnection_.get();
    }
    if (conn && conn->isConnected()) {
        conn->sendPlayerAction(action);
    }
}

void Game::handleTickInput() {
    if (activeScreen_) {
        player_.velocity.x = 0.0f;
        player_.velocity.z = 0.0f;
        player_.sprinting = false;
        return;
    }

    // 控制台打开时也阻止移动
    if (console_.isOpen()) {
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
    // 菜单状态：只渲染菜单 UI
    if (gameState_ != GameState::Playing) {
        renderMenu(cmd);
        return;
    }

    // === Pass 0: Sky (fullscreen triangle, no depth write) ===
    skyRenderer_.render(cmd, engine_, dayNightCycle_);

    // === Pass 1: Opaque geometry (pipeline already bound by engine) ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_.getPipeline());
    auto& frame0 = engine_.getCurrentFrame();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine_.getPipelineLayout(), 0, 1, &frame0.descriptorSet, 0, nullptr);

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
        transChunksSorted_.clear();
        for (auto& [key, chunk] : world_.chunks()) {
            if (!chunk.hasTransparentMesh()) continue;
            float cx = static_cast<float>(chunk.worldX()) + CHUNK_SIZE * 0.5f;
            float cz = static_cast<float>(chunk.worldZ()) + CHUNK_SIZE * 0.5f;
            float dx = cx - camPos.x;
            float dz = cz - camPos.z;
            transChunksSorted_.push_back({&chunk, dx * dx + dz * dz});
        }
        // Sort back-to-front (farthest first)
        std::sort(transChunksSorted_.begin(), transChunksSorted_.end(),
                  [](const ChunkDist& a, const ChunkDist& b) { return a.distSq > b.distSq; });

        for (auto& cd : transChunksSorted_) {
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

    // 粒子系统渲染（方块碎片、爆炸、火焰等）
    particleSystem_.render(cmd);

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

    // 远程玩家第三人称模型渲染
    if (remotePlayerRenderer_.hasContent()) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_.getPipeline());
        remotePlayerRenderer_.render(cmd, engine_.getPipelineLayout());
        // 恢复方块纹理的 descriptor set
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            engine_.getPipelineLayout(), 0, 1, &frame.descriptorSet, 0, nullptr);
    }

    // 第一人称 viewmodel（手臂 + 手持物品）— 仅在第一人称模式下渲染
    // 使用 viewmodel 专用管线（depthCompareOp = ALWAYS），确保手臂永远不被世界遮挡
    if (cameraMode_ == CameraMode::FirstPerson && playerRenderer_.hasContent()) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_.getViewmodelPipeline());
        playerRenderer_.render(cmd, engine_.getPipelineLayout(), frame.descriptorSet);
        // 恢复方块纹理的 descriptor set
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            engine_.getPipelineLayout(), 0, 1, &frame.descriptorSet, 0, nullptr);
    }

    // === UI pass (engine switches to UI pipeline internally) ===
    uiRenderer_.flush(cmd, engine_.getScreenCoordWidth(), engine_.getScreenCoordHeight());
}

// ============================================================
// 菜单状态更新与渲染
// ============================================================

void Game::updateMenu(float dt) {
    float sw = static_cast<float>(engine_.getScreenCoordWidth());
    float sh = static_cast<float>(engine_.getScreenCoordHeight());

    switch (gameState_) {
        case GameState::MainMenu: {
            auto action = mainMenuScreen_.update(input_, sw, sh);
            switch (action) {
                case MainMenuScreen::Action::SinglePlayer:
                    gameState_ = GameState::WorldSelect;
                    worldSelectScreen_.refreshWorldList(savesBasePath_);
                    break;
                case MainMenuScreen::Action::Multiplayer:
                    gameState_ = GameState::ServerConnect;
                    serverConnectScreen_.open();
                    input_.enableTextInput(true);
                    break;
                case MainMenuScreen::Action::Quit:
                    glfwSetWindowShouldClose(engine_.getWindow(), GLFW_TRUE);
                    break;
                default:
                    break;
            }
            break;
        }

        case GameState::WorldSelect: {
            auto result = worldSelectScreen_.update(input_, sw, sh);
            switch (result.action) {
                case WorldSelectScreen::Action::PlayWorld: {
                    auto& worlds = worldSelectScreen_.getWorlds();
                    if (result.worldIndex >= 0 && result.worldIndex < static_cast<int>(worlds.size())) {
                        auto& world = worlds[result.worldIndex];
                        enterWorld(world.dirName, world.seed);
                    }
                    break;
                }
                case WorldSelectScreen::Action::CreateNew:
                    gameState_ = GameState::CreateWorld;
                    createWorldScreen_.open();
                    input_.enableTextInput(true);
                    break;
                case WorldSelectScreen::Action::DeleteWorld: {
                    auto& worlds = worldSelectScreen_.getWorlds();
                    if (result.worldIndex >= 0 && result.worldIndex < static_cast<int>(worlds.size())) {
                        deleteWorld(worlds[result.worldIndex].dirName);
                    }
                    break;
                }
                case WorldSelectScreen::Action::Back:
                    gameState_ = GameState::MainMenu;
                    break;
                default:
                    break;
            }
            break;
        }

        case GameState::CreateWorld: {
            auto result = createWorldScreen_.update(input_, sw, sh);
            switch (result.action) {
                case CreateWorldScreen::Action::Create:
                    input_.enableTextInput(false);
                    enterWorld(result.worldName, result.seed);
                    break;
                case CreateWorldScreen::Action::Cancel:
                    input_.enableTextInput(false);
                    gameState_ = GameState::WorldSelect;
                    break;
                default:
                    break;
            }
            break;
        }

        case GameState::ServerConnect: {
            auto result = serverConnectScreen_.update(input_, sw, sh);
            switch (result.action) {
                case ServerConnectScreen::Action::Connect:
                    input_.enableTextInput(false);
                    connectToServer(result.host, result.port, result.playerName);
                    break;
                case ServerConnectScreen::Action::Cancel:
                    input_.enableTextInput(false);
                    gameState_ = GameState::MainMenu;
                    break;
                default:
                    break;
            }
            break;
        }

        case GameState::Connecting: {
            // 连接中状态，等待连接完成
            // 当前 connectToServer 是同步的，所以这个状态不会被触发
            // 后续可以改为异步连接
            break;
        }

        default:
            break;
    }
}

void Game::renderMenu(VkCommandBuffer cmd) {
    float sw = static_cast<float>(engine_.getScreenCoordWidth());
    float sh = static_cast<float>(engine_.getScreenCoordHeight());

    // 设置深色背景
    engine_.setClearColor(0.1f, 0.1f, 0.15f);

    switch (gameState_) {
        case GameState::MainMenu:
            mainMenuScreen_.draw(sw, sh);
            break;
        case GameState::WorldSelect:
            worldSelectScreen_.draw(sw, sh);
            break;
        case GameState::CreateWorld:
            createWorldScreen_.draw(sw, sh);
            break;
        case GameState::ServerConnect:
            serverConnectScreen_.draw(sw, sh);
            break;
        case GameState::Connecting:
            serverConnectScreen_.drawConnecting(sw, sh, connectStatus_);
            break;
        default:
            break;
    }

    // 提交 UI 渲染
    uiRenderer_.flush(cmd, engine_.getScreenCoordWidth(), engine_.getScreenCoordHeight());
}
