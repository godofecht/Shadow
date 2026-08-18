// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include <SDL2/SDL.h>
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

// Key codes matching SDL scancodes
enum KeyCode {
    KEY_NONE = -1,
    KEY_UP = SDL_SCANCODE_UP,
    KEY_DOWN = SDL_SCANCODE_DOWN,
    KEY_LEFT = SDL_SCANCODE_LEFT,
    KEY_RIGHT = SDL_SCANCODE_RIGHT,
    KEY_W = SDL_SCANCODE_W,
    KEY_S = SDL_SCANCODE_S,
    KEY_A = SDL_SCANCODE_A,
    KEY_D = SDL_SCANCODE_D,
    KEY_SPACE = SDL_SCANCODE_SPACE,
    KEY_ENTER = SDL_SCANCODE_RETURN,
    KEY_ESCAPE = SDL_SCANCODE_ESCAPE,
    KEY_R = SDL_SCANCODE_R,
    KEY_P = SDL_SCANCODE_P,
    KEY_TAB = SDL_SCANCODE_TAB,
    KEY_0 = SDL_SCANCODE_0,
    KEY_1 = SDL_SCANCODE_1,
    KEY_2 = SDL_SCANCODE_2,
    KEY_3 = SDL_SCANCODE_3,
    KEY_4 = SDL_SCANCODE_4,
    KEY_5 = SDL_SCANCODE_5,
    KEY_6 = SDL_SCANCODE_6,
    KEY_7 = SDL_SCANCODE_7,
    KEY_8 = SDL_SCANCODE_8,
    KEY_9 = SDL_SCANCODE_9,
};

// Mouse buttons
enum MouseButton {
    MOUSE_LEFT = SDL_BUTTON_LEFT,
    MOUSE_RIGHT = SDL_BUTTON_RIGHT,
    MOUSE_MIDDLE = SDL_BUTTON_MIDDLE,
};

// Input binding - represents a bound action
class InputBinding {
public:
    using Callback = std::function<void()>;
    
    InputBinding& onPress(Callback cb) {
        pressCallback = std::move(cb);
        return *this;
    }
    
    InputBinding& onHold(Callback cb) {
        holdCallback = std::move(cb);
        return *this;
    }
    
    InputBinding& onRelease(Callback cb) {
        releaseCallback = std::move(cb);
        return *this;
    }
    
    // Internal call methods
    void triggerPress() { if (pressCallback) pressCallback(); }
    void triggerHold() { if (holdCallback) holdCallback(); }
    void triggerRelease() { if (releaseCallback) releaseCallback(); }
    
private:
    Callback pressCallback;
    Callback holdCallback;
    Callback releaseCallback;
};

// 2D Game Input Manager - handles all input with bindable actions
class InputManager2D {
public:
    InputManager2D();
    
    // Bind actions to keys - fluent API
    InputBinding& bind(KeyCode key);
    InputBinding& bindMouse(MouseButton button);
    
    // Check input state directly
    bool isKeyPressed(KeyCode key) const;
    bool isKeyHeld(KeyCode key) const;
    bool wasKeyJustPressed(KeyCode key) const;
    
    // Mouse state
    bool isMousePressed(MouseButton button) const;
    void getMousePosition(int& x, int& y) const;
    
    // Update - call once per frame to process input
    void update();
    
    // Public state for direct access in lambdas
    int mouseX = 0, mouseY = 0;
    
private:
    std::unordered_map<int, std::shared_ptr<InputBinding>> keyBindings;
    std::unordered_map<int, std::shared_ptr<InputBinding>> mouseBindings;
    
    bool currentKeys[SDL_NUM_SCANCODES];
    bool previousKeys[SDL_NUM_SCANCODES];
    // Sized 9 (indices 1..8) because SDL button numbers start at 1
    // (SDL_BUTTON_LEFT=1 ... SDL_BUTTON_X2=5) and update() samples 1..8.
    // Index 0 is unused. (Was sized 8 - an out-of-bounds write/read that
    // UBSan caught in the headless example runs.)
    bool currentMouse[9];
    bool previousMouse[9];
};
