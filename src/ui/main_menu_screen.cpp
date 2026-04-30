#include "main_menu_screen.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "core/serialization.h"
#include "audio/sound_engine.h"

namespace fs = std::filesystem;

// ============================================================
// MainMenuScreen
// ============================================================

void MainMenuScreen::init(UIRenderer* ui, TextureAtlas* atlas) {
    ui_ = ui;
    atlas_ = atlas;
}

void MainMenuScreen::layoutButtons(float screenW, float screenH) {
    buttons_.clear();

    float btnW = 200.0f;
    float btnH = 30.0f;
    float gap = 10.0f;
    float startY = screenH * 0.50f;
    float centerX = screenW * 0.5f - btnW * 0.5f;

    buttons_.push_back({"SINGLEPLAYER", centerX, startY, btnW, btnH});
    buttons_.push_back({"MULTIPLAYER", centerX, startY + (btnH + gap), btnW, btnH});
    buttons_.push_back({"QUIT GAME", centerX, startY + (btnH + gap) * 2, btnW, btnH});
}

MainMenuScreen::Action MainMenuScreen::update(InputManager& input, float screenW, float screenH) {
    layoutButtons(screenW, screenH);

    double mx = input.getMouseX();
    double my = input.getMouseY();

    hoveredButton_ = -1;
    for (int i = 0; i < static_cast<int>(buttons_.size()); i++) {
        auto& btn = buttons_[i];
        if (mx >= btn.x && mx <= btn.x + btn.w &&
            my >= btn.y && my <= btn.y + btn.h) {
            hoveredButton_ = i;
            break;
        }
    }

    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && hoveredButton_ >= 0) {
        // MC 原版：UI 按钮点击音效
        getSoundEngine().play2D(SoundEventId::UIButtonClick, 0.5f);
        switch (hoveredButton_) {
            case 0: return Action::SinglePlayer;
            case 1: return Action::Multiplayer;
            case 2: return Action::Quit;
        }
    }

    return Action::None;
}

void MainMenuScreen::draw(float screenW, float screenH) {
    // 背景：使用原版 menu_background 平铺（16x16 像素深色泥土纹理）
    ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.1f, 0.1f, 0.15f, 1.0f));

    // 标题 Logo（使用原版 minecraft.png，1024x256 像素）
    // 缩放到合适大小
    float logoScale = screenW * 0.4f / 1024.0f;
    float logoW = 1024.0f * logoScale;
    float logoH = 256.0f * logoScale;
    float logoX = (screenW - logoW) * 0.5f;
    float logoY = screenH * 0.12f;
    ui_->drawGuiSprite("title/minecraft", logoX, logoY, logoW, logoH);

    // 按钮（使用原版 widget/button.png 200x20 像素）
    for (int i = 0; i < static_cast<int>(buttons_.size()); i++) {
        auto& btn = buttons_[i];
        bool hovered = (i == hoveredButton_);

        // MC 原版按钮精灵图
        if (hovered) {
            ui_->drawGuiSprite("widget/button_highlighted", btn.x, btn.y, btn.w, btn.h);
        } else {
            ui_->drawGuiSprite("widget/button", btn.x, btn.y, btn.w, btn.h);
        }

        // 按钮文字
        float textH = 12.0f;
        float textY = btn.y + (btn.h - textH) * 0.5f;
        glm::vec4 textColor = hovered ? glm::vec4(1.0f, 1.0f, 0.5f, 1.0f)
                                      : glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        ui_->drawText(btn.label, btn.x + btn.w * 0.5f, textY, textH, textColor);
    }

    // 版本信息
    ui_->drawTextLeft("MYCRAFT V0.6", 5.0f, screenH - 18.0f, 10.0f,
                      glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
}

// ============================================================
// ServerConnectScreen
// ============================================================

void ServerConnectScreen::init(UIRenderer* ui, TextureAtlas* atlas) {
    ui_ = ui;
    atlas_ = atlas;
}

void ServerConnectScreen::open() {
    addressInput_ = "localhost";
    nameInput_ = "Player";
    activeField_ = 0;
    hoveredButton_ = -1;
    cursorBlinkTime_ = 0.0;
    cursorVisible_ = true;
}

void ServerConnectScreen::parseAddress(const std::string& addr, std::string& host, uint16_t& port) {
    port = 25565;  // 默认端口
    size_t colonPos = addr.rfind(':');
    if (colonPos != std::string::npos && colonPos > 0) {
        host = addr.substr(0, colonPos);
        try {
            int p = std::stoi(addr.substr(colonPos + 1));
            if (p > 0 && p <= 65535) port = static_cast<uint16_t>(p);
        } catch (...) {
            // 解析失败，使用默认端口
        }
    } else {
        host = addr;
    }
}

