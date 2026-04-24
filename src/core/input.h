#pragma once

#include <GLFW/glfw3.h>
#include <array>

class InputManager {
public:
    void init(GLFWwindow* window);

    // Per-frame update (call before game logic)
    void update();

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

    // Cursor lock
    void setCursorLocked(bool locked);
    bool isCursorLocked() const { return cursorLocked_; }
    void toggleCursorLock();

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
    bool firstMouse_ = true;
    bool cursorLocked_ = true;

    // GLFW callbacks (static, forward to instance)
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double xpos, double ypos);
};
