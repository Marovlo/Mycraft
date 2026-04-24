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
    if (targetHighlight_.indexCount > 0) {
        engine_.destroyMesh(targetHighlight_);
    }
    blockModel_.destroy(engine_);
    uiRenderer_.destroy();
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
    uiRenderer_.init(&engine_);
    blockModel_.init(engine_, textureAtlas_);

    hud_.init(&uiRenderer_, &blockModel_, &textureAtlas_, &engine_);

    // Starting items
    inventory_.getSlot(0) = {1, 64, 0};   // Grass
    inventory_.getSlot(1) = {2, 64, 0};   // Dirt
    inventory_.getSlot(2) = {10, 64, 0};  // Stone
    inventory_.getSlot(3) = {4, 64, 0};   // Sand
    inventory_.getSlot(4) = {5, 64, 0};   // Oak Log
    inventory_.getSlot(5) = {7, 64, 0};   // Oak Planks
    inventory_.getSlot(6) = {3, 64, 0};   // Cobblestone
    inventory_.getSlot(7) = {9, 64, 0};   // Gravel
    inventory_.getSlot(8) = {6, 64, 0};   // Oak Leaves

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
    double now = glfwGetTime();
    int ticks = tickClock_.advance(now);

    // Handle input BEFORE update() — update() copies current→previous,
    // which would make isKeyPressed() always false
    handleFrameInput();

    // Run fixed-rate game ticks
    for (int i = 0; i < ticks; i++) {
        gameTick();
    }

    // Move input update to AFTER handling — prepares for next frame
    input_.update();
    input_.postUpdate();

    buildMeshes();

    // Interpolate player position between ticks for smooth rendering
    float partial = tickClock_.getPartialTick();
    glm::vec3 renderPos = glm::mix(prevPlayerPos_, player_.position, partial);
    glm::vec3 renderEye = renderPos + glm::vec3(0.0f, PLAYER_EYE_HEIGHT, 0.0f);

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

    // Queue HUD 2D backgrounds
    float sw = static_cast<float>(engine_.getWindowWidth());
    float sh = static_cast<float>(engine_.getWindowHeight());
    hud_.drawBackgrounds(sw, sh, inventory_);

    engine_.updateUniformBuffer(ubo);

    // Build target block highlight
    if (targetHighlight_.indexCount > 0) {
        engine_.destroyMesh(targetHighlight_);
        targetHighlight_ = {};
    }
    hasTarget_ = false;

    if (input_.isCursorLocked()) {
        RayHit hit = raycastWorld(world_, player_.getEyePosition(),
                                  player_.getForward(), MAX_REACH);
        if (hit.hit) {
            hasTarget_ = true;
            // Build outline: 6 thin faces slightly outside the block (wireframe effect)
            float e = 0.002f;  // expansion to prevent z-fighting
            float bx = static_cast<float>(hit.blockX);
            float by = static_cast<float>(hit.blockY);
            float bz = static_cast<float>(hit.blockZ);

            std::vector<Vertex> verts;
            std::vector<uint32_t> idx;
            glm::vec3 n(0, 1, 0);
            glm::vec2 uv(0, 0);  // sample dark pixel

            auto addLine = [&](glm::vec3 a, glm::vec3 b, glm::vec3 offset) {
                uint32_t base = static_cast<uint32_t>(verts.size());
                verts.push_back({a - offset, n, uv});
                verts.push_back({a + offset, n, uv});
                verts.push_back({b + offset, n, uv});
                verts.push_back({b - offset, n, uv});
                idx.push_back(base); idx.push_back(base+1); idx.push_back(base+2);
                idx.push_back(base); idx.push_back(base+2); idx.push_back(base+3);
            };

            float t = 0.01f; // line thickness
            // 12 edges of the cube
            glm::vec3 corners[8] = {
                {bx-e, by-e, bz-e}, {bx+1+e, by-e, bz-e},
                {bx+1+e, by-e, bz+1+e}, {bx-e, by-e, bz+1+e},
                {bx-e, by+1+e, bz-e}, {bx+1+e, by+1+e, bz-e},
                {bx+1+e, by+1+e, bz+1+e}, {bx-e, by+1+e, bz+1+e},
            };
            // Bottom edges
            addLine(corners[0], corners[1], glm::vec3(0, t, 0));
            addLine(corners[1], corners[2], glm::vec3(0, t, 0));
            addLine(corners[2], corners[3], glm::vec3(0, t, 0));
            addLine(corners[3], corners[0], glm::vec3(0, t, 0));
            // Top edges
            addLine(corners[4], corners[5], glm::vec3(0, t, 0));
            addLine(corners[5], corners[6], glm::vec3(0, t, 0));
            addLine(corners[6], corners[7], glm::vec3(0, t, 0));
            addLine(corners[7], corners[4], glm::vec3(0, t, 0));
            // Vertical edges
            addLine(corners[0], corners[4], glm::vec3(t, 0, 0));
            addLine(corners[1], corners[5], glm::vec3(t, 0, 0));
            addLine(corners[2], corners[6], glm::vec3(t, 0, 0));
            addLine(corners[3], corners[7], glm::vec3(t, 0, 0));

            targetHighlight_ = engine_.uploadMesh(verts, idx);
        }
    }
}

