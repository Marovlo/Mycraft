#pragma once

#include <climits>

#include "engine/vulkan_engine.h"
#include "core/input.h"
#include "core/block.h"
#include "world/world.h"
#include "world/terrain_generator.h"
#include "world/biome_colormap.h"
#include "renderer/mesh_builder.h"
#include "renderer/texture_atlas.h"
#include "renderer/texture_animator.h"
#include "world/block_update_system.h"
#include "renderer/particle_system.h"
#include "renderer/sky_renderer.h"
#include "renderer/ui_renderer.h"
#include "renderer/block_model.h"
#include "renderer/entity_renderer.h"
#include "player/player.h"
#include "player/physics.h"
#include "player/inventory.h"
#include "core/tick_clock.h"
#include "ui/hud.h"
#include "game/block_interaction.h"
#include "entity/entity_manager.h"
#include "crafting/recipe.h"
#include "ui/container_screen.h"
#include "ui/inventory_screen.h"
#include "ui/crafting_screen.h"
#include "ui/furnace_screen.h"
#include "ui/chest_screen.h"
#include "world/save_manager.h"
#include "world/chest_manager.h"
#include "world/furnace_manager.h"
#include "world/chunk_task_manager.h"
#include "world/day_night_cycle.h"
#include "crafting/smelting_recipe.h"
#include "renderer/mob_renderer.h"
#include "renderer/player_renderer.h"
#include "renderer/remote_player_renderer.h"
#include "entity/mob_spawner.h"
#include "ui/game_console.h"
#include "ui/main_menu_screen.h"
#include "audio/sound_engine.h"
#include "audio/block_sound_map.h"
#include "audio/music_manager.h"
#include "renderer/gui_atlas.h"
#include "network/integrated_server.h"
#include "network/client_connection.h"

#include <memory>
#include <functional>

class Game {
public:
    // MC 原版视角模式（F5 切换）
    enum class CameraMode {
        FirstPerson,      // 第一人称（默认）
        ThirdPersonBack,  // 第三人称背面
        ThirdPersonFront  // 第三人称正面（第二人称）
    };

    Game();
    ~Game();

    void init();
    void run();

    // 世界管理：从菜单进入指定世界（单人模式，通过 IntegratedServer）
    void enterWorld(const std::string& worldName, int64_t seed);
    void leaveWorld();  // 保存并返回菜单

    // 多人模式：连接远程服务器
    void connectToServer(const std::string& host, uint16_t port, const std::string& playerName);
    void disconnectFromServer();

    // Open a container screen (called by block interaction callbacks).
    void openScreen(ContainerScreen* screen);
    void closeActiveScreen();
    bool hasActiveScreen() const { return activeScreen_ != nullptr; }

private:
    void update(float dt);
    void gameTick();
    void render(VkCommandBuffer cmd);

    void handleFrameInput();
    void handleTickInput();
    void handleGameplayInput();
    void handleRightClick();
    void releaseBow();  // 松开弓：射出箭矢
    void sendNetworkAction(PlayerActionType action);  // 发送动作到服务器广播
    void updateChunks();
    void buildMeshes();
    void unloadDistantChunks();
    void loadTextureAtlas();

    // 多线程区块系统：主线程轮询工作线程结果
    void pollChunkGenResults();
    void pollMeshResults();
    void submitPendingMeshTasks();

    // 玩家生存系统 tick（game_survival.cpp）
    void tickFallDamage();
    void tickVoidDamage();
    void tickHunger();
    void tickEating();
    void tickBreathing();

    // 方块高亮与破坏覆盖层（game_highlight.cpp）
    void updateBlockHighlight();

    // 调试命令（game_debug.cpp）
    void handleDebugKeys();
    void drawDebugScreen(float screenW, float screenH);

    // 控制台命令注册（game_console_cmds.cpp）
    void registerConsoleCommands();

    // 游戏状态管理
    void updateMenu(float dt);
    void renderMenu(VkCommandBuffer cmd);
    void deleteWorld(const std::string& worldName);

    // 网络同步：处理服务器发来的数据
    void processNetworkSync();
    void applyReceivedChunks();
    void renderRemotePlayers(VkCommandBuffer cmd);

    // Save/Load
    void saveAll();       // Save player + dirty chunks + level data

    // Register block interaction handlers (crafting table, future: furnace, chest, etc.)
    void registerBlockInteractions();

    // Systems
    VulkanEngine engine_;
    InputManager input_;
    World world_;
    Player player_;
    std::unique_ptr<TerrainGenerator> terrainGen_;
    MeshBuilder meshBuilder_;
    BiomeColorMap biomeColorMap_;
    TickClock tickClock_;
    UIRenderer uiRenderer_;
    BlockModelRenderer blockModel_;
    HUD hud_;
    Inventory inventory_;
    BlockInteraction blockInteraction_;
    EntityManager    entityManager_;
    EntityRenderer   entityRenderer_;
    ParticleSystem   particleSystem_;
    MobRenderer      mobRenderer_;
    PlayerRenderer   playerRenderer_;
    RemotePlayerRenderer remotePlayerRenderer_;
    MobSpawner       mobSpawner_;
    DayNightCycle    dayNightCycle_;
    GameConsole      console_;