ServerConnectScreen::Result ServerConnectScreen::update(InputManager& input, float screenW, float screenH) {
    double mx = input.getMouseX();
    double my = input.getMouseY();

    // 光标闪烁
    cursorBlinkTime_ += 1.0 / 60.0;
    if (cursorBlinkTime_ > 0.5) {
        cursorBlinkTime_ = 0.0;
        cursorVisible_ = !cursorVisible_;
    }

    // 输入框点击切换焦点
    float fieldW = 300.0f;
    float fieldH = 28.0f;
    float centerX = (screenW - fieldW) * 0.5f;
    float addrFieldY = screenH * 0.35f;
    float nameFieldY = addrFieldY + 60.0f;

    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (mx >= centerX && mx <= centerX + fieldW) {
            if (my >= addrFieldY && my <= addrFieldY + fieldH) {
                activeField_ = 0;
                cursorBlinkTime_ = 0.0;
                cursorVisible_ = true;
            } else if (my >= nameFieldY && my <= nameFieldY + fieldH) {
                activeField_ = 1;
                cursorBlinkTime_ = 0.0;
                cursorVisible_ = true;
            }
        }
    }

    // 文本输入处理
    std::string textInput = input.consumeTextInput();
    std::string& activeStr = (activeField_ == 0) ? addressInput_ : nameInput_;

    for (char ch : textInput) {
        if (activeField_ == 0) {
            // 地址：允许字母、数字、点、冒号、连字符
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9') || ch == '.' || ch == ':' || ch == '-') {
                if (activeStr.size() < 64) activeStr += ch;
            }
        } else {
            // 玩家名称
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9') || ch == '_' || ch == '-') {
                if (activeStr.size() < 16) activeStr += ch;
            }
        }
    }

    if (input.hasBackspace() && !activeStr.empty()) {
        activeStr.pop_back();
    }

    // Tab 切换字段
    if (input.isKeyPressed(GLFW_KEY_TAB)) {
        activeField_ = (activeField_ + 1) % 2;
        cursorBlinkTime_ = 0.0;
        cursorVisible_ = true;
    }

    // 按钮
    float btnW = 120.0f, btnH = 28.0f;
    float btnY = screenH * 0.7f;
    float connectBtnX = screenW * 0.5f - btnW - 10.0f;
    float cancelBtnX = screenW * 0.5f + 10.0f;

    hoveredButton_ = -1;
    if (mx >= connectBtnX && mx <= connectBtnX + btnW && my >= btnY && my <= btnY + btnH) {
        hoveredButton_ = 0;
    } else if (mx >= cancelBtnX && mx <= cancelBtnX + btnW && my >= btnY && my <= btnY + btnH) {
        hoveredButton_ = 1;
    }

    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (hoveredButton_ == 0 && !addressInput_.empty() && !nameInput_.empty()) {
            getSoundEngine().play2D(SoundEventId::UIButtonClick, 0.5f);
            Result r;
            r.action = Action::Connect;
            parseAddress(addressInput_, r.host, r.port);
            r.playerName = nameInput_;
            return r;
        } else if (hoveredButton_ == 1) {
            getSoundEngine().play2D(SoundEventId::UIButtonClick, 0.5f);
            return {Action::Cancel, "", 0, ""};
        }
    }

    // Enter 连接
    if (input.isKeyPressed(GLFW_KEY_ENTER) && !addressInput_.empty() && !nameInput_.empty()) {
        Result r;
        r.action = Action::Connect;
        parseAddress(addressInput_, r.host, r.port);
        r.playerName = nameInput_;
        return r;
    }

    // ESC 取消
    if (input.isKeyPressed(GLFW_KEY_ESCAPE)) {
        return {Action::Cancel, "", 0, ""};
    }

    return {Action::None, "", 0, ""};
}