void Game::gameTick() {
    const float dt = static_cast<float>(TickClock::TICK_DURATION);

    // Save position before physics for render interpolation
    prevPlayerPos_ = player_.position;

    handleTickInput();
    Physics::update(player_, world_, dt);

    playerChunkX_ = blockToChunk(static_cast<int>(std::floor(player_.position.x)));
    playerChunkZ_ = blockToChunk(static_cast<int>(std::floor(player_.position.z)));

    updateChunks();
    unloadDistantChunks();
}

void Game::handleFrameInput() {
    // ESC: unlock cursor, or quit if already unlocked
    if (input_.isKeyPressed(GLFW_KEY_ESCAPE)) {
        if (input_.isCursorLocked()) {
            input_.setCursorLocked(false);
        } else {
            glfwSetWindowShouldClose(engine_.getWindow(), GLFW_TRUE);
        }
    }

    // Click window to re-lock cursor
    if (!input_.isCursorLocked() && input_.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        input_.setCursorLocked(true);
    }

    // Mouse look (every frame for smoothness)
    if (input_.isCursorLocked()) {
        player_.look(input_.getMouseDeltaX(), input_.getMouseDeltaY());
    }

    // Hotbar selection (1-9)
    for (int i = 0; i < 9; i++) {
        if (input_.isKeyPressed(GLFW_KEY_1 + i)) {
            inventory_.setSelectedSlot(i);
        }
    }

    // Scroll wheel to cycle hotbar
    double scroll = input_.getScrollDelta();
    if (scroll != 0.0) {
        int slot = inventory_.getSelectedSlot();
        slot -= static_cast<int>(scroll);  // scroll up = previous slot
        if (slot < 0) slot += Inventory::HOTBAR_SIZE;
        inventory_.setSelectedSlot(slot % Inventory::HOTBAR_SIZE);
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
                const auto& held = inventory_.getHeldItem();
                if (!held.isEmpty()) {
                    const auto& itemProps = ItemRegistry::instance().get(held.id);
                    if (itemProps.type == ItemType::Block && itemProps.blockId > 0) {
                        world_.setBlock(hit.prevX, hit.prevY, hit.prevZ, itemProps.blockId);
                        // Don't consume in creative-like mode for now
                    }
                }
            }
        }
    }
}

void Game::handleTickInput() {
    // Only continuous-state inputs here (held keys)
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

        if (++meshBuilds >= 2) break; // Limit per frame to avoid GPU stalls
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

    // Draw target block highlight
    if (hasTarget_ && targetHighlight_.indexCount > 0) {
        VkBuffer vb[] = {targetHighlight_.vertexBuffer.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
        vkCmdBindIndexBuffer(cmd, targetHighlight_.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, targetHighlight_.indexCount, 1, 0, 0, 0);
    }

    // Draw 3D block icons in hotbar (reuses 3D pipeline with small viewports)
    float sw = static_cast<float>(engine_.getWindowWidth());
    float sh = static_cast<float>(engine_.getWindowHeight());
    hud_.render3DIcons(cmd, sw, sh, inventory_, engine_.getWindowWidth(), engine_.getWindowHeight());

    // Restore full viewport and world UBO for next frame
    VkViewport fullVp{};
    fullVp.width = static_cast<float>(engine_.getWindowWidth());
    fullVp.height = static_cast<float>(engine_.getWindowHeight());
    fullVp.minDepth = 0.0f;
    fullVp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &fullVp);

    VkRect2D fullScissor{};
    fullScissor.extent = {engine_.getWindowWidth(), engine_.getWindowHeight()};
    vkCmdSetScissor(cmd, 0, 1, &fullScissor);

    // Draw 2D UI overlay (crosshair, hotbar backgrounds)
    uiRenderer_.flush(cmd, engine_.getWindowWidth(), engine_.getWindowHeight());
}
