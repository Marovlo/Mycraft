#pragma once

#include <GLFW/glfw3.h>
#include <array>
#include <vector>
#include <string>

class InputManager {
public:
    void init(GLFWwindow* window);

    // Per-frame update (call before game logic)
    void update();

    // Post-frame update (call after game logic consumes mouse delta)
    void postUpdate();

    // Key state
    bool isKeyDown(int key) const;
    bool isKeyPressed(int key) const;   // Just pressed this frame
    bool isKeyReleased(int key) const;  // Just released this frame

    // Mouse state
    bool isMouseButtonDown(int button) const;
    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonReleased(int button) const;

    // Mouse position & delta
    double getMouseX() const { return mouseX_; }
    double getMouseY() const { return mouseY_; }
    double getMouseDeltaX() const { return mouseDeltaX_; }
    double getMouseDeltaY() const { return mouseDeltaY_; }
    double getScrollDelta() const { return scrollDelta_; }

    // Cursor lock
    void setCursorLocked(bool locked);
    bool isCursorLocked() const { return cursorLocked_; }
    void toggleCursorLock();

    // Text input (character callback)
    // 启用后，GLFW charCallback 会将输入的字符追加到缓冲区
    void enableTextInput(bool enable);
    bool isTextInputEnabled() const { return textInputEnabled_; }
    // 获取并清空本帧输入的字符
    std::string consumeTextInput();
    // 本帧是否有退格键按下
    bool hasBackspace() const { return backspacePressed_; }
    // 本帧是否有回车键按下
    bool hasEnter() const { return enterPressed_; }
    // 本帧是否有上/下箭头按下（用于命令历史）
    bool hasArrowUp() const { return arrowUpPressed_; }
    bool hasArrowDown() const { return arrowDownPressed_; }

private:
    GLFWwindow* window_ = nullptr;

    // Keyboard
    std::array<bool, GLFW_KEY_LAST + 1> currentKeys_ = {};
    std::array<bool, GLFW_KEY_LAST + 1> previousKeys_ = {};

    // Mouse buttons
    std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> currentMouseButtons_ = {};
    std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> previousMouseButtons_ = {};

    // Mouse position
    double mouseX_ = 0, mouseY_ = 0;
    double lastMouseX_ = 0, lastMouseY_ = 0;
    double mouseDeltaX_ = 0, mouseDeltaY_ = 0;
    double scrollDelta_ = 0;
    bool firstMouse_ = true;
    bool cursorLocked_ = true;

    // Text input state
    bool textInputEnabled_ = false;
    std::string textInputBuffer_;  // 本帧累积的字符输入
    bool backspacePressed_ = false;
    bool enterPressed_ = false;
    bool arrowUpPressed_ = false;
    bool arrowDownPressed_ = false;

    // GLFW callbacks (static, forward to instance)
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* w, unsigned int codepoint);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset);
};