    // 多线程区块任务管理器
    ChunkTaskManager chunkTaskMgr_;

    // 游戏状态
    GameState        gameState_ = GameState::MainMenu;
    bool             worldLoaded_ = false;  // 世界是否已加载

    // 菜单界面
    MainMenuScreen   mainMenuScreen_;
    WorldSelectScreen worldSelectScreen_;
    CreateWorldScreen createWorldScreen_;
    ServerConnectScreen serverConnectScreen_;

    // 网络系统
    std::unique_ptr<IntegratedServer> integratedServer_;  // 单人模式本地服务器
    std::unique_ptr<ClientConnection> clientConnection_;  // 多人模式远程连接
    bool isMultiplayer_ = false;  // 当前是否为多人模式
    std::string connectStatus_;   // 连接状态文本

    // Save system
    SaveManager      saveManager_;
    int64_t          worldSeed_ = 42;
    std::string      savesBasePath_;  // saves/ 目录路径

    // GUI screens (owned by Game, activated via pointer)
    InventoryScreen  inventoryScreen_;
    CraftingScreen   craftingScreen_;
    FurnaceScreen    furnaceScreen_;
    ChestScreen      chestScreen_;
    ContainerScreen* activeScreen_ = nullptr;  // currently open screen (nullptr = none)

    // Chest storage (per-block-position inventories)
    ChestManager     chestManager_;

    // Furnace storage (per-block-position furnace state)
    FurnaceManager   furnaceManager_;

    // Block interaction callbacks: BlockId → handler function.
    // Called when player right-clicks an interactable block.
    using BlockUseHandler = std::function<void(Game& game, int bx, int by, int bz)>;
    std::unordered_map<BlockId, BlockUseHandler> blockUseHandlers_;

    // Per-frame input snapshot used by tick logic.
    bool leftMouseHeld_ = false;

    double scrollAccum_ = 0.0;

    // Double-tap W sprint detection
    double lastWPressTime_ = 0.0;
    bool   sprintToggled_  = false;

    TextureAtlas textureAtlas_;
    TextureAnimator textureAnimator_;
    GuiAtlas guiAtlas_;
    BlockUpdateSystem blockUpdateSystem_;
    SkyRenderer skyRenderer_;

    int playerChunkX_ = 0;
    int playerChunkZ_ = 0;
    glm::vec3 prevPlayerPos_{0.0f, 100.0f, 0.0f};

    Mesh targetHighlight_;
    bool hasTarget_ = false;

    // 准星是否指向生物（用于显示攻击标识）
    bool targetingMob_ = false;

    // 缓存上一帧的目标方块坐标，只在目标变化时重建高亮 mesh
    int prevTargetX_ = INT_MIN, prevTargetY_ = INT_MIN, prevTargetZ_ = INT_MIN;

    // Block break overlay (6-face quad mesh with destroy_stage texture)
    Mesh breakOverlay_;
    int prevBreakStage_ = -1;  // 缓存上一帧的破坏阶段，只在阶段变化时重建

    std::vector<ChunkKey> chunksToRemove_;

    // 性能优化：透明区块排序缓存（避免每帧分配 vector）
    struct ChunkDist { Chunk* chunk; float distSq; };
    std::vector<ChunkDist> transChunksSorted_;

    // Auto-save: every 6000 ticks (5 minutes at 20 TPS)
    static constexpr uint64_t AUTOSAVE_INTERVAL_TICKS = 6000;
    uint64_t lastAutoSaveTick_ = 0;

    // Incremental chunk saving: save up to N dirty chunks per tick
    static constexpr int INCREMENTAL_SAVE_PER_TICK = 2;

    // 玩家攻击生物（game_survival.cpp）
    void tickPlayerAttack();

    // F3 调试屏幕
    bool showDebug_ = false;
    int  fps_     = 0;       // 当前显示的 FPS 值
    int  fpsFrameCount_ = 0; // 当前秒内的帧计数
    double fpsTimer_ = 0.0;  // 累计时间（秒）

    // ===== 音效系统 =====
    MusicManager musicManager_;

    // 行走音效计时器（MC 原版：每走一定距离播放一次脚步声）
    float stepSoundDistance_ = 0.0f;
    // 挖掘音效计时器（MC 原版：挖掘过程中每 4 tick 播放一次挖掘音效）
    int   digSoundTimer_ = 0;

    // ===== 洞穴环境音效 =====
    // MC 原版：每 tick 1/6000 概率触发，检测玩家附近光照 ≤ 3 的方块
    void tickCaveAmbient();

    // ===== 天气音效 =====
    // MC 原版：下雨时播放雨声循环，雷暴时随机播放雷声
    void tickWeatherAmbient();
    bool isRaining_ = false;
    bool isThundering_ = false;

    // 网络同步计时器：每 N tick 发送一次位置更新
    int positionSendTimer_ = 0;
    static constexpr int POSITION_SEND_INTERVAL = 1;  // 每 tick 发送

    // ===== F5 视角切换系统 =====
    CameraMode cameraMode_ = CameraMode::FirstPerson;
    static constexpr float THIRD_PERSON_DISTANCE = 4.0f;  // 第三人称相机距离（方块）
    void buildLocalPlayerThirdPerson(float partialTick);   // 构建本地玩家第三人称 mesh
};
