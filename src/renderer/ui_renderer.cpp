#include "ui_renderer.h"

// 将字符映射到字体 tile 名称，返回空字符串表示跳过（空格等）
static std::string charToTileName(char ch) {
    if (ch >= 'A' && ch <= 'Z')
        return std::string("font_letter_") + static_cast<char>(ch - 'A' + 'a');
    if (ch >= 'a' && ch <= 'z')
        return std::string("font_letter_") + ch;
    if (ch >= '0' && ch <= '9')
        return std::string("font_digit_") + ch;
    switch (ch) {
        case '/':  return "font_slash";
        case '.':  return "font_dot";
        case '-':  return "font_minus";
        case '_':  return "font_underscore";
        case ':':  return "font_colon";
        case ',':  return "font_comma";
        case '(':  return "font_lparen";
        case ')':  return "font_rparen";
        case '~':  return "font_tilde";
        case '@':  return "font_at";
        case '#':  return "font_hash";
        case '+':  return "font_plus";
        case '=':  return "font_equal";
        case '!':  return "font_excl";
        case '?':  return "font_question";
        case '>':  return "font_gt";
        case '<':  return "font_lt";
        case ';':  return "font_semicolon";
        case '[':  return "font_lbracket";
        case ']':  return "font_rbracket";
        default:   return ""; // 空格或未知字符 → 跳过
    }
}

void UIRenderer::init(VulkanEngine* engine) {
    engine_ = engine;

    // Pre-allocate dynamic buffers
    vertexBufferSize_ = sizeof(UIVertex) * INITIAL_VERTS;
    indexBufferSize_ = sizeof(uint32_t) * INITIAL_INDICES;
    vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    indexBuffer_ = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void UIRenderer::destroy() {
    if (engine_) {
        if (vertexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
            vertexBuffer_ = {};
        }
        if (indexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
            indexBuffer_ = {};
        }
        if (guiVertexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), guiVertexBuffer_.buffer, guiVertexBuffer_.allocation);
            guiVertexBuffer_ = {};
        }
        if (guiIndexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), guiIndexBuffer_.buffer, guiIndexBuffer_.allocation);
            guiIndexBuffer_ = {};
        }
    }
    engine_ = nullptr;
}

void UIRenderer::addQuad(float x0, float y0, float x1, float y1,
                         float u0, float v0, float u1, float v1,
                         const glm::vec4& color) {
    uint32_t base = static_cast<uint32_t>(vertices_.size());

    vertices_.push_back({{x0, y0}, {u0, v0}, color});
    vertices_.push_back({{x1, y0}, {u1, v0}, color});
    vertices_.push_back({{x1, y1}, {u1, v1}, color});
    vertices_.push_back({{x0, y1}, {u0, v1}, color});

    indices_.push_back(base + 0);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);
    indices_.push_back(base + 0);
    indices_.push_back(base + 2);
    indices_.push_back(base + 3);
}

void UIRenderer::drawRect(float x, float y, float w, float h, const glm::vec4& color) {
    // 使用 GUI 图集中的白色像素精灵绘制纯色矩形
    // 这样 drawRect 和 drawGuiSprite 在同一个渲染 Pass 中，按绘制顺序正确叠加
    if (guiAtlas_ && guiAtlas_->isBuilt()) {
        const auto& sp = guiAtlas_->getSprite("_white");
        if (sp.pixelW > 0) {
            addGuiQuad(x, y, x + w, y + h, sp.u0, sp.v0, sp.u1, sp.v1, color);
            return;
        }
    }
    // 回退：使用方块图集缓冲区（GUI 图集未就绪时）
    addQuad(x, y, x + w, y + h, 0.0f, 0.0f, 0.001f, 0.001f, color);
}

void UIRenderer::drawTexturedRect(float x, float y, float w, float h,
                                  float u0, float v0, float u1, float v1,
                                  const glm::vec4& tint) {
    addQuad(x, y, x + w, y + h, u0, v0, u1, v1, tint);
}

void UIRenderer::drawCrosshair(float screenW, float screenH, float size, float thickness) {
    float cx = screenW * 0.5f;
    float cy = screenH * 0.5f;
    float hs = size * 0.5f;
    float ht = thickness * 0.5f;

    glm::vec4 color(1.0f, 1.0f, 1.0f, 0.9f);

    // Horizontal bar
    drawRect(cx - hs, cy - ht, size, thickness, color);
    // Vertical bar
    drawRect(cx - ht, cy - hs, thickness, size, color);
}

void UIRenderer::drawTexturedTri(const UIVertex& a, const UIVertex& b, const UIVertex& c) {
    uint32_t base = static_cast<uint32_t>(vertices_.size());
    vertices_.push_back(a);
    vertices_.push_back(b);
    vertices_.push_back(c);
    indices_.push_back(base + 0);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);
}

