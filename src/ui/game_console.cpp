// game_console.cpp — 游戏内控制台实现
// 支持命令输入、历史翻阅、输出消息显示

#include "game_console.h"
#include "renderer/ui_renderer.h"
#include "core/input.h"
#include <algorithm>
#include <sstream>
#include <GLFW/glfw3.h>

void GameConsole::init(UIRenderer* ui) {
    ui_ = ui;
}

void GameConsole::open() {
    open_ = true;
    inputLine_.clear();
    inputLine_ = "/";  // 默认以 / 开头
    cursorPos_ = 1;
    historyIndex_ = -1;
    savedInput_.clear();
}

void GameConsole::close() {
    open_ = false;
    inputLine_.clear();
    cursorPos_ = 0;
}

bool GameConsole::handleInput(InputManager& input) {
    if (!open_) return false;

    // ESC：关闭控制台
    if (input.isKeyPressed(GLFW_KEY_ESCAPE)) {
        close();
        return true;
    }

    // 回车：执行命令（不关闭控制台，继续等待输入）
    if (input.hasEnter()) {
        if (!inputLine_.empty()) {
            // 添加到命令历史
            std::string cmd = inputLine_;
            if (commandHistory_.empty() || commandHistory_.back() != cmd) {
                commandHistory_.push_back(cmd);
                if (static_cast<int>(commandHistory_.size()) > MAX_HISTORY) {
                    commandHistory_.erase(commandHistory_.begin());
                }
            }
            // 显示输入的命令
            print("> " + cmd, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            // 执行
            executeCommand(cmd);
        }
        // 清空输入行，准备下一条命令
        inputLine_ = "/";
        cursorPos_ = 1;
        historyIndex_ = -1;
        savedInput_.clear();
        return true;
    }

    // 退格键
    if (input.hasBackspace()) {
        if (cursorPos_ > 0 && !inputLine_.empty()) {
            inputLine_.erase(cursorPos_ - 1, 1);
            cursorPos_--;
        }
    }

    // 上箭头：翻阅历史（更早的命令）
    if (input.hasArrowUp()) {
        if (!commandHistory_.empty()) {
            if (historyIndex_ == -1) {
                savedInput_ = inputLine_;
                historyIndex_ = static_cast<int>(commandHistory_.size()) - 1;
            } else if (historyIndex_ > 0) {
                historyIndex_--;
            }
            inputLine_ = commandHistory_[historyIndex_];
            cursorPos_ = static_cast<int>(inputLine_.size());
        }
    }

    // 下箭头：翻阅历史（更新的命令）
    if (input.hasArrowDown()) {
        if (historyIndex_ >= 0) {
            historyIndex_++;
            if (historyIndex_ >= static_cast<int>(commandHistory_.size())) {
                historyIndex_ = -1;
                inputLine_ = savedInput_;
            } else {
                inputLine_ = commandHistory_[historyIndex_];
            }
            cursorPos_ = static_cast<int>(inputLine_.size());
        }
    }

    // 字符输入
    std::string chars = input.consumeTextInput();
    if (!chars.empty()) {
        inputLine_.insert(cursorPos_, chars);
        cursorPos_ += static_cast<int>(chars.size());
    }

    return true;  // 控制台打开时消费所有输入
}

void GameConsole::draw(float screenW, float screenH) {
    if (!ui_ || !open_) return;

    int scale = std::clamp(static_cast<int>(screenH / 240.0f), 2, 4);
    float glyphH = 7.0f * scale;
    float pad = 4.0f * scale;
    float lineH = glyphH + pad;

    // 控制台背景：屏幕底部往上
    float consoleH = lineH * (VISIBLE_LINES + 1) + pad * 2;  // +1 for input line
    float consoleY = screenH - consoleH;

    // 半透明黑色背景
    ui_->drawRect(0, consoleY, screenW, consoleH, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f));

    // 分隔线（输入行上方）
    float inputY = screenH - lineH - pad;
    ui_->drawRect(0, inputY - 1.0f * scale, screenW, 1.0f * scale,
                  glm::vec4(0.5f, 0.5f, 0.5f, 0.8f));

    // 绘制输入行
    // 闪烁光标：每 0.5 秒切换
    double time = glfwGetTime();
    bool showCursor = (static_cast<int>(time * 2.0) % 2 == 0);
    std::string displayLine = inputLine_;
    if (showCursor) {
        // 在光标位置插入 '_'
        if (cursorPos_ <= static_cast<int>(displayLine.size())) {
            displayLine.insert(cursorPos_, "_");
        } else {
            displayLine += "_";
        }
    }
    ui_->drawTextLeft(displayLine, pad, inputY, glyphH,
                      glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // 绘制消息历史（从下往上）
    int msgCount = static_cast<int>(messages_.size());
    int startIdx = std::max(0, msgCount - VISIBLE_LINES);
    float msgY = inputY - lineH - pad * 0.5f;
    for (int i = msgCount - 1; i >= startIdx && msgY >= consoleY; i--) {
        const auto& msg = messages_[i];
        ui_->drawTextLeft(msg.text, pad, msgY, glyphH, msg.color);
        msgY -= lineH;
    }
}

void GameConsole::registerCommand(const std::string& name, const std::string& usage,
                                   CommandHandler handler) {
    commands_.push_back({name, usage, std::move(handler)});
}

void GameConsole::print(const std::string& msg, const glm::vec4& color) {
    messages_.push_back({msg, color});
    if (static_cast<int>(messages_.size()) > MAX_MESSAGES) {
        messages_.erase(messages_.begin());
    }
}

void GameConsole::executeCommand(const std::string& line) {
    // 去掉开头的 /
    std::string cmdLine = line;
    if (!cmdLine.empty() && cmdLine[0] == '/') {
        cmdLine = cmdLine.substr(1);
    }
    if (cmdLine.empty()) return;

    auto tokens = tokenize(cmdLine);
    if (tokens.empty()) return;

    std::string cmdName = tokens[0];
    // 转小写
    std::transform(cmdName.begin(), cmdName.end(), cmdName.begin(), ::tolower);

    // 查找命令
    for (const auto& cmd : commands_) {
        if (cmd.name == cmdName) {
            std::vector<std::string> args(tokens.begin() + 1, tokens.end());
            std::string result = cmd.handler(args);
            if (!result.empty()) {
                print(result, glm::vec4(0.6f, 1.0f, 0.6f, 1.0f));
            }
            return;
        }
    }

    // help 命令（内置）
    if (cmdName == "help") {
        print("=== Available Commands ===", glm::vec4(1.0f, 1.0f, 0.5f, 1.0f));
        print("/help - Show this help", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        for (const auto& cmd : commands_) {
            print("/" + cmd.name + " " + cmd.usage, glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        }
        return;
    }

    print("Unknown command: " + cmdName + ". Type /help for list.",
          glm::vec4(1.0f, 0.4f, 0.4f, 1.0f));
}

std::vector<std::string> GameConsole::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}
