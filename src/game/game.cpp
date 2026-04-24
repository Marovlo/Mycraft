#include "game.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdlib>

Game::Game() = default;

Game::~Game() {
    // Cleanup meshes before engine
    for (auto& [key, chunk] : world_.chunks()) {
        if (chunk.hasMesh()) {
            engine_.destroyMesh(chunk.getMesh());
        }
    }
    if (hasTextureAtlas_) {
        engine_.destroyTexture(blockTextureAtlas_);
    }
    engine_.cleanup();
}

void Game::init() {
    // Register all block types
    BlockRegistry::instance().registerDefaults();

    // Init engine
    if (!engine_.init(1280, 720, "VoxelCraft")) {
        throw std::runtime_error("Failed to init engine");
    }

    // Init input
    input_.init(engine_.getWindow());

    // Init terrain generator
    terrainGen_ = std::make_unique<OverworldGenerator>(42);

    // Generate texture atlas
    generateBlockTexture();

    // Set engine callbacks
    engine_.onUpdate = [this](float dt) { update(dt); };
    engine_.onRender = [this](VkCommandBuffer cmd, uint32_t) { render(cmd); };
}

void Game::run() {
    engine_.run();
}

void Game::update(float dt) {
    dt = std::min(dt, 0.05f);

    input_.update();
    handleInput(dt);

    Physics::update(player_, world_, dt);

    updateChunks();
    buildMeshes();
    unloadDistantChunks();

    // Update UBO
    float aspect = static_cast<float>(engine_.getWindowWidth()) /
                   static_cast<float>(engine_.getWindowHeight());

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view  = player_.getViewMatrix();
    ubo.proj  = player_.getProjectionMatrix(aspect);
    engine_.updateUniformBuffer(ubo);
}

void Game::handleInput(float dt) {
    // ESC to toggle cursor
    if (input_.isKeyPressed(GLFW_KEY_ESCAPE)) {
        input_.toggleCursorLock();
    }

    // Look
    if (input_.isCursorLocked()) {
        player_.look(input_.getMouseDeltaX(), input_.getMouseDeltaY());
    }

    // Movement
    glm::vec3 move(0.0f);
    if (input_.isKeyDown(GLFW_KEY_W)) move += player_.getFlatForward();
    if (input_.isKeyDown(GLFW_KEY_S)) move -= player_.getFlatForward();
    if (input_.isKeyDown(GLFW_KEY_A)) move -= player_.getRight();
    if (input_.isKeyDown(GLFW_KEY_D)) move += player_.getRight();

    if (glm::length(move) > 0.01f) move = glm::normalize(move);

    // Sprint
    player_.sprinting = input_.isKeyDown(GLFW_KEY_LEFT_CONTROL);
    float speed = player_.sprinting ? SPRINT_SPEED : MOVE_SPEED;

    player_.velocity.x = move.x * speed;
    player_.velocity.z = move.z * speed;

    // Jump
    if (input_.isKeyDown(GLFW_KEY_SPACE) && player_.onGround) {
        player_.velocity.y = JUMP_FORCE;
        player_.onGround = false;
    }

    // Block selection (1-9)
    for (int i = 0; i < 9; i++) {
        if (input_.isKeyPressed(GLFW_KEY_1 + i)) {
            BlockId id = static_cast<BlockId>(i + 1);
            if (id < BlockRegistry::instance().blockCount()) {
                player_.selectedBlock = id;
            }
        }
    }

    // Block interaction
    if (input_.isCursorLocked()) {
        if (input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            RayHit hit = raycastWorld(world_, player_.getEyePosition(),
                                      player_.getForward(), MAX_REACH);
            if (hit.hit) {
                world_.setBlock(hit.blockX, hit.blockY, hit.blockZ, Block::Air);
            }
        }

        if (input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            RayHit hit = raycastWorld(world_, player_.getEyePosition(),
                                      player_.getForward(), MAX_REACH);
            if (hit.hit) {
                world_.setBlock(hit.prevX, hit.prevY, hit.prevZ, player_.selectedBlock);
            }
        }
    }
}

void Game::updateChunks() {
    int pcx = blockToChunk(static_cast<int>(std::floor(player_.position.x)));
    int pcz = blockToChunk(static_cast<int>(std::floor(player_.position.z)));

    for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; dx++) {
        for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; dz++) {
            // Circular render distance
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) continue;

            int cx = pcx + dx, cz = pcz + dz;
            auto& chunk = world_.getOrCreateChunk(cx, cz);

            if (!chunk.hasData()) {
                terrainGen_->generate(chunk);
            }
        }
    }
}