void ServerConnectScreen::draw(float screenW, float screenH) {
    // 背景
    ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.12f, 0.12f, 0.15f, 1.0f));

    // 标题
    ui_->drawText("MULTIPLAYER", screenW * 0.5f, 30.0f, 20.0f, glm::vec4(1.0f));

    float fieldW = 300.0f;
    float fieldH = 28.0f;
    float centerX = (screenW - fieldW) * 0.5f;
    float addrFieldY = screenH * 0.35f;
    float nameFieldY = addrFieldY + 60.0f;

    // 服务器地址标签
    ui_->drawText("SERVER ADDRESS", screenW * 0.5f, addrFieldY - 20.0f, 11.0f,
                  glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));

    // 地址输入框
    bool addrActive = (activeField_ == 0);
    glm::vec4 addrBorder = addrActive ? glm::vec4(0.4f, 0.7f, 0.4f, 1.0f)
                                      : glm::vec4(0.3f, 0.3f, 0.3f, 0.8f);
    ui_->drawRect(centerX, addrFieldY, fieldW, fieldH, glm::vec4(0.08f, 0.08f, 0.1f, 0.9f));
    ui_->drawRect(centerX, addrFieldY, fieldW, 1.0f, addrBorder);
    ui_->drawRect(centerX, addrFieldY + fieldH - 1.0f, fieldW, 1.0f, addrBorder);
    ui_->drawRect(centerX, addrFieldY, 1.0f, fieldH, addrBorder);
    ui_->drawRect(centerX + fieldW - 1.0f, addrFieldY, 1.0f, fieldH, addrBorder);

    std::string addrDisplay = addressInput_;
    if (addrActive && cursorVisible_) addrDisplay += "_";
    if (!addrDisplay.empty()) {
        ui_->drawTextLeft(addrDisplay, centerX + 8.0f, addrFieldY + 8.0f, 11.0f,
                          glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // 玩家名称标签
    ui_->drawText("PLAYER NAME", screenW * 0.5f, nameFieldY - 20.0f, 11.0f,
                  glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));

    // 名称输入框
    bool nameActive = (activeField_ == 1);
    glm::vec4 nameBorder = nameActive ? glm::vec4(0.4f, 0.7f, 0.4f, 1.0f)
                                      : glm::vec4(0.3f, 0.3f, 0.3f, 0.8f);
    ui_->drawRect(centerX, nameFieldY, fieldW, fieldH, glm::vec4(0.08f, 0.08f, 0.1f, 0.9f));
    ui_->drawRect(centerX, nameFieldY, fieldW, 1.0f, nameBorder);
    ui_->drawRect(centerX, nameFieldY + fieldH - 1.0f, fieldW, 1.0f, nameBorder);
    ui_->drawRect(centerX, nameFieldY, 1.0f, fieldH, nameBorder);
    ui_->drawRect(centerX + fieldW - 1.0f, nameFieldY, 1.0f, fieldH, nameBorder);

    std::string nameDisplay = nameInput_;
    if (nameActive && cursorVisible_) nameDisplay += "_";
    if (!nameDisplay.empty()) {
        ui_->drawTextLeft(nameDisplay, centerX + 8.0f, nameFieldY + 8.0f, 11.0f,
                          glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // 按钮
    float btnW = 120.0f, btnH = 28.0f;
    float btnY = screenH * 0.7f;
    float connectBtnX = screenW * 0.5f - btnW - 10.0f;
    float cancelBtnX = screenW * 0.5f + 10.0f;

    const char* labels[] = {"CONNECT", "CANCEL"};
    float btnXs[] = {connectBtnX, cancelBtnX};

    for (int i = 0; i < 2; i++) {
        bool hovered = (i == hoveredButton_);
        bool disabled = (i == 0 && (addressInput_.empty() || nameInput_.empty()));

        if (disabled) {
            ui_->drawGuiSprite("widget/button_disabled", btnXs[i], btnY, btnW, btnH);
        } else if (hovered) {
            ui_->drawGuiSprite("widget/button_highlighted", btnXs[i], btnY, btnW, btnH);
        } else {
            ui_->drawGuiSprite("widget/button", btnXs[i], btnY, btnW, btnH);
        }

        float textH = 11.0f;
        float textY = btnY + (btnH - textH) * 0.5f;
        glm::vec4 textColor;
        if (disabled) {
            textColor = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
        } else if (hovered) {
            textColor = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f);
        } else {
            textColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        }
        ui_->drawText(labels[i], btnXs[i] + btnW * 0.5f, textY, textH, textColor);
    }
}

void ServerConnectScreen::drawConnecting(float screenW, float screenH, const std::string& status) {
    ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.12f, 0.12f, 0.15f, 1.0f));
    ui_->drawText("CONNECTING TO SERVER", screenW * 0.5f, screenH * 0.4f, 16.0f,
                  glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    ui_->drawText(status, screenW * 0.5f, screenH * 0.4f + 30.0f, 11.0f,
                  glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
}

// ============================================================
// WorldSelectScreen
// ============================================================

void WorldSelectScreen::init(UIRenderer* ui, TextureAtlas* atlas) {
    ui_ = ui;
    atlas_ = atlas;
}

void WorldSelectScreen::refreshWorldList(const std::string& savesDir) {
    worlds_.clear();
    selectedIndex_ = -1;
    scrollOffset_ = 0;

    std::error_code ec;
    if (!fs::exists(savesDir, ec)) return;

    for (auto& entry : fs::directory_iterator(savesDir, ec)) {
        if (!entry.is_directory()) continue;

        std::string dirName = entry.path().filename().string();
        std::string levelPath = entry.path().string() + "/level.dat";

        if (!fs::exists(levelPath, ec)) continue;

        WorldInfo info;
        info.dirName = dirName;
        info.name = dirName;

        // 尝试读取 level.dat
        BinaryReader r(levelPath);
        if (r.isValid()) {
            uint16_t version;
            if (r.readHeader(VCFile::Type::Level, VCFile::VERSION_LEVEL, version)) {
                info.name = r.readString();
                info.seed = r.readI64();
                /*gameMode*/ r.readU8();
                info.totalTicks = r.readU64();
            }
        }

        // 计算存档大小
        uint64_t totalSize = 0;
        for (auto& f : fs::recursive_directory_iterator(entry.path(), ec)) {
            if (f.is_regular_file()) {
                totalSize += f.file_size(ec);
            }
        }
        info.sizeBytes = totalSize;

        // 最后修改时间
        auto ftime = fs::last_write_time(entry.path(), ec);
        if (!ec) {
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            auto time_t_val = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm_val;
            localtime_r(&time_t_val, &tm_val);
            std::ostringstream oss;
            oss << std::put_time(&tm_val, "%Y-%m-%d %H:%M");
            info.lastPlayed = oss.str();
        }

        worlds_.push_back(std::move(info));
    }

    // 按最后游玩时间排序（最近的在前）
    std::sort(worlds_.begin(), worlds_.end(), [](const WorldInfo& a, const WorldInfo& b) {
        return a.lastPlayed > b.lastPlayed;
    });

    if (!worlds_.empty()) {
        selectedIndex_ = 0;
    }
}

WorldSelectScreen::Result WorldSelectScreen::update(InputManager& input, float screenW, float screenH) {
    double mx = input.getMouseX();
    double my = input.getMouseY();

    // 删除确认对话框
    if (showDeleteConfirm_) {
        float dlgW = 300.0f, dlgH = 120.0f;
        float dlgX = (screenW - dlgW) * 0.5f;
        float dlgY = (screenH - dlgH) * 0.5f;

        float btnW = 100.0f, btnH = 25.0f;
        float btnY = dlgY + dlgH - 40.0f;
        float confirmX = dlgX + dlgW * 0.25f - btnW * 0.5f;
        float cancelX = dlgX + dlgW * 0.75f - btnW * 0.5f;

        deleteHoveredBtn_ = -1;
        if (mx >= confirmX && mx <= confirmX + btnW && my >= btnY && my <= btnY + btnH) {
            deleteHoveredBtn_ = 0;
        } else if (mx >= cancelX && mx <= cancelX + btnW && my >= btnY && my <= btnY + btnH) {
            deleteHoveredBtn_ = 1;
        }

        if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            if (deleteHoveredBtn_ == 0) {
                // 确认删除
                getSoundEngine().play2D(SoundEventId::UIButtonClick, 0.5f);
                showDeleteConfirm_ = false;
                Result r;
                r.action = Action::DeleteWorld;
                r.worldIndex = deleteTargetIndex_;
                return r;
            } else if (deleteHoveredBtn_ == 1) {
                getSoundEngine().play2D(SoundEventId::UIButtonClick, 0.5f);
                showDeleteConfirm_ = false;
            }
        }

        if (input.isKeyPressed(GLFW_KEY_ESCAPE)) {
            showDeleteConfirm_ = false;
        }

        return {Action::None, -1};
    }

    // 世界列表区域
    float listX = screenW * 0.1f;
    float listY = 60.0f;
    float listW = screenW * 0.8f;
    float itemH = 40.0f;
    float listH = screenH - 160.0f;
    int maxVisible = static_cast<int>(listH / itemH);

    // 滚动
    double scroll = input.getScrollDelta();
    if (scroll != 0.0) {
        scrollOffset_ -= static_cast<int>(scroll);
        int maxScroll = std::max(0, static_cast<int>(worlds_.size()) - maxVisible);
        scrollOffset_ = std::clamp(scrollOffset_, 0, maxScroll);
    }

    // 列表项悬停和选择
    hoveredIndex_ = -1;
    for (int i = 0; i < maxVisible && (i + scrollOffset_) < static_cast<int>(worlds_.size()); i++) {
        float itemY = listY + i * itemH;
        if (mx >= listX && mx <= listX + listW && my >= itemY && my <= itemY + itemH) {
            hoveredIndex_ = i + scrollOffset_;
            break;
        }
    }

    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && hoveredIndex_ >= 0) {
        selectedIndex_ = hoveredIndex_;
    }

    // 双击进入世界
    static double lastClickTime = 0.0;
    static int lastClickIndex = -1;
    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && hoveredIndex_ >= 0) {
        double now = glfwGetTime();
        if (hoveredIndex_ == lastClickIndex && (now - lastClickTime) < 0.4) {
            Result r;
            r.action = Action::PlayWorld;
            r.worldIndex = hoveredIndex_;
            return r;
        }
        lastClickTime = now;
        lastClickIndex = hoveredIndex_;
    }

    // 底部按钮
    float btnW = 120.0f, btnH = 28.0f;
    float btnY = screenH - 50.0f;
    float totalBtnW = btnW * 4 + 15.0f * 3;
    float btnStartX = (screenW - totalBtnW) * 0.5f;

    struct BtnRect { float x, y, w, h; };
    BtnRect btns[4] = {
        {btnStartX, btnY, btnW, btnH},
        {btnStartX + btnW + 15.0f, btnY, btnW, btnH},
        {btnStartX + (btnW + 15.0f) * 2, btnY, btnW, btnH},
        {btnStartX + (btnW + 15.0f) * 3, btnY, btnW, btnH}
    };

    hoveredButton_ = -1;
    for (int i = 0; i < 4; i++) {
        if (mx >= btns[i].x && mx <= btns[i].x + btns[i].w &&
            my >= btns[i].y && my <= btns[i].y + btns[i].h) {
            hoveredButton_ = i;
            break;
        }
    }

    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && hoveredButton_ >= 0) {
        // MC 原版：UI 按钮点击音效
        getSoundEngine().play2D(SoundEventId::UIButtonClick, 0.5f);
        switch (hoveredButton_) {
            case 0:  // 进入游戏
                if (selectedIndex_ >= 0) {
                    return {Action::PlayWorld, selectedIndex_};
                }
                break;
            case 1:  // 创建新世界
                return {Action::CreateNew, -1};
            case 2:  // 删除
                if (selectedIndex_ >= 0) {
                    showDeleteConfirm_ = true;
                    deleteTargetIndex_ = selectedIndex_;
                }
                break;
            case 3:  // 返回
                return {Action::Back, -1};
        }
    }

    // ESC 返回
    if (input.isKeyPressed(GLFW_KEY_ESCAPE)) {
        return {Action::Back, -1};
    }

    return {Action::None, -1};
}