void UIRenderer::drawNumber(int value, float rightX, float y, float glyphH,
                            const glm::vec4& color) {
    if (!atlas_) return;
    if (value < 0) value = 0;

    // Extract digits least-significant-first, then render right-to-left so we
    // can walk from rightX leftward without allocating a buffer.
    // Glyph aspect: 5x7 source → width ≈ glyphH * 5/7, but for readable tight
    // MC-style digits we simplify to width = glyphH * 0.6 and pack 1-px apart.
    float glyphW  = glyphH * 0.6f;
    float advance = glyphW + glyphH * 0.1f;     // small gap between digits

    // Build the digit sequence.
    int digits[10];
    int nDigits = 0;
    if (value == 0) {
        digits[nDigits++] = 0;
    } else {
        int v = value;
        while (v > 0 && nDigits < 10) {
            digits[nDigits++] = v % 10;
            v /= 10;
        }
    }

    // Drop shadow first (black, 1-px offset) to match MC look.
    float shadow = glyphH * 0.1f;
    glm::vec4 shadowColor(0.0f, 0.0f, 0.0f, color.a);

    auto drawDigit = [&](int d, float x, float yy, const glm::vec4& tint) {
        uint16_t tile = atlas_->getTileIndex("font_digit_" + std::to_string(d));
        glm::vec4 uv  = atlas_->getTileUV(tile);
        drawTexturedRect(x, yy, glyphW, glyphH, uv.x, uv.y, uv.z, uv.w, tint);
    };

    float cursor = rightX;
    for (int i = 0; i < nDigits; ++i) {
        int d = digits[i];               // least-significant first
        cursor -= glyphW;
        drawDigit(d, cursor + shadow, y + shadow, shadowColor);
        drawDigit(d, cursor,          y,          color);
        cursor -= (advance - glyphW);
    }
}

void UIRenderer::drawText(const std::string& text, float centerX, float y, float glyphH,
                          const glm::vec4& color) {
    if (!atlas_ || text.empty()) return;

    float glyphW  = glyphH * 0.6f;
    float advance = glyphW + glyphH * 0.1f;
    float totalW  = text.size() * advance - glyphH * 0.1f;
    float startX  = centerX - totalW * 0.5f;

    float shadow = glyphH * 0.1f;
    glm::vec4 shadowColor(0.0f, 0.0f, 0.0f, color.a);

    auto drawGlyph = [&](const std::string& tileName, float x, float yy, const glm::vec4& tint) {
        uint16_t tile = atlas_->getTileIndex(tileName);
        glm::vec4 uv = atlas_->getTileUV(tile);
        drawTexturedRect(x, yy, glyphW, glyphH, uv.x, uv.y, uv.z, uv.w, tint);
    };

    float cx = startX;
    for (char ch : text) {
        std::string tileName = charToTileName(ch);
        if (tileName.empty()) {
            cx += advance;
            continue;
        }
        drawGlyph(tileName, cx + shadow, y + shadow, shadowColor);
        drawGlyph(tileName, cx, y, color);
        cx += advance;
    }
}

void UIRenderer::drawTextLeft(const std::string& text, float leftX, float y, float glyphH,
                              const glm::vec4& color) {
    if (!atlas_ || text.empty()) return;

    float glyphW  = glyphH * 0.6f;
    float advance = glyphW + glyphH * 0.1f;

    float shadow = glyphH * 0.1f;
    glm::vec4 shadowColor(0.0f, 0.0f, 0.0f, color.a);

    auto drawGlyph = [&](const std::string& tileName, float x, float yy, const glm::vec4& tint) {
        uint16_t tile = atlas_->getTileIndex(tileName);
        glm::vec4 uv = atlas_->getTileUV(tile);
        drawTexturedRect(x, yy, glyphW, glyphH, uv.x, uv.y, uv.z, uv.w, tint);
    };

    float cx = leftX;
    for (char ch : text) {
        std::string tileName = charToTileName(ch);
        if (tileName.empty()) {
            cx += advance;
            continue;
        }
        drawGlyph(tileName, cx + shadow, y + shadow, shadowColor);
        drawGlyph(tileName, cx, y, color);
        cx += advance;
    }
}

// ============================================================
// GUI 精灵绘制（使用 GUI 图集纹理）
// ============================================================