void Game::buildMeshes() {
    int pcx = blockToChunk(static_cast<int>(std::floor(player_.position.x)));
    int pcz = blockToChunk(static_cast<int>(std::floor(player_.position.z)));

    int meshBuilds = 0;
    for (auto& [key, chunk] : world_.chunks()) {
        if (!chunk.isMeshDirty()) continue;

        int dx = chunk.chunkX() - pcx;
        int dz = chunk.chunkZ() - pcz;
        if (dx * dx + dz * dz > (RENDER_DISTANCE + 1) * (RENDER_DISTANCE + 1)) continue;

        meshBuilder_.build(world_, chunk);

        if (chunk.hasMesh()) {
            engine_.destroyMesh(chunk.getMesh());
        }

        if (!meshBuilder_.isEmpty()) {
            chunk.setMesh(engine_.uploadMesh(meshBuilder_.getVertices(), meshBuilder_.getIndices()));
        } else {
            chunk.setMesh(Mesh{});
        }
        chunk.clearMeshDirty();

        if (++meshBuilds >= 4) break; // Limit per frame to avoid stalls
    }
}

void Game::unloadDistantChunks() {
    int pcx = blockToChunk(static_cast<int>(std::floor(player_.position.x)));
    int pcz = blockToChunk(static_cast<int>(std::floor(player_.position.z)));

    int unloadDist = RENDER_DISTANCE + 3;

    std::vector<ChunkKey> toRemove;
    for (auto& [key, chunk] : world_.chunks()) {
        int dx = chunk.chunkX() - pcx;
        int dz = chunk.chunkZ() - pcz;
        if (dx * dx + dz * dz > unloadDist * unloadDist) {
            if (chunk.hasMesh()) {
                engine_.destroyMesh(chunk.getMesh());
            }
            toRemove.push_back(key);
        }
    }
    for (auto& k : toRemove) {
        world_.removeChunk(k.x, k.z);
    }
}

void Game::render(VkCommandBuffer cmd) {
    for (auto& [key, chunk] : world_.chunks()) {
        if (!chunk.hasMesh()) continue;

        const auto& mesh = chunk.getMesh();
        VkBuffer vb[] = {mesh.vertexBuffer.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
        vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
    }
}

void Game::generateBlockTexture() {
    // Procedural color texture atlas: one tile per texture ID
    // This is a placeholder — will be replaced with real textures later
    const int TILE = 16;
    uint16_t texCount = BlockRegistry::instance().blockCount() * 2; // Extra for per-face textures
    texCount = std::max(texCount, static_cast<uint16_t>(16));

    // Atlas layout: texCount tiles wide, 1 tile tall
    int atlasW = texCount * TILE;
    int atlasH = TILE;
    std::vector<uint8_t> pixels(atlasW * atlasH * 4, 255);

    // Define colors for each texture ID
    struct TexColor { float r, g, b; };
    TexColor colors[] = {
        {0.45f, 0.75f, 0.25f},  // 0: grass_top
        {0.36f, 0.60f, 0.18f},  // 1: grass_side (greenish-brown)
        {0.55f, 0.37f, 0.24f},  // 2: dirt
        {0.50f, 0.50f, 0.50f},  // 3: stone
        {0.90f, 0.85f, 0.60f},  // 4: sand
        {0.55f, 0.35f, 0.15f},  // 5: oak_log_side
        {0.60f, 0.50f, 0.30f},  // 6: oak_log_top
        {0.20f, 0.55f, 0.12f},  // 7: leaves
        {0.20f, 0.35f, 0.75f},  // 8: water
        {0.45f, 0.45f, 0.45f},  // 9: cobblestone
        {0.65f, 0.50f, 0.28f},  // 10: oak_planks
        {0.30f, 0.30f, 0.30f},  // 11: bedrock
        {0.55f, 0.52f, 0.50f},  // 12: gravel
    };
    int numColors = sizeof(colors) / sizeof(colors[0]);

    for (int t = 0; t < texCount && t < numColors; t++) {
        TexColor c = colors[t];
        for (int y = 0; y < TILE; y++) {
            for (int x = 0; x < TILE; x++) {
                int idx = (y * atlasW + t * TILE + x) * 4;
                // Subtle noise for texture variation
                float noise = ((float)(rand() % 20) - 10.0f) / 255.0f;
                pixels[idx + 0] = static_cast<uint8_t>(std::clamp((int)((c.r + noise) * 255), 0, 255));
                pixels[idx + 1] = static_cast<uint8_t>(std::clamp((int)((c.g + noise) * 255), 0, 255));
                pixels[idx + 2] = static_cast<uint8_t>(std::clamp((int)((c.b + noise) * 255), 0, 255));
                pixels[idx + 3] = 255;
            }
        }
    }

    blockTextureAtlas_ = engine_.uploadTexture(pixels.data(), atlasW, atlasH, 4);
    engine_.updateTextureDescriptor(blockTextureAtlas_.imageView, engine_.getDefaultSampler());
    hasTextureAtlas_ = true;
}
