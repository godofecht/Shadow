// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for InputBinding and InputManager2D binding API
#include "../Engine/Core/InputManager.h"
#include "test_main.h"

REGISTER_TEST(test_InputBinding_default_callbacks)
{
    InputBinding b;
    bool fired = false;
    b.triggerPress();
    b.triggerHold();
    b.triggerRelease();
    ASSERT_FALSE(fired);  // No callbacks set = no-op
}

REGISTER_TEST(test_InputBinding_callbacks)
{
    InputBinding b;
    int pressCount = 0, holdCount = 0, releaseCount = 0;

    b.onPress([&] { pressCount++; });
    b.onHold([&] { holdCount++; });
    b.onRelease([&] { releaseCount++; });

    b.triggerPress();
    b.triggerHold();
    b.triggerRelease();

    ASSERT_EQ(1, pressCount);
    ASSERT_EQ(1, holdCount);
    ASSERT_EQ(1, releaseCount);

    // Chaining returns the same binding
    InputBinding& chained = b.onPress([&] {}).onHold([&] {});
    ASSERT_EQ(&chained, &b);
}

REGISTER_TEST(test_InputBinding_override)
{
    InputBinding b;
    int first = 0, second = 0;
    b.onPress([&] { first++; });
    b.triggerPress();
    b.onPress([&] { second++; });  // Replaces previous
    b.triggerPress();

    ASSERT_EQ(1, first);
    ASSERT_EQ(1, second);
}

REGISTER_TEST(test_InputManager2D_bind)
{
    InputManager2D input;

    InputBinding& b1 = input.bind(KEY_SPACE);
    InputBinding& b2 = input.bind(KEY_SPACE);  // Same binding returned
    ASSERT_EQ(&b1, &b2);

    InputBinding& w = input.bind(KEY_W);
    ASSERT_FALSE(&b1 == &w);
}

REGISTER_TEST(test_InputManager2D_bind_mouse)
{
    InputManager2D input;

    InputBinding& left = input.bindMouse(MOUSE_LEFT);
    InputBinding& leftAgain = input.bindMouse(MOUSE_LEFT);
    ASSERT_EQ(&left, &leftAgain);
}

REGISTER_TEST(test_InputManager2D_initial_state)
{
    // isKeyPressed/isKeyHeld read zeroed state arrays before any update
    InputManager2D input;
    ASSERT_FALSE(input.isKeyPressed(KEY_SPACE));
    ASSERT_FALSE(input.isKeyHeld(KEY_W));
    ASSERT_FALSE(input.wasKeyJustPressed(KEY_UP));
    ASSERT_FALSE(input.isMousePressed(MOUSE_LEFT));

    int mx = -1, my = -1;
    input.getMousePosition(mx, my);
    ASSERT_EQ(0, mx);
    ASSERT_EQ(0, my);
}

REGISTER_TEST(test_InputManager2D_out_of_range_keys)
{
    InputManager2D input;
    ASSERT_FALSE(input.isKeyPressed(KEY_NONE));
    ASSERT_FALSE(input.wasKeyJustPressed(KEY_NONE));
}

REGISTER_TEST(test_InputManager2D_mouse_button_bounds)
{
    // Regression: currentMouse/previousMouse were sized 8 but update() writes
    // SDL button indices 1..8 (SDL reports up to SDL_BUTTON_X2=5, the loop
    // covers 1..8) - an out-of-bounds write UBSan caught in the headless
    // example runs. update() must never touch adjacent memory, and every
    // declared MouseButton must stay readable. SDL_GetKeyboardState and
    // SDL_GetMouseState are safe without SDL_Init (they return zeroed state),
    // so this runs headless everywhere. (Note: only the declared enum values
    // are exercised - constructing invalid MouseButton values is itself UB.)
    InputManager2D input;
    input.update();  // Writes currentMouse[1..8]
    ASSERT_FALSE(input.isMousePressed(MOUSE_LEFT));
    ASSERT_FALSE(input.isMousePressed(MOUSE_RIGHT));
    ASSERT_FALSE(input.isMousePressed(MOUSE_MIDDLE));

    // Repeated updates keep every button in-bounds and clean.
    input.update();
    ASSERT_FALSE(input.isMousePressed(MOUSE_LEFT));
    ASSERT_FALSE(input.isMousePressed(MOUSE_RIGHT));
    ASSERT_FALSE(input.isMousePressed(MOUSE_MIDDLE));
}