void WorldSelectScreen::draw(float screenW, float screenH) {
    // 背景
    ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.12f, 0.12f, 0.15f, 1.0f));

    // 标题
    ui_->drawText("SELECT WORLD", screenW * 0.5f, 20.0f, 20.0f, glm::vec4(1.0f));

    // 世界列表
    float listX = screenW * 0.1f;
    float listY = 60.0f;
    float listW = screenW * 0.8f;
    float itemH = 40.0f;
    float listH = screenH - 160.0f;
    int maxVisible = static_cast<int>(listH / itemH);

    // 列表背景
    ui_->drawRect(listX - 2, listY - 2, listW + 4, listH + 4,
                  glm::vec4(0.05f, 0.05f, 0.08f, 0.9f));

    for (int i = 0; i < maxVisible && (i + scrollOffset_) < static_cast<int>(worlds_.size()); i++) {
        int idx = i + scrollOffset_;
        auto& world = worlds_[idx];
        float itemY = listY + i * itemH;

        // 选中/悬停高亮
        glm::vec4 bgColor(0.15f, 0.15f, 0.18f, 0.8f);
        if (idx == selectedIndex_) {
            bgColor = glm::vec4(0.2f, 0.35f, 0.2f, 0.9f);
        } else if (idx == hoveredIndex_) {
            bgColor = glm::vec4(0.2f, 0.2f, 0.25f, 0.9f);
        }
        ui_->drawRect(listX, itemY, listW, itemH - 2.0f, bgColor);

        // 世界名称
        float nameH = 12.0f;
        ui_->drawTextLeft(world.name, listX + 10.0f, itemY + 5.0f, nameH,
                          glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // 世界信息（种子、大小、最后游玩时间）
        float infoH = 9.0f;
        std::string infoStr = "SEED " + std::to_string(world.seed);
        if (!world.lastPlayed.empty()) {
            infoStr += "  " + world.lastPlayed;
        }
        // 大小格式化
        if (world.sizeBytes > 0) {
            float sizeMB = static_cast<float>(world.sizeBytes) / (1024.0f * 1024.0f);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << sizeMB << " MB";
            infoStr += "  " + oss.str();
        }
        ui_->drawTextLeft(infoStr, listX + 10.0f, itemY + 22.0f, infoH,
                          glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
    }

    // 滚动条（如果需要）
    if (static_cast<int>(worlds_.size()) > maxVisible) {
        float scrollBarH = listH * (static_cast<float>(maxVisible) / worlds_.size());
        float scrollBarY = listY + (listH - scrollBarH) *
            (static_cast<float>(scrollOffset_) / (worlds_.size() - maxVisible));
        ui_->drawRect(listX + listW - 4, scrollBarY, 4, scrollBarH,
                      glm::vec4(0.5f, 0.5f, 0.5f, 0.6f));
    }

    // 空列表提示
    if (worlds_.empty()) {
        ui_->drawText("NO WORLDS FOUND", screenW * 0.5f, screenH * 0.4f, 14.0f,
                      glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
        ui_->drawText("CREATE A NEW WORLD TO BEGIN", screenW * 0.5f, screenH * 0.4f + 25.0f, 11.0f,
                      glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));
    }

    // 底部按钮
    float btnW = 120.0f, btnH = 28.0f;
    float btnY = screenH - 50.0f;
    float totalBtnW = btnW * 4 + 15.0f * 3;
    float btnStartX = (screenW - totalBtnW) * 0.5f;

    const char* labels[] = {"PLAY", "CREATE NEW", "DELETE", "BACK"};
    for (int i = 0; i < 4; i++) {
        float bx = btnStartX + i * (btnW + 15.0f);
        bool hovered = (i == hoveredButton_);
        bool disabled = false;

        // "进入游戏" 和 "删除" 在没有选中世界时禁用
        if ((i == 0 || i == 2) && selectedIndex_ < 0) {
            disabled = true;
        }

        // 使用原版按钮精灵图
        if (disabled) {
            ui_->drawGuiSprite("widget/button_disabled", bx, btnY, btnW, btnH);
        } else if (hovered) {
            ui_->drawGuiSprite("widget/button_highlighted", bx, btnY, btnW, btnH);
        } else {
            ui_->drawGuiSprite("widget/button", bx, btnY, btnW, btnH);
        }

        // 文字
        float textH = 11.0f;
        float textY = btnY + (btnH - textH) * 0.5f;
        glm::vec4 textColor;
        if (disabled) {
            textColor = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
        } else if (hovered) {
            textColor = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f);
        } else {
            textColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        }
        ui_->drawText(labels[i], bx + btnW * 0.5f, textY, textH, textColor);
    }

    // 删除确认对话框
    if (showDeleteConfirm_) {
        // 暗色遮罩
        ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));

        float dlgW = 300.0f, dlgH = 120.0f;
        float dlgX = (screenW - dlgW) * 0.5f;
        float dlgY = (screenH - dlgH) * 0.5f;

        // 对话框背景
        ui_->drawRect(dlgX, dlgY, dlgW, dlgH, glm::vec4(0.15f, 0.15f, 0.18f, 0.95f));
        // 边框
        ui_->drawRect(dlgX, dlgY, dlgW, 1.0f, glm::vec4(0.5f, 0.2f, 0.2f, 1.0f));
        ui_->drawRect(dlgX, dlgY + dlgH - 1.0f, dlgW, 1.0f, glm::vec4(0.5f, 0.2f, 0.2f, 1.0f));
        ui_->drawRect(dlgX, dlgY, 1.0f, dlgH, glm::vec4(0.5f, 0.2f, 0.2f, 1.0f));
        ui_->drawRect(dlgX + dlgW - 1.0f, dlgY, 1.0f, dlgH, glm::vec4(0.5f, 0.2f, 0.2f, 1.0f));

        // 提示文字
        ui_->drawText("DELETE THIS WORLD", dlgX + dlgW * 0.5f, dlgY + 15.0f, 13.0f,
                      glm::vec4(1.0f, 0.3f, 0.3f, 1.0f));

        if (deleteTargetIndex_ >= 0 && deleteTargetIndex_ < static_cast<int>(worlds_.size())) {
            ui_->drawText(worlds_[deleteTargetIndex_].name,
                          dlgX + dlgW * 0.5f, dlgY + 40.0f, 11.0f,
                          glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        }

        ui_->drawText("THIS CANNOT BE UNDONE", dlgX + dlgW * 0.5f, dlgY + 60.0f, 9.0f,
                      glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));

        // 确认/取消按钮
        float dbtnW = 100.0f, dbtnH = 25.0f;
        float dbtnY = dlgY + dlgH - 40.0f;
        float confirmX = dlgX + dlgW * 0.25f - dbtnW * 0.5f;
        float cancelX = dlgX + dlgW * 0.75f - dbtnW * 0.5f;

        // 确认按钮
        bool confirmHover = (deleteHoveredBtn_ == 0);
        ui_->drawRect(confirmX, dbtnY, dbtnW, dbtnH,
                      confirmHover ? glm::vec4(0.5f, 0.2f, 0.2f, 0.9f)
                                   : glm::vec4(0.3f, 0.15f, 0.15f, 0.8f));
        ui_->drawText("DELETE", confirmX + dbtnW * 0.5f, dbtnY + 6.0f, 10.0f,
                      confirmHover ? glm::vec4(1.0f, 0.5f, 0.5f, 1.0f)
                                   : glm::vec4(0.8f, 0.4f, 0.4f, 1.0f));

        // 取消按钮
        bool cancelHover = (deleteHoveredBtn_ == 1);
        ui_->drawRect(cancelX, dbtnY, dbtnW, dbtnH,
                      cancelHover ? glm::vec4(0.3f, 0.3f, 0.3f, 0.9f)
                                  : glm::vec4(0.2f, 0.2f, 0.2f, 0.8f));
        ui_->drawText("CANCEL", cancelX + dbtnW * 0.5f, dbtnY + 6.0f, 10.0f,
                      cancelHover ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
                                  : glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    }
}

