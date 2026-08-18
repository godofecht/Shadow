// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

#include "Engine/Core/InputManager.h"
#include <cstring>

InputManager2D::InputManager2D() : mouseX(0), mouseY(0) {
    std::memset(currentKeys, 0, sizeof(currentKeys));
    std::memset(previousKeys, 0, sizeof(previousKeys));
    std::memset(currentMouse, 0, sizeof(currentMouse));
    std::memset(previousMouse, 0, sizeof(previousMouse));
}

InputBinding& InputManager2D::bind(KeyCode key) {
    if (keyBindings.find(key) == keyBindings.end()) {
        keyBindings[key] = std::make_shared<InputBinding>();
    }
    return *keyBindings[key];
}

InputBinding& InputManager2D::bindMouse(MouseButton button) {
    if (mouseBindings.find(button) == mouseBindings.end()) {
        mouseBindings[button] = std::make_shared<InputBinding>();
    }
    return *mouseBindings[button];
}

bool InputManager2D::isKeyPressed(KeyCode key) const {
    const int keyIndex = static_cast<int>(key);
    if (keyIndex < 0 || keyIndex >= SDL_NUM_SCANCODES) return false;
    return currentKeys[keyIndex];
}

bool InputManager2D::isKeyHeld(KeyCode key) const {
    return isKeyPressed(key);
}

bool InputManager2D::wasKeyJustPressed(KeyCode key) const {
    const int keyIndex = static_cast<int>(key);
    if (keyIndex < 0 || keyIndex >= SDL_NUM_SCANCODES) return false;
    return currentKeys[keyIndex] && !previousKeys[keyIndex];
}

bool InputManager2D::isMousePressed(MouseButton button) const {
    const int buttonIndex = static_cast<int>(button);
    if (buttonIndex < 1 || buttonIndex > 8) return false;
    return currentMouse[buttonIndex];
}

void InputManager2D::getMousePosition(int& x, int& y) const {
    x = mouseX;
    y = mouseY;
}

void InputManager2D::update() {
    // Save previous state
    std::memcpy(previousKeys, currentKeys, sizeof(currentKeys));
    std::memcpy(previousMouse, currentMouse, sizeof(currentMouse));
    
    // Get current keyboard state
    const Uint8* keyState = SDL_GetKeyboardState(nullptr);
    std::memcpy(currentKeys, keyState, sizeof(currentKeys));
    
    // Get current mouse state
    Uint32 buttons = SDL_GetMouseState(&mouseX, &mouseY);
    for (int i = 1; i <= 8; i++) {
        currentMouse[i] = (buttons & SDL_BUTTON(i)) != 0;
    }
    
    // Trigger callbacks
    for (const auto& [key, binding] : keyBindings) {
        const int keyIndex = key;
        if (wasKeyJustPressed(static_cast<KeyCode>(keyIndex))) {
            binding->triggerPress();
        }
        if (isKeyHeld(static_cast<KeyCode>(keyIndex))) {
            binding->triggerHold();
        }
    }
    
    // Trigger mouse callbacks
    for (const auto& [button, binding] : mouseBindings) {
        if (currentMouse[button] && !previousMouse[button]) {
            binding->triggerPress();
        }
        if (currentMouse[button]) {
            binding->triggerHold();
        }
    }
}
