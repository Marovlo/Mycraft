#pragma once

#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

class UIRenderer;
class InputManager;

// 游戏内控制台 — 按 / 打开，支持调试命令
// 命令格式: /command arg1 arg2 ...
class GameConsole {
public:
    // 命令回调: (命令名, 参数列表) → 返回输出消息
    using CommandHandler = std::function<std::string(const std::vector<std::string>& args)>;

    void init(UIRenderer* ui);

    // 打开/关闭控制台
    void open();
    void close();
    bool isOpen() const { return open_; }

    // 每帧处理输入（在 handleFrameInput 中调用）
    // 返回 true 表示控制台消费了输入，游戏不应处理
    bool handleInput(InputManager& input);

    // 渲染控制台 UI
    void draw(float screenW, float screenH);

    // 注册命令处理器
    void registerCommand(const std::string& name, const std::string& usage,
                         CommandHandler handler);

    // 向控制台输出一条消息（非命令，用于系统提示）
    void print(const std::string& msg, const glm::vec4& color = glm::vec4(1.0f));

private:
    struct Message {
        std::string text;
        glm::vec4 color;
    };

    struct CommandEntry {
        std::string name;
        std::string usage;
        CommandHandler handler;
    };

    UIRenderer* ui_ = nullptr;
    bool open_ = false;

    // 当前输入行
    std::string inputLine_;
    int cursorPos_ = 0;

    // 输出历史（最新在末尾）
    std::vector<Message> messages_;
    static constexpr int MAX_MESSAGES = 50;
    static constexpr int VISIBLE_LINES = 10;

    // 命令历史（用于上下箭头翻阅）
    std::vector<std::string> commandHistory_;
    int historyIndex_ = -1;  // -1 表示当前输入行
    std::string savedInput_;  // 翻阅历史时保存的当前输入
    static constexpr int MAX_HISTORY = 50;

    // 已注册的命令
    std::vector<CommandEntry> commands_;

    // 执行命令
    void executeCommand(const std::string& line);

    // 解析命令行为 tokens
    static std::vector<std::string> tokenize(const std::string& line);
};