// ============================================================
// CreateWorldScreen
// ============================================================

void CreateWorldScreen::init(UIRenderer* ui, TextureAtlas* atlas) {
    ui_ = ui;
    atlas_ = atlas;
}

void CreateWorldScreen::open() {
    nameInput_ = "New World";
    seedInput_ = "";
    activeField_ = 0;
    hoveredButton_ = -1;
    cursorBlinkTime_ = 0.0;
    cursorVisible_ = true;
}

CreateWorldScreen::Result CreateWorldScreen::update(InputManager& input, float screenW, float screenH) {
    double mx = input.getMouseX();
    double my = input.getMouseY();

    // 光标闪烁
    cursorBlinkTime_ += 1.0 / 60.0;  // 近似
    if (cursorBlinkTime_ > 0.5) {
        cursorBlinkTime_ = 0.0;
        cursorVisible_ = !cursorVisible_;
    }

    // 输入框点击切换焦点
    float fieldW = 300.0f;
    float fieldH = 28.0f;
    float centerX = (screenW - fieldW) * 0.5f;
    float nameFieldY = screenH * 0.35f;
    float seedFieldY = nameFieldY + 60.0f;

    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (mx >= centerX && mx <= centerX + fieldW) {
            if (my >= nameFieldY && my <= nameFieldY + fieldH) {
                activeField_ = 0;
                cursorBlinkTime_ = 0.0;
                cursorVisible_ = true;
            } else if (my >= seedFieldY && my <= seedFieldY + fieldH) {
                activeField_ = 1;
                cursorBlinkTime_ = 0.0;
                cursorVisible_ = true;
            }
        }
    }

    // 文本输入处理
    std::string textInput = input.consumeTextInput();
    std::string& activeStr = (activeField_ == 0) ? nameInput_ : seedInput_;

    for (char ch : textInput) {
        if (activeField_ == 1) {
            // 种子只允许数字和负号
            if ((ch >= '0' && ch <= '9') || (ch == '-' && seedInput_.empty())) {
                activeStr += ch;
            }
        } else {
            // 世界名称：允许字母、数字、空格、下划线、连字符
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9') || ch == ' ' || ch == '_' || ch == '-') {
                if (activeStr.size() < 32) {
                    activeStr += ch;
                }
            }
        }
    }

    if (input.hasBackspace() && !activeStr.empty()) {
        activeStr.pop_back();
    }

    // Tab 切换字段
    if (input.isKeyPressed(GLFW_KEY_TAB)) {
        activeField_ = (activeField_ + 1) % 2;
        cursorBlinkTime_ = 0.0;
        cursorVisible_ = true;
    }

    // 按钮
    float btnW = 120.0f, btnH = 28.0f;
    float btnY = screenH * 0.7f;
    float createBtnX = screenW * 0.5f - btnW - 10.0f;
    float cancelBtnX = screenW * 0.5f + 10.0f;

    hoveredButton_ = -1;
    if (mx >= createBtnX && mx <= createBtnX + btnW && my >= btnY && my <= btnY + btnH) {
        hoveredButton_ = 0;
    } else if (mx >= cancelBtnX && mx <= cancelBtnX + btnW && my >= btnY && my <= btnY + btnH) {
        hoveredButton_ = 1;
    }

    if (input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        if (hoveredButton_ == 0 && !nameInput_.empty()) {
            // 创建世界
            getSoundEngine().play2D(SoundEventId::UIButtonClick, 0.5f);
            Result r;
            r.action = Action::Create;
            r.worldName = nameInput_;
            if (seedInput_.empty()) {
                // 随机种子
                std::random_device rd;
                std::mt19937_64 gen(rd());
                std::uniform_int_distribution<int64_t> dist(
                    -999999999LL, 999999999LL);
                r.seed = dist(gen);
            } else {
                try {
                    r.seed = std::stoll(seedInput_);
                } catch (...) {
                    // 如果解析失败，用字符串哈希作为种子
                    std::hash<std::string> hasher;
                    r.seed = static_cast<int64_t>(hasher(seedInput_));
                }
            }
            return r;
        } else if (hoveredButton_ == 1) {
            getSoundEngine().play2D(SoundEventId::UIButtonClick, 0.5f);
            return {Action::Cancel, "", 0};
        }
    }

    // Enter 创建
    if (input.isKeyPressed(GLFW_KEY_ENTER) && !nameInput_.empty()) {
        Result r;
        r.action = Action::Create;
        r.worldName = nameInput_;
        if (seedInput_.empty()) {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            std::uniform_int_distribution<int64_t> dist(-999999999LL, 999999999LL);
            r.seed = dist(gen);
        } else {
            try {
                r.seed = std::stoll(seedInput_);
            } catch (...) {
                std::hash<std::string> hasher;
                r.seed = static_cast<int64_t>(hasher(seedInput_));
            }
        }
        return r;
    }

    // ESC 取消
    if (input.isKeyPressed(GLFW_KEY_ESCAPE)) {
        return {Action::Cancel, "", 0};
    }

    return {Action::None, "", 0};
}