void UIRenderer::addGuiQuad(float x0, float y0, float x1, float y1,
                            float u0, float v0, float u1, float v1,
                            const glm::vec4& color) {
    uint32_t base = static_cast<uint32_t>(guiVertices_.size());
    guiVertices_.push_back({{x0, y0}, {u0, v0}, color});
    guiVertices_.push_back({{x1, y0}, {u1, v0}, color});
    guiVertices_.push_back({{x1, y1}, {u1, v1}, color});
    guiVertices_.push_back({{x0, y1}, {u0, v1}, color});
    guiIndices_.push_back(base + 0);
    guiIndices_.push_back(base + 1);
    guiIndices_.push_back(base + 2);
    guiIndices_.push_back(base + 0);
    guiIndices_.push_back(base + 2);
    guiIndices_.push_back(base + 3);
}

void UIRenderer::drawGuiSprite(const std::string& spriteName, float x, float y, float w, float h,
                               const glm::vec4& tint) {
    if (!guiAtlas_ || !guiAtlas_->isBuilt()) return;
    const auto& sp = guiAtlas_->getSprite(spriteName);
    if (sp.pixelW == 0) return;  // 找不到精灵
    addGuiQuad(x, y, x + w, y + h, sp.u0, sp.v0, sp.u1, sp.v1, tint);
}

void UIRenderer::drawGuiSpriteRegion(const std::string& spriteName,
                                     float x, float y, float w, float h,
                                     float srcX, float srcY, float srcW, float srcH,
                                     const glm::vec4& tint) {
    if (!guiAtlas_ || !guiAtlas_->isBuilt()) return;
    const auto& sp = guiAtlas_->getSprite(spriteName);
    if (sp.pixelW == 0) return;
    // 计算子区域的 UV
    float uRange = sp.u1 - sp.u0;
    float vRange = sp.v1 - sp.v0;
    float su0 = sp.u0 + (srcX / sp.pixelW) * uRange;
    float sv0 = sp.v0 + (srcY / sp.pixelH) * vRange;
    float su1 = sp.u0 + ((srcX + srcW) / sp.pixelW) * uRange;
    float sv1 = sp.v0 + ((srcY + srcH) / sp.pixelH) * vRange;
    addGuiQuad(x, y, x + w, y + h, su0, sv0, su1, sv1, tint);
}

void UIRenderer::drawGuiSpriteUV(const GuiSprite& sprite, float x, float y, float w, float h,
                                  const glm::vec4& tint) {
    if (sprite.pixelW == 0) return;
    addGuiQuad(x, y, x + w, y + h, sprite.u0, sprite.v0, sprite.u1, sprite.v1, tint);
}

// ============================================================
// Buffer management
// ============================================================

void UIRenderer::ensureBufferCapacity(size_t vertCount, size_t idxCount) {
    VkDeviceSize neededVB = sizeof(UIVertex) * vertCount;
    VkDeviceSize neededIB = sizeof(uint32_t) * idxCount;

    if (neededVB > vertexBufferSize_) {
        if (vertexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), vertexBuffer_.buffer, vertexBuffer_.allocation);
        }
        vertexBufferSize_ = neededVB * 2;
        vertexBuffer_ = engine_->createDynamicBuffer(vertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    if (neededIB > indexBufferSize_) {
        if (indexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), indexBuffer_.buffer, indexBuffer_.allocation);
        }
        indexBufferSize_ = neededIB * 2;
        indexBuffer_ = engine_->createDynamicBuffer(indexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }
}

void UIRenderer::ensureGuiBufferCapacity(size_t vertCount, size_t idxCount) {
    VkDeviceSize neededVB = sizeof(UIVertex) * vertCount;
    VkDeviceSize neededIB = sizeof(uint32_t) * idxCount;

    if (neededVB > guiVertexBufferSize_) {
        if (guiVertexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), guiVertexBuffer_.buffer, guiVertexBuffer_.allocation);
        }
        guiVertexBufferSize_ = neededVB * 2;
        guiVertexBuffer_ = engine_->createDynamicBuffer(guiVertexBufferSize_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    }

    if (neededIB > guiIndexBufferSize_) {
        if (guiIndexBuffer_.allocation) {
            vmaDestroyBuffer(engine_->getAllocator(), guiIndexBuffer_.buffer, guiIndexBuffer_.allocation);
        }
        guiIndexBufferSize_ = neededIB * 2;
        guiIndexBuffer_ = engine_->createDynamicBuffer(guiIndexBufferSize_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    }
}

// ============================================================
// Flush — 两阶段渲染：先方块图集，再 GUI 图集
// ============================================================

