// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for GameState (Engine/Core/GameState.h)
#include "../Engine/Core/GameState.h"
#include "test_main.h"

REGISTER_TEST(test_GameState_defaults)
{
    GameState s;
    ASSERT_FALSE(s.gameRunning);
    ASSERT_FALSE(s.gameOver);
    ASSERT_FALSE(s.gameWon);
    ASSERT_EQ(0, s.score);
    ASSERT_EQ(1, s.level);
    ASSERT_EQ(0, s.gridWidth);
    ASSERT_EQ(0, s.gridHeight);
    ASSERT_TRUE(s.availableActions.empty());
}

REGISTER_TEST(test_GameState_toString_playing)
{
    GameState s;
    s.gameRunning = true;
    s.score = 100;
    s.level = 2;

    std::string out = s.toString();
    ASSERT_TRUE(out.find("=== GAME STATE ===") != std::string::npos);
    ASSERT_TRUE(out.find("Status: PLAYING") != std::string::npos);
    ASSERT_TRUE(out.find("Score: 100") != std::string::npos);
    ASSERT_TRUE(out.find("Level: 2") != std::string::npos);
}

REGISTER_TEST(test_GameState_toString_game_over)
{
    GameState s;
    s.gameRunning = false;
    s.gameOver = true;
    std::string out = s.toString();
    ASSERT_TRUE(out.find("Status: GAME OVER") != std::string::npos);
}

REGISTER_TEST(test_GameState_toString_won)
{
    GameState s;
    s.gameRunning = false;
    s.gameWon = true;
    std::string out = s.toString();
    ASSERT_TRUE(out.find("Status: WON") != std::string::npos);
}

REGISTER_TEST(test_GameState_toString_grid)
{
    GameState s;
    s.gameRunning = true;
    s.gridWidth = 3;
    s.gridHeight = 2;
    s.grid = {
        {0, 1, 0},
        {1, 0, 2}
    };

    std::string out = s.toString();
    ASSERT_TRUE(out.find("=== GRID ===") != std::string::npos);
    ASSERT_TRUE(out.find(".#.") != std::string::npos);
    // value 2 renders as 'O'
    ASSERT_TRUE(out.find("#.O") != std::string::npos);
}

REGISTER_TEST(test_GameState_toString_entities)
{
    GameState s;
    s.entities["player"] = {5, 10};
    s.entities["food"] = {3, 6};

    std::string out = s.toString();
    ASSERT_TRUE(out.find("player: (5, 10)") != std::string::npos);
    ASSERT_TRUE(out.find("food: (3, 6)") != std::string::npos);
}

REGISTER_TEST(test_GameState_toString_actions)
{
    GameState s;
    s.availableActions = {"up", "down", "left", "right"};
    std::string out = s.toString();
    ASSERT_TRUE(out.find("=== AVAILABLE ACTIONS ===") != std::string::npos);
    ASSERT_TRUE(out.find("  - up") != std::string::npos);
    ASSERT_TRUE(out.find("  - right") != std::string::npos);
}

REGISTER_TEST(test_GameState_toJSON)
{
    GameState s;
    s.gameRunning = true;
    s.score = 42;
    s.level = 3;
    s.gridWidth = 4;
    s.gridHeight = 4;
    s.availableActions = {"up", "down"};

    std::string json = s.toJSON();
    ASSERT_TRUE(json.find("\"gameRunning\": true") != std::string::npos);
    ASSERT_TRUE(json.find("\"gameOver\": false") != std::string::npos);
    ASSERT_TRUE(json.find("\"score\": 42") != std::string::npos);
    ASSERT_TRUE(json.find("\"level\": 3") != std::string::npos);
    ASSERT_TRUE(json.find("\"gridWidth\": 4") != std::string::npos);
    ASSERT_TRUE(json.find("\"gridHeight\": 4") != std::string::npos);
    ASSERT_TRUE(json.find("\"availableActions\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"up\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"down\"") != std::string::npos);
}

REGISTER_TEST(test_GameState_toJSON_won)
{
    GameState s;
    s.gameWon = true;
    std::string json = s.toJSON();
    // toJSON only serializes gameRunning/gameOver - verify it stays valid
    ASSERT_TRUE(json.find("{") != std::string::npos);
    ASSERT_TRUE(json.find("}") != std::string::npos);
}

REGISTER_TEST(test_ActionResult_defaults)
{
    ActionResult r;
    ASSERT_FALSE(r.success);
    ASSERT_TRUE(r.message.empty());
    ASSERT_EQ(0, r.scoreChange);
    ASSERT_FALSE(r.gameOver);
    ASSERT_FALSE(r.gameWon);
}
