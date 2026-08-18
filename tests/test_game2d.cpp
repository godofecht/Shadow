// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Tests for Game2D LLM interface (headless - no window created)
#include "../Engine/Core/Game2D.h"
#include "test_main.h"
#include <algorithm>

// Minimal concrete Game2D for testing the LLM interface
class TestLLMGame : public Game2D {
public:
    TestLLMGame() : Game2D("Test Game", 400, 300, 20) {}

    void initGame() override {
        createGrid(10, 10, 20);
        player = createEntity<GridEntity>(getGrid(), 5, 5);

        registerAction("move_up", [this]() -> ActionResult {
            if (player && player->tryMove(0, -1))
                return {true, "Moved up"};
            return {false, "Blocked"};
        });

        registerAction("move_left", [this]() -> ActionResult {
            if (player && player->tryMove(-1, 0))
                return {true, "Moved left"};
            return {false, "Blocked"};
        });
    }

    // Public accessors for testing protected Game2D members
    Grid* testGrid() { return getGrid(); }
    bool  isRunning() { return isGameRunning(); }

    std::shared_ptr<GridEntity> player;
};

REGISTER_TEST(test_Game2D_start_and_state)
{
    TestLLMGame game;
    game.reset();  // Public LLMPlayable reset -> startGame()

    ASSERT_TRUE(game.isRunning());

    GameState state = game.getState();
    ASSERT_TRUE(state.gameRunning);
    ASSERT_FALSE(state.gameOver);
    ASSERT_EQ(10, state.gridWidth);
    ASSERT_EQ(10, state.gridHeight);
    ASSERT_EQ(10, static_cast<int>(state.grid.size()));
    ASSERT_EQ(10, static_cast<int>(state.grid[0].size()));
}

REGISTER_TEST(test_Game2D_available_actions)
{
    TestLLMGame game;
    game.reset();

    auto actions = game.getAvailableActions();
    ASSERT_EQ(2, static_cast<int>(actions.size()));
    ASSERT_TRUE(std::find(actions.begin(), actions.end(), "move_up") != actions.end());
    ASSERT_TRUE(std::find(actions.begin(), actions.end(), "move_left") != actions.end());
}

REGISTER_TEST(test_Game2D_execute_action)
{
    TestLLMGame game;
    game.reset();

    ActionResult r = game.executeAction("move_up");
    ASSERT_TRUE(r.success);
    ASSERT_EQ(4, game.player->getY());

    ActionResult r2 = game.executeAction("move_left");
    ASSERT_TRUE(r2.success);
    ASSERT_EQ(4, game.player->getX());
}

REGISTER_TEST(test_Game2D_unknown_action)
{
    TestLLMGame game;
    game.reset();

    ActionResult r = game.executeAction("fly");
    ASSERT_FALSE(r.success);
    ASSERT_TRUE(r.message.find("Unknown action") != std::string::npos);
}

REGISTER_TEST(test_Game2D_blocked_action)
{
    TestLLMGame game;
    game.reset();

    // Put a solid cell above the player
    game.testGrid()->cell(5, 4).isSolid = true;
    ActionResult r = game.executeAction("move_up");
    ASSERT_FALSE(r.success);
    ASSERT_TRUE(r.message.find("Blocked") != std::string::npos);
    ASSERT_EQ(5, game.player->getY());
}

REGISTER_TEST(test_Game2D_game_over_flags)
{
    TestLLMGame game;
    game.reset();

    ASSERT_FALSE(game.isGameOver());
    ASSERT_FALSE(game.isGameWon());
    ASSERT_TRUE(game.isRunning());
}

REGISTER_TEST(test_Game2D_register_duplicate_action)
{
    TestLLMGame game;
    game.reset();

    // registerAction on the same name should not duplicate the name
    game.registerAction("move_up", []() -> ActionResult { return {true, ""}; });
    auto actions = game.getAvailableActions();
    ASSERT_EQ(2, static_cast<int>(actions.size()));
}