void UIRenderer::flushBatch(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight,
                            const std::vector<UIVertex>& verts, const std::vector<uint32_t>& idxs,
                            VkDescriptorSet descriptorSet) {
    // 此方法现在仅用于方块图集 Pass
    if (verts.empty()) return;

    ensureBufferCapacity(verts.size(), idxs.size());

    void* vData = engine_->mapBuffer(vertexBuffer_);
    memcpy(vData, verts.data(), sizeof(UIVertex) * verts.size());
    engine_->unmapBuffer(vertexBuffer_);

    void* iData = engine_->mapBuffer(indexBuffer_);
    memcpy(iData, idxs.data(), sizeof(uint32_t) * idxs.size());
    engine_->unmapBuffer(indexBuffer_);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine_->getUIPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

    VkBuffer vb[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(idxs.size()), 1, 0, 0, 0);
}

void UIRenderer::flushGuiBatch(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight,
                               const std::vector<UIVertex>& verts, const std::vector<uint32_t>& idxs,
                               VkDescriptorSet descriptorSet) {
    // GUI 图集使用独立的 buffer，避免与方块图集 buffer 竞争
    if (verts.empty()) return;

    ensureGuiBufferCapacity(verts.size(), idxs.size());

    void* vData = engine_->mapBuffer(guiVertexBuffer_);
    memcpy(vData, verts.data(), sizeof(UIVertex) * verts.size());
    engine_->unmapBuffer(guiVertexBuffer_);

    void* iData = engine_->mapBuffer(guiIndexBuffer_);
    memcpy(iData, idxs.data(), sizeof(uint32_t) * idxs.size());
    engine_->unmapBuffer(guiIndexBuffer_);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine_->getUIPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

    VkBuffer vb[] = {guiVertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vb, offsets);
    vkCmdBindIndexBuffer(cmd, guiIndexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(idxs.size()), 1, 0, 0, 0);
}

void UIRenderer::flush(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight) {
    flushWithTexture(cmd, screenWidth, screenHeight, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void UIRenderer::flushWithTexture(VkCommandBuffer cmd, uint32_t screenWidth, uint32_t screenHeight,
                                   VkImageView textureView, VkSampler sampler) {
    bool hasMain = !vertices_.empty();
    bool hasGui  = !guiVertices_.empty();

    if (!hasMain && !hasGui) {
        return;
    }

    if (!engine_) {
        vertices_.clear(); indices_.clear();
        guiVertices_.clear(); guiIndices_.clear();
        return;
    }

    // Switch to UI pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_->getUIPipeline());

    // Viewport 和 scissor 使用 framebuffer 像素尺寸（Retina 屏幕上 = 窗口坐标 × DPI 缩放）
    uint32_t fbWidth = engine_->getWindowWidth();
    uint32_t fbHeight = engine_->getWindowHeight();

    VkViewport viewport{};
    viewport.width = static_cast<float>(fbWidth);
    viewport.height = static_cast<float>(fbHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = {fbWidth, fbHeight};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Push constants 使用窗口坐标尺寸（与 GLFW 鼠标坐标和 UI 逻辑坐标一致）
    // UI 着色器将窗口坐标映射到 NDC [-1,1]，viewport 再将 NDC 映射到 framebuffer 像素
    UIPushConstants pc{};
    pc.screenSize = {static_cast<float>(screenWidth), static_cast<float>(screenHeight)};
    vkCmdPushConstants(cmd, engine_->getUIPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UIPushConstants), &pc);

    // Pass 1: GUI 图集内容（背景矩形、按钮背景、Logo、HUD 精灵等）
    // 先渲染 GUI 背景层，确保后续的文字和方块图标能覆盖在上面
    // 使用独立的 buffer，避免与方块图集 Pass 的 buffer 竞争导致闪烁
    if (hasGui && guiAtlas_ && guiAtlas_->isBuilt()) {
        if (!guiDescriptorAllocated_) {
            guiDescriptorSet_ = engine_->allocateExtraDescriptorSet(
                guiAtlas_->getImageView(), guiAtlas_->getSampler());
            guiDescriptorAllocated_ = true;
        }
        flushGuiBatch(cmd, screenWidth, screenHeight, guiVertices_, guiIndices_,
                      guiDescriptorSet_);
    }

    // Pass 2: 方块图集内容（文字、方块图标、纯色矩形等）
    // 后渲染，确保文字和图标显示在 GUI 背景之上
    if (hasMain) {
        // 如果指定了自定义纹理，临时更新 descriptor
        if (textureView != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE) {
            engine_->updateTextureDescriptor(textureView, sampler);
        }
        flushBatch(cmd, screenWidth, screenHeight, vertices_, indices_,
                   engine_->getCurrentFrame().descriptorSet);
    }

    // Restore 3D pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine_->getPipeline());
    // 恢复方块纹理 descriptor
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        engine_->getPipelineLayout(), 0, 1,
        &engine_->getCurrentFrame().descriptorSet, 0, nullptr);

    vertices_.clear();
    indices_.clear();
    guiVertices_.clear();
    guiIndices_.clear();
}
