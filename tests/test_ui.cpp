// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for UI components (logic only - rendering requires a live renderer)
#include "../Engine/Core/UI.h"
#include "test_main.h"

REGISTER_TEST(test_TextDisplay_default)
{
    TextDisplay td(10, 20);
    ASSERT_TRUE(td.getText().empty());
    td.setText("Hello");
    ASSERT_EQ("Hello", td.getText());
}

REGISTER_TEST(test_TextDisplay_set_text)
{
    TextDisplay td(0, 0, "Start");
    ASSERT_EQ("Start", td.getText());
    td.setText("Updated");
    ASSERT_EQ("Updated", td.getText());
}

REGISTER_TEST(test_TextDisplay_stream_append)
{
    TextDisplay td(0, 0, "Score: ");
    td << 100;
    ASSERT_EQ("Score: 100", td.getText());

    td << " points";
    ASSERT_EQ("Score: 100 points", td.getText());
}

REGISTER_TEST(test_Button_click_logic)
{
    Button btn(10, 10, 50, 30, "Go");

    // Click inside
    btn.update(30, 25, true);
    ASSERT_TRUE(btn.isClicked());

    // Not pressed = not clicked (even if hovering)
    btn.update(30, 25, false);
    ASSERT_FALSE(btn.isClicked());

    // Click outside bounds
    btn.update(200, 200, true);
    ASSERT_FALSE(btn.isClicked());
}

REGISTER_TEST(test_Button_edge_hover)
{
    Button btn(10, 10, 50, 30, "Go");

    // Left/top edges inclusive
    btn.update(10, 10, true);
    ASSERT_TRUE(btn.isClicked());

    // Right/bottom edges inclusive
    btn.update(60, 40, true);
    ASSERT_TRUE(btn.isClicked());

    // Just outside
    btn.update(61, 40, true);
    ASSERT_FALSE(btn.isClicked());
}

REGISTER_TEST(test_Button_setters)
{
    Button btn(0, 0, 10, 10, "Old");
    btn.setLabel("New");
    btn.setPosition(5, 5);
    btn.setSize(20, 20);

    // Click at new position
    btn.update(15, 15, true);
    ASSERT_TRUE(btn.isClicked());
}

REGISTER_TEST(test_GameStats_defaults)
{
    GameStats gs(0, 0);
    ASSERT_EQ(0, gs.getStat("nonexistent"));
}

REGISTER_TEST(test_GameStats_set_get)
{
    GameStats gs(0, 0);
    gs.setStat("score", 100);
    ASSERT_EQ(100, gs.getStat("score"));

    gs.setStat("score", 50);  // Overwrite
    ASSERT_EQ(50, gs.getStat("score"));
}

REGISTER_TEST(test_GameStats_add)
{
    GameStats gs(0, 0);
    gs.addStat("score", 10);
    gs.addStat("score", 5);
    ASSERT_EQ(15, gs.getStat("score"));

    gs.addStat("lives", -1);
    ASSERT_EQ(-1, gs.getStat("lives"));
}

REGISTER_TEST(test_ExplanationOverlay_lines)
{
    ExplanationOverlay overlay(0, 0, 200);
    overlay.addLine("Line 1");
    overlay.addLine("Line 2");
    overlay.clear();  // No crash
    overlay.addLine("After clear");
}
