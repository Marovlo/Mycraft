#include "game.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <GLFW/glfw3.h>
#include "core/item.h"

Game::Game() = default;

Game::~Game() {
    for (auto& [key, chunk] : world_.chunks()) {
        if (chunk.hasMesh()) {
            engine_.destroyMesh(chunk.getMesh());
        }
    }
    crosshair_.destroy(engine_);
    textureAtlas_.destroy(engine_);
    engine_.cleanup();
}

void Game::init() {
    BlockRegistry::instance().registerDefaults();
    ItemRegistry::instance().registerDefaults();

    if (!engine_.init(1280, 720, "VoxelCraft")) {
        throw std::runtime_error("Failed to init engine");
    }

    input_.init(engine_.getWindow());
    terrainGen_ = std::make_unique<OverworldGenerator>(42);

    loadTextureAtlas();

    engine_.onUpdate = [this](float) { update(0.0f); };
    engine_.onRender = [this](VkCommandBuffer cmd, uint32_t) { render(cmd); };
}

void Game::run() {
    engine_.run();
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

void Game::update(float) {
    // Advance tick clock — returns how many fixed ticks to run this frame
    double now = glfwGetTime();
    int ticks = tickClock_.advance(now);

    for (int i = 0; i < ticks; i++) {
        gameTick();
    }

    // Per-frame work (visual, not gameplay)
    buildMeshes();

    // Update UBO for rendering
    float aspect = static_cast<float>(engine_.getWindowWidth()) /
                   static_cast<float>(engine_.getWindowHeight());

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view  = player_.getViewMatrix();
    ubo.proj  = player_.getProjectionMatrix(aspect);
    ubo.fogColor = glm::vec4(0.53f, 0.81f, 0.92f, 1.0f);
    ubo.viewPos  = glm::vec4(player_.getEyePosition(), 1.0f);
    float fogStart = static_cast<float>((RENDER_DISTANCE - 2) * CHUNK_SIZE);
    float fogEnd   = static_cast<float>(RENDER_DISTANCE * CHUNK_SIZE);
    ubo.fogRange = glm::vec2(fogStart, fogEnd);
    // Update crosshair position
    crosshair_.update(engine_, player_.getEyePosition(),
                      player_.getForward(), player_.getRight(),
                      glm::vec3(0, 1, 0));

    engine_.updateUniformBuffer(ubo);
}

void Game::gameTick() {
    const float dt = static_cast<float>(TickClock::TICK_DURATION);

    input_.update();
    handleInput();
    input_.postUpdate();

    Physics::update(player_, world_, dt);

    playerChunkX_ = blockToChunk(static_cast<int>(std::floor(player_.position.x)));
    playerChunkZ_ = blockToChunk(static_cast<int>(std::floor(player_.position.z)));

    updateChunks();
    unloadDistantChunks();
}

void Game::handleInput() {
    if (input_.isKeyPressed(GLFW_KEY_ESCAPE)) {
        input_.toggleCursorLock();
    }

    if (input_.isCursorLocked()) {
        player_.look(input_.getMouseDeltaX(), input_.getMouseDeltaY());
    }

    glm::vec3 move(0.0f);
    if (input_.isKeyDown(GLFW_KEY_W)) move += player_.getFlatForward();
    if (input_.isKeyDown(GLFW_KEY_S)) move -= player_.getFlatForward();
    if (input_.isKeyDown(GLFW_KEY_A)) move -= player_.getRight();
    if (input_.isKeyDown(GLFW_KEY_D)) move += player_.getRight();

    if (glm::length(move) > 0.01f) move = glm::normalize(move);

    player_.sprinting = input_.isKeyDown(GLFW_KEY_LEFT_CONTROL);
    float speed = player_.sprinting ? SPRINT_SPEED : MOVE_SPEED;

    player_.velocity.x = move.x * speed;
    player_.velocity.z = move.z * speed;

    if (input_.isKeyDown(GLFW_KEY_SPACE) && player_.onGround) {
        player_.velocity.y = JUMP_FORCE;
        player_.onGround = false;
    }

    for (int i = 0; i < 9; i++) {
        if (input_.isKeyPressed(GLFW_KEY_1 + i)) {
            BlockId id = static_cast<BlockId>(i + 1);
            if (id < BlockRegistry::instance().blockCount()) {
                player_.selectedBlock = id;
            }
        }
    }

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
    for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; dx++) {
        for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; dz++) {
            if (dx * dx + dz * dz > RENDER_DISTANCE * RENDER_DISTANCE) continue;

            int cx = playerChunkX_ + dx, cz = playerChunkZ_ + dz;
            auto& chunk = world_.getOrCreateChunk(cx, cz);

            if (!chunk.hasData()) {
                terrainGen_->generate(chunk);
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
        if ((!nxp || !nxp->hasData()) ||
            (!nxn || !nxn->hasData()) ||
            (!nzp || !nzp->hasData()) ||
            (!nzn || !nzn->hasData())) {
            continue;
        }

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

        if (++meshBuilds >= 4) break;
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
            if (chunk.hasMesh()) {
                engine_.destroyMesh(chunk.getMesh());
            }
            chunksToRemove_.push_back(key);
        }
    }
    for (auto& k : chunksToRemove_) {
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

    // Draw crosshair on top
    crosshair_.render(cmd);
}
