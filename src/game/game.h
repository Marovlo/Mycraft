#pragma once

#include <climits>

#include "engine/vulkan_engine.h"
#include "core/input.h"
#include "core/block.h"
#include "world/world.h"
#include "world/terrain_generator.h"
#include "renderer/mesh_builder.h"
#include "renderer/texture_atlas.h"
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
#include "crafting/smelting_recipe.h"

#include <memory>
#include <functional>

class Game {
public:
    Game();
    ~Game();

    void init();
    void run();

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
    TickClock tickClock_;
    UIRenderer uiRenderer_;
    BlockModelRenderer blockModel_;
    HUD hud_;
    Inventory inventory_;
    BlockInteraction blockInteraction_;
    EntityManager    entityManager_;
    EntityRenderer   entityRenderer_;

    // 多线程区块任务管理器
    ChunkTaskManager chunkTaskMgr_;

    // Save system
    SaveManager      saveManager_;
    int64_t          worldSeed_ = 42;

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

    int playerChunkX_ = 0;
    int playerChunkZ_ = 0;
    glm::vec3 prevPlayerPos_{0.0f, 100.0f, 0.0f};

    Mesh targetHighlight_;
    bool hasTarget_ = false;

    // 缓存上一帧的目标方块坐标，只在目标变化时重建高亮 mesh
    int prevTargetX_ = INT_MIN, prevTargetY_ = INT_MIN, prevTargetZ_ = INT_MIN;

    // Block break overlay (6-face quad mesh with destroy_stage texture)
    Mesh breakOverlay_;
    int prevBreakStage_ = -1;  // 缓存上一帧的破坏阶段，只在阶段变化时重建

    std::vector<ChunkKey> chunksToRemove_;

    // Auto-save: every 6000 ticks (5 minutes at 20 TPS)
    static constexpr uint64_t AUTOSAVE_INTERVAL_TICKS = 6000;
    uint64_t lastAutoSaveTick_ = 0;

    // Incremental chunk saving: save up to N dirty chunks per tick
    static constexpr int INCREMENTAL_SAVE_PER_TICK = 2;

    // FPS 显示（F3 切换）
    bool showFps_ = false;
    int  fps_     = 0;       // 当前显示的 FPS 值
    int  fpsFrameCount_ = 0; // 当前秒内的帧计数
    double fpsTimer_ = 0.0;  // 累计时间（秒）
};