void CreateWorldScreen::draw(float screenW, float screenH) {
    // 背景
    ui_->drawRect(0, 0, screenW, screenH, glm::vec4(0.12f, 0.12f, 0.15f, 1.0f));

    // 标题
    ui_->drawText("CREATE NEW WORLD", screenW * 0.5f, 30.0f, 20.0f, glm::vec4(1.0f));

    float fieldW = 300.0f;
    float fieldH = 28.0f;
    float centerX = (screenW - fieldW) * 0.5f;
    float nameFieldY = screenH * 0.35f;
    float seedFieldY = nameFieldY + 60.0f;

    // 世界名称标签
    ui_->drawText("WORLD NAME", screenW * 0.5f, nameFieldY - 20.0f, 11.0f,
                  glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));

    // 名称输入框
    bool nameActive = (activeField_ == 0);
    glm::vec4 nameBorder = nameActive ? glm::vec4(0.4f, 0.7f, 0.4f, 1.0f)
                                      : glm::vec4(0.3f, 0.3f, 0.3f, 0.8f);
    ui_->drawRect(centerX, nameFieldY, fieldW, fieldH, glm::vec4(0.08f, 0.08f, 0.1f, 0.9f));
    ui_->drawRect(centerX, nameFieldY, fieldW, 1.0f, nameBorder);
    ui_->drawRect(centerX, nameFieldY + fieldH - 1.0f, fieldW, 1.0f, nameBorder);
    ui_->drawRect(centerX, nameFieldY, 1.0f, fieldH, nameBorder);
    ui_->drawRect(centerX + fieldW - 1.0f, nameFieldY, 1.0f, fieldH, nameBorder);

    // 名称文本
    std::string nameDisplay = nameInput_;
    if (nameActive && cursorVisible_) nameDisplay += "_";
    if (!nameDisplay.empty()) {
        ui_->drawTextLeft(nameDisplay, centerX + 8.0f, nameFieldY + 8.0f, 11.0f,
                          glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // 种子标签
    ui_->drawText("SEED  LEAVE BLANK FOR RANDOM", screenW * 0.5f, seedFieldY - 20.0f, 11.0f,
                  glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));

    // 种子输入框
    bool seedActive = (activeField_ == 1);
    glm::vec4 seedBorder = seedActive ? glm::vec4(0.4f, 0.7f, 0.4f, 1.0f)
                                      : glm::vec4(0.3f, 0.3f, 0.3f, 0.8f);
    ui_->drawRect(centerX, seedFieldY, fieldW, fieldH, glm::vec4(0.08f, 0.08f, 0.1f, 0.9f));
    ui_->drawRect(centerX, seedFieldY, fieldW, 1.0f, seedBorder);
    ui_->drawRect(centerX, seedFieldY + fieldH - 1.0f, fieldW, 1.0f, seedBorder);
    ui_->drawRect(centerX, seedFieldY, 1.0f, fieldH, seedBorder);
    ui_->drawRect(centerX + fieldW - 1.0f, seedFieldY, 1.0f, fieldH, seedBorder);

    // 种子文本
    std::string seedDisplay = seedInput_;
    if (seedActive && cursorVisible_) seedDisplay += "_";
    if (!seedDisplay.empty()) {
        ui_->drawTextLeft(seedDisplay, centerX + 8.0f, seedFieldY + 8.0f, 11.0f,
                          glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else if (!seedActive) {
        ui_->drawTextLeft("RANDOM", centerX + 8.0f, seedFieldY + 8.0f, 11.0f,
                          glm::vec4(0.4f, 0.4f, 0.4f, 1.0f));
    }

    // 按钮
    float btnW = 120.0f, btnH = 28.0f;
    float btnY = screenH * 0.7f;
    float createBtnX = screenW * 0.5f - btnW - 10.0f;
    float cancelBtnX = screenW * 0.5f + 10.0f;

    const char* labels[] = {"CREATE", "CANCEL"};
    float btnXs[] = {createBtnX, cancelBtnX};

    for (int i = 0; i < 2; i++) {
        bool hovered = (i == hoveredButton_);
        bool disabled = (i == 0 && nameInput_.empty());

        // 使用原版按钮精灵图
        if (disabled) {
            ui_->drawGuiSprite("widget/button_disabled", btnXs[i], btnY, btnW, btnH);
        } else if (hovered) {
            ui_->drawGuiSprite("widget/button_highlighted", btnXs[i], btnY, btnW, btnH);
        } else {
            ui_->drawGuiSprite("widget/button", btnXs[i], btnY, btnW, btnH);
        }

        float textH = 11.0f;
        float textY = btnY + (btnH - textH) * 0.5f;
        glm::vec4 textColor;
        if (disabled) {
            textColor = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
        } else if (hovered) {
            textColor = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f);
        } else {
            textColor = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        }
        ui_->drawText(labels[i], btnXs[i] + btnW * 0.5f, textY, textH, textColor);
    }
}
