#include "input.h"

void InputManager::init(GLFWwindow* window) {
    window_ = window;
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);

    setCursorLocked(true);
}

void InputManager::update() {
    previousKeys_ = currentKeys_;
    previousMouseButtons_ = currentMouseButtons_;

    // Mouse delta is computed in callback; reset after consumption
    // (delta is accumulated between frames via callback)
}

bool InputManager::isKeyDown(int key) const {
    if (key < 0 || key > GLFW_KEY_LAST) return false;
    return currentKeys_[key];
}

bool InputManager::isKeyPressed(int key) const {
    if (key < 0 || key > GLFW_KEY_LAST) return false;
    return currentKeys_[key] && !previousKeys_[key];
}

bool InputManager::isKeyReleased(int key) const {
    if (key < 0 || key > GLFW_KEY_LAST) return false;
    return !currentKeys_[key] && previousKeys_[key];
}

bool InputManager::isMouseButtonDown(int button) const {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return currentMouseButtons_[button];
}

bool InputManager::isMouseButtonPressed(int button) const {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return currentMouseButtons_[button] && !previousMouseButtons_[button];
}

bool InputManager::isMouseButtonReleased(int button) const {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return !currentMouseButtons_[button] && previousMouseButtons_[button];
}

void InputManager::setCursorLocked(bool locked) {
    cursorLocked_ = locked;
    glfwSetInputMode(window_, GLFW_CURSOR,
        locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (locked) {
        firstMouse_ = true;
    }
}

void InputManager::toggleCursorLock() {
    setCursorLocked(!cursorLocked_);
}

// ========== GLFW Callbacks ==========

void InputManager::keyCallback(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(w));
    if (key >= 0 && key <= GLFW_KEY_LAST) {
        input->currentKeys_[key] = (action != GLFW_RELEASE);
    }
}

void InputManager::mouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(w));
    if (button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST) {
        input->currentMouseButtons_[button] = (action != GLFW_RELEASE);
    }
}

void InputManager::cursorPosCallback(GLFWwindow* w, double xpos, double ypos) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(w));

    if (input->firstMouse_) {
        input->lastMouseX_ = xpos;
        input->lastMouseY_ = ypos;
        input->firstMouse_ = false;
        input->mouseDeltaX_ = 0;
        input->mouseDeltaY_ = 0;
    } else {
        input->mouseDeltaX_ = xpos - input->lastMouseX_;
        input->mouseDeltaY_ = ypos - input->lastMouseY_;
    }

    input->mouseX_ = xpos;
    input->mouseY_ = ypos;
    input->lastMouseX_ = xpos;
    input->lastMouseY_ = ypos;
}
