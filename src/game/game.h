#pragma once

#include "engine/vulkan_engine.h"
#include "core/input.h"
#include "core/block.h"
#include "world/world.h"
#include "world/terrain_generator.h"
#include "renderer/mesh_builder.h"
#include "player/player.h"
#include "player/physics.h"

#include <memory>

class Game {
public:
    Game();
    ~Game();

    void init();
    void run();

private:
    void update(float dt);
    void render(VkCommandBuffer cmd);

    void handleInput(float dt);
    void updateChunks();
    void buildMeshes();
    void unloadDistantChunks();
    void generateBlockTexture();

    // Systems
    VulkanEngine engine_;
    InputManager input_;
    World world_;
    Player player_;
    std::unique_ptr<TerrainGenerator> terrainGen_;
    MeshBuilder meshBuilder_;

    // Texture atlas
    AllocatedImage blockTextureAtlas_;
    bool hasTextureAtlas_ = false;
};
