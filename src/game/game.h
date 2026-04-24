#pragma once

#include "engine/vulkan_engine.h"
#include "core/input.h"
#include "core/block.h"
#include "world/world.h"
#include "world/terrain_generator.h"
#include "renderer/mesh_builder.h"
#include "renderer/texture_atlas.h"
#include "renderer/ui_renderer.h"
#include "renderer/block_model.h"
#include "player/player.h"
#include "player/physics.h"
#include "player/inventory.h"
#include "core/tick_clock.h"
#include "ui/hud.h"

#include <memory>

class Game {
public:
    Game();
    ~Game();

    void init();
    void run();

private:
    void update(float dt);
    void gameTick();
    void render(VkCommandBuffer cmd);

    void handleFrameInput();
    void handleTickInput();
    void updateChunks();
    void buildMeshes();
    void unloadDistantChunks();
    void loadTextureAtlas();

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

    // Texture atlas
    TextureAtlas textureAtlas_;

    // Cached per-frame
    int playerChunkX_ = 0;
    int playerChunkZ_ = 0;
    glm::vec3 prevPlayerPos_{0.0f, 100.0f, 0.0f};

    // Target block highlight
    Mesh targetHighlight_;
    bool hasTarget_ = false;

    // Reusable buffer
    std::vector<ChunkKey> chunksToRemove_;
};
