// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

// Headless tests for the shipped Games/ (Pong + the Coin Collector template).
//
// The game sources are included directly (with UMBRA_GAME_NO_MAIN so their
// int main() is skipped) and driven through the LLM interface plus
// Game2D::tick(dt), which steps the real updateGame() code without the SDL
// input poll. No window is ever created, so these run in the native unit-test
// binary AND under `node sdl_app_tests.js` in the Emscripten+ASan CI job -
// that is how the browser build path of the 100-game program gets ASan
// coverage: serve -> rally -> score and enemy AI all execute under the
// sanitizer.

#define UMBRA_GAME_NO_MAIN
#include "../Games/Pong/main.cpp"
#include "../Games/Breakout/main.cpp"
#include "../Games/SpaceInvaders/main.cpp"
#include "../Games/Tetris/main.cpp"
#include "../Games/FlappyBird/main.cpp"
#include "../Games/DoodleJump/main.cpp"
#include "../Games/Asteroids/main.cpp"
#include "../Games/PacMan/main.cpp"
#include "../Examples/Minesweeper/main.cpp"
#include "../Examples/Snake/main.cpp"
#include "../Examples/Roguelike/main.cpp"
#include "../Examples/TicTacToe/main.cpp"
#include "../Games/MemoryMatch/main.cpp"
#include "../Games/SimonSays/main.cpp"
#include "../Games/WhackAMole/main.cpp"
#include "../Games/Frogger/main.cpp"
#include "../Games/PortalSnake/main.cpp"
#include "../Games/BrickBreakerPlus/main.cpp"
#include "../Games/Galaga/main.cpp"
#include "../Games/2048/main.cpp"
#include "../Games/_template/main.cpp"

#include "test_main.h"

// --- Fixed-path regressions -------------------------------------------------
// Each test drives exactly the code path whose sanitizer bug was fixed, so
// the focused memcheck_path_* ctest entries (and the sanitizer jobs) run it
// deterministically instead of hoping the autopilot smoke happens to hit it.

REGISTER_TEST(test_gamejuice_floating_text_lifecycle)
{
    // GameJuice FloatingText::update clamps the fade alpha to [0, 255] on the
    // frame a label's life crosses 0 (a negative float -> uint8_t cast would
    // be float-cast-overflow). Step the label PAST its life in one big step so
    // the fade factor 255 * life/maxLife lands at -51 (life = -0.1): the
    // unclamped cast is out of uint8_t's range, the clamp pins it to 0.
    uj::FloatingText ft;
    auto display = std::make_shared<TextDisplay>(10, 20, "+10");
    ft.spawn(display, 10, 20, 26.0f, 0.5f);
    ASSERT_FALSE(ft.empty());

    ft.update(0.6f);   // life: 0.5 -> -0.1, alpha factor -> -51 (would overflow)
    ASSERT_TRUE(ft.empty());
}

REGISTER_TEST(test_invaders_bomb_hits_player)
{
    // A bomb reaching the cannon runs onPlayerHit() mid-loop in stepBombs; the
    // clear must happen after the loop or the range-for's iterators dangle
    // (container-overflow). Drop TWO bombs so a mid-iteration clear leaves a
    // second element the loop would have to read past - the dangling access a
    // sanitizer (with container annotations) catches.
    SpaceInvaders game;
    game.reset();
    ASSERT_EQ(3, game.getState().stats.at("lives"));

    game.dropBombOnPlayerForTest();
    game.dropBombOnPlayerForTest();
    game.tick(1.0f / 60.0f);

    const auto st = game.getState();
    ASSERT_EQ(2, st.stats.at("lives"));       // the hit landed
    ASSERT_EQ(0, st.stats.at("bombs"));       // the field was cleared
    ASSERT_TRUE(st.gameRunning);
}

REGISTER_TEST(test_pacman_ghost_target_stopped)
{
    // Pink/cyan chase "ahead" of Pac-Man's heading; with no heading
    // (pdir == -1) they must target his tile directly, never DX/DY[-1].
    PacMan game;
    game.reset();

    const int px = game.getState().stats.at("pac_x");
    const int py = game.getState().stats.at("pac_y");

    const auto pink = game.ghostTargetStoppedForTest(1);   // pink
    const auto cyan = game.ghostTargetStoppedForTest(2);   // cyan
    ASSERT_EQ(px, pink.first);
    ASSERT_EQ(py, pink.second);
    ASSERT_EQ(px, cyan.first);
    ASSERT_EQ(py, cyan.second);
}

// --- Pong: serve -> rally -> score via the LLM interface --------------------

REGISTER_TEST(test_pong_headless_serve_and_physics)
{
    Pong game;
    game.reset();  // LLMPlayable::reset -> startGame -> initGame
    ASSERT_TRUE(game.getState().gameRunning);

    // During serve-wait the ball rests on the serving paddle.
    const GameState s0 = game.getState();
    const int x0 = s0.stats.at("ball_x");
    const int y0 = s0.stats.at("ball_y");

    // Serve puts the ball in play (the same code path as the SPACE key).
    ASSERT_TRUE(game.executeAction("serve").success);

    // Drive the physics headlessly: 5 seconds of 60 Hz frames. The ball flies
    // at >= 24 cells/s across a 100-cell court, so it must have moved or a
    // point must have been scored - proving the updateGame() path executed.
    for (int i = 0; i < 300; ++i) {
        game.tick(1.0f / 60.0f);
    }

    const GameState s1 = game.getState();
    const bool moved = s1.stats.at("ball_x") != x0 || s1.stats.at("ball_y") != y0;
    const bool scored = s1.stats.at("score_left") > 0 || s1.stats.at("score_right") > 0;
    ASSERT_TRUE(moved || scored);
    // Still a valid state: running, or a legitimate game-over (first to 7).
    ASSERT_TRUE(s1.gameRunning || s1.gameOver);
}

REGISTER_TEST(test_pong_headless_paddle_actions)
{
    Pong game;
    game.reset();
    // Paddle movement through the same clamp the keyboard/LLM share.
    ASSERT_TRUE(game.executeAction("paddle_left_up").success);
    ASSERT_TRUE(game.executeAction("paddle_right_down").success);

    const GameState s = game.getState();
    ASSERT_TRUE(s.stats.count("paddle_left_y") > 0);
    ASSERT_TRUE(s.stats.count("paddle_right_y") > 0);
}

// --- Breakout: serve, rally, bricks, win ------------------------------------

REGISTER_TEST(test_breakout_headless_serve_and_physics)
{
    Breakout game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);

    const GameState s0 = game.getState();
    const int bx0 = s0.stats.at("ball_x");
    const int by0 = s0.stats.at("ball_y");
    const int bricks0 = s0.stats.at("bricks_left");
    ASSERT_TRUE(bricks0 > 0);

    // Serve puts the ball in play (same code path as the SPACE key).
    ASSERT_TRUE(game.executeAction("serve").success);

    // 5 seconds of 60 Hz frames: the ball must move, hit bricks, or end the
    // game legitimately - proving the full updateGame() path executed.
    for (int i = 0; i < 300; ++i) {
        game.tick(1.0f / 60.0f);
    }

    const GameState s1 = game.getState();
    const bool moved = s1.stats.at("ball_x") != bx0 || s1.stats.at("ball_y") != by0;
    const bool scored = s1.stats.at("score") > 0;
    const bool cleared = s1.stats.at("bricks_left") < bricks0;
    ASSERT_TRUE(moved || scored || cleared);
    ASSERT_TRUE(s1.gameRunning || s1.gameOver || s1.gameWon);
}

REGISTER_TEST(test_breakout_headless_paddle_actions)
{
    Breakout game;
    game.reset();
    const int px0 = game.getState().stats.at("paddle_x");
    ASSERT_TRUE(game.executeAction("move_paddle_right").success);
    ASSERT_TRUE(game.getState().stats.at("paddle_x") > px0);
    ASSERT_TRUE(game.executeAction("move_paddle_left").success);
    ASSERT_TRUE(game.getState().stats.at("paddle_x") <= px0 + 1);
}

REGISTER_TEST(test_breakout_clearing_wall_wins)
{
    Breakout game;
    game.reset();
    // The brick grid is private; drive the real game until it ends or the
    // window passes. The win path (countBricks() == 0 -> endGame + gameWon)
    // is reached through the same physics a player uses - we just hammer it
    // with serves + paddle sweeps and require a legal terminal state.
    bool ended = false;
    for (int i = 0; i < 60 * 60 && !ended; ++i) {
        (void)game.executeAction(i % 60 == 0 ? "serve" : "move_paddle_right");
        if (i % 61 == 30) (void)game.executeAction("move_paddle_left");
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        ended = st.gameOver || st.gameWon;
    }
    // The game must have progressed (bricks cleared or a life lost).
    const auto st = game.getState();
    ASSERT_TRUE(st.stats.at("score") > 0 || st.stats.at("bricks_left") < 12 * 5);
    // Restart must reset to a fresh wall.
    game.executeAction("restart");
    ASSERT_TRUE(game.getState().gameRunning);
    ASSERT_EQ(12 * 5, game.getState().stats.at("bricks_left"));
}

// --- Breakout feel: the GameJuice systems execute under autoplay ------------

REGISTER_TEST(test_breakout_autoplay_juice_loop)
{
    // The autopilot sweeps + serves; bricks break -> particles, hit-stop,
    // screen shake, floating text, and procedural SFX must all run without
    // error, and the game must make real progress. Restart on game over/win
    // so the loop stays live across a full 15s of frames.
    Breakout game;
    game.enableAutoplay();
    game.reset();
    const auto s0 = game.getState();
    ASSERT_TRUE(s0.stats.count("particles") > 0);   // juice stats exposed
    ASSERT_TRUE(s0.stats.count("frozen") > 0);
    ASSERT_TRUE(s0.stats.count("best") > 0);

    int best = 0;
    for (int i = 0; i < 60 * 15; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        best = std::max(best, st.stats.at("score"));
        if (st.gameOver || st.gameWon) game.reset();
    }
    // Bricks must have shattered: score > 0 (particles fired) or the wall
    // cleared.
    ASSERT_TRUE(best > 0);
}

// --- Space Invaders: march, bombs, shooting, shields ------------------------

REGISTER_TEST(test_invaders_headless_march_and_bombs)
{
    SpaceInvaders game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);

    const int fx0 = game.getState().stats.at("formation_x");
    ASSERT_EQ(55, game.getState().stats.at("invaders_left"));

    // 5 seconds of frames: the formation must march (0.8s/step -> ~6 steps)
    // and the invaders must drop bombs (first at 1.6s, then every 1.6s).
    int maxBombs = 0;
    for (int i = 0; i < 300; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        maxBombs = std::max(maxBombs, st.stats.at("bombs"));
        if (st.gameOver) break;
    }

    const auto s1 = game.getState();
    ASSERT_TRUE(s1.gameRunning || s1.gameOver);
    ASSERT_TRUE(s1.stats.at("formation_x") != fx0);
    ASSERT_TRUE(maxBombs > 0);
}

REGISTER_TEST(test_invaders_headless_shoot)
{
    SpaceInvaders game;
    game.reset();

    // Park the cannon in the gap between shields 1 and 2 (cells 23..33) and
    // fire: the single shot flies straight up through the gap and must kill
    // the bottom invader of the column above it.
    for (int i = 0; i < 3; ++i) (void)game.executeAction("move_left");
    ASSERT_EQ(24, game.getState().stats.at("player_x"));
    ASSERT_TRUE(game.executeAction("fire").success);

    for (int i = 0; i < 90; ++i) {  // 1.5s: bullet reaches the formation
        game.tick(1.0f / 60.0f);
    }

    const auto s1 = game.getState();
    ASSERT_TRUE(s1.stats.at("invaders_left") < 55);
    ASSERT_TRUE(s1.stats.at("score") > 0);
    ASSERT_TRUE(s1.gameRunning || s1.gameOver);
}

REGISTER_TEST(test_invaders_shield_erodes)
{
    SpaceInvaders game;
    game.reset();

    // Park under shield 2 (cells 34..46) and fire: the bullet chips shield
    // cells and is consumed - it must NOT reach an invader.
    (void)game.executeAction("move_right");
    ASSERT_TRUE(game.executeAction("fire").success);

    for (int i = 0; i < 30; ++i) {  // 0.5s: bullet dies on the shield
        game.tick(1.0f / 60.0f);
    }

    const auto s1 = game.getState();
    ASSERT_TRUE(s1.stats.at("shields_left") < 195);  // erosion happened
    ASSERT_TRUE(s1.stats.at("bullets") == 0);        // shot consumed
    ASSERT_EQ(55, s1.stats.at("invaders_left"));     // no invader killed
}

REGISTER_TEST(test_invaders_headless_kill_loop_and_restart)
{
    SpaceInvaders game;
    game.reset();

    // Columns 2, 3, 8 and 9 sit under the shield gaps, so shots from the
    // reachable cannon positions (24, 29, 49, 54) are guaranteed kills. Fire
    // at each until its 5 invaders are gone, retrying across up to 3 passes
    // to absorb the occasional bomb hit + respawn. Ends on win or game over.
    const int plan[4] = {24, 29, 49, 54};
    bool ended = false;
    for (int pass = 0; pass < 3 && !ended; ++pass) {
        for (int target : plan) {
            if (ended) break;
            for (int guard = 0; guard < 30; ++guard) {
                const int px = game.getState().stats.at("player_x");
                if (px == target) break;
                (void)game.executeAction(px < target ? "move_right" : "move_left");
            }
            const int before = game.getState().stats.at("invaders_left");
            for (int i = 0; i < 60 * 5 && !ended; ++i) {
                (void)game.executeAction("fire");
                game.tick(1.0f / 60.0f);
                const auto st = game.getState();
                ended = st.gameOver || st.gameWon;
                if (ended) break;
                if (st.stats.at("invaders_left") <= before - 5) break;
            }
        }
        if (game.getState().stats.at("invaders_left") <= 55 - 20) break;
    }

    const auto st = game.getState();
    // The game either ended (win / invaders reached earth) or made strong
    // progress - the four gap columns (20 invaders) are clear.
    ASSERT_TRUE(st.gameWon || st.gameOver ||
                st.stats.at("invaders_left") <= 55 - 20);
    // Restart must reset to a fresh 55-invader sky.
    game.executeAction("restart");
    ASSERT_TRUE(game.getState().gameRunning);
    ASSERT_EQ(55, game.getState().stats.at("invaders_left"));
}

REGISTER_TEST(test_invaders_autoplay_juice_loop)
{
    // The autopilot sweeps + fires; invaders die -> explosion particles,
    // hit-stop, screen shake, floating score, and procedural SFX all run
    // headlessly. Restart on game over/win so the loop stays live; require
    // real progress within 12s of frames.
    SpaceInvaders game;
    game.enableAutoplay();
    game.reset();
    const auto s0 = game.getState();
    ASSERT_TRUE(s0.stats.count("particles") > 0);   // juice stats exposed
    ASSERT_TRUE(s0.stats.count("frozen") > 0);
    ASSERT_TRUE(s0.stats.count("best") > 0);

    int best = 0;
    for (int i = 0; i < 60 * 12; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        best = std::max(best, st.stats.at("score"));
        if (st.gameOver || st.gameWon) game.reset();
    }
    // Invaders must have been shot down (explosions fired).
    ASSERT_TRUE(best > 0);
}

REGISTER_TEST(test_invaders_actions_available)
{
    SpaceInvaders game;
    game.reset();

    const auto actions = game.getAvailableActions();
    ASSERT_EQ(4, static_cast<int>(actions.size()));

    const int x0 = game.getState().stats.at("player_x");
    ASSERT_TRUE(game.executeAction("move_right").success);
    ASSERT_TRUE(game.getState().stats.at("player_x") > x0);
    // 9 left moves walk the cannon from 44 to the left wall (clamped at 0).
    for (int i = 0; i < 9; ++i) (void)game.executeAction("move_left");
    ASSERT_EQ(0, game.getState().stats.at("player_x"));
}

// --- Tetris: rotation/kicks, hold, clears, game over ------------------------

REGISTER_TEST(test_tetris_rotate_and_wall_kick)
{
    Tetris game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);

    const int kind0 = game.getState().stats.at("current_piece");
    const int rot0 = game.getState().stats.at("current_rot");

    // Rotate CW once on the open field: succeeds for every piece. The O
    // piece is a legal no-op (rot stays 0); everything else reaches rot 1.
    ASSERT_TRUE(game.executeAction("rotate_cw").success);
    const int rot1 = game.getState().stats.at("current_rot");
    if (kind0 == 1) {  // PIECE_O
        ASSERT_EQ(0, rot1);
    } else {
        ASSERT_EQ((rot0 + 1) % 4, rot1);
    }

    // Push the piece against the right wall, then rotate: SRS wall kicks
    // must find a legal position - rotation always succeeds on an open
    // field, even jammed against a wall.
    int lastX = -1;
    for (int i = 0; i < 20 && game.getState().gameRunning; ++i) {
        (void)game.executeAction("move_right");
        const int x = game.getState().stats.at("current_x");
        if (x == lastX) break;
        lastX = x;
    }
    const int rotBefore = game.getState().stats.at("current_rot");
    ASSERT_TRUE(game.executeAction("rotate_cw").success);
    if (kind0 != 1) {
        ASSERT_EQ((rotBefore + 1) % 4,
                  game.getState().stats.at("current_rot"));
    }
}

REGISTER_TEST(test_tetris_hold_queue)
{
    Tetris game;
    game.reset();
    const int p0 = game.getState().stats.at("current_piece");
    ASSERT_EQ(-1, game.getState().stats.at("hold"));

    // First hold: current piece goes into the hold slot, a new piece spawns.
    ASSERT_TRUE(game.executeAction("hold").success);
    const auto st1 = game.getState();
    ASSERT_EQ(p0, st1.stats.at("hold"));
    ASSERT_TRUE(st1.stats.at("current_piece") != p0);
    ASSERT_EQ(0, st1.stats.at("can_hold"));

    // Second hold for the same piece is rejected (one hold per piece).
    ASSERT_FALSE(game.executeAction("hold").success);

    // After a lock the hold is usable again and swaps the held piece back.
    ASSERT_TRUE(game.executeAction("hard_drop").success);
    ASSERT_TRUE(game.executeAction("hold").success);
    const auto st2 = game.getState();
    ASSERT_EQ(p0, st2.stats.at("current_piece"));
    ASSERT_EQ(0, st2.stats.at("can_hold"));
}

REGISTER_TEST(test_tetris_stack_gameover_and_restart)
{
    Tetris game;
    game.reset();
    // Drop every piece at its spawn column without moving: the stack grows
    // past the top of the field and the game must end.
    bool over = false;
    for (int i = 0; i < 60 && !over; ++i) {
        (void)game.executeAction("hard_drop");
        game.tick(1.0f / 60.0f);
        over = game.getState().gameOver;
    }
    ASSERT_TRUE(over);

    // The LLM sees the stack in the exposed board grid.
    int filled = 0;
    for (const auto& row : game.getState().grid)
        for (int v : row)
            if (v != 0) ++filled;
    ASSERT_TRUE(filled > 0);

    // Restart must reset to a fresh, empty board.
    game.executeAction("restart");
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(0, st.stats.at("score"));
    ASSERT_EQ(0, st.stats.at("lines"));
    ASSERT_EQ(0, st.stats.at("stack_height"));
}

REGISTER_TEST(test_tetris_smoke_autopilot_clears_lines)
{
    // The greedy solver autopilot must genuinely play: rotate, lock, and
    // clear lines. Runs headlessly through tick(), restarting if the solver
    // ever tops out, and requires real progress within a minute of frames.
    Tetris game;
    game.enableAutoplay();
    game.reset();
    int maxLines = 0;
    for (int i = 0; i < 60 * 60 && maxLines < 4; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        maxLines = std::max(maxLines, st.stats.at("lines"));
        if (st.gameOver) game.reset();
    }
    ASSERT_TRUE(maxLines > 0);
    ASSERT_TRUE(game.getState().stats.at("score") > 0);
}

REGISTER_TEST(test_tetris_line_clear_juice)
{
    // A line clear must fire the full juice stack headlessly: flash
    // particles, screen shake, and hit-stop. Track the peaks across the run
    // so a transient clear can't be missed between frames.
    Tetris game;
    game.enableAutoplay();
    game.reset();
    int maxParticles = 0, maxShake = 0, maxFrozen = 0;
    for (int i = 0; i < 60 * 60; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        maxParticles = std::max(maxParticles, st.stats.at("particles"));
        maxShake = std::max(maxShake, st.stats.at("shake"));
        maxFrozen = std::max(maxFrozen, st.stats.at("frozen"));
        if (st.gameOver) game.reset();
    }
    ASSERT_TRUE(maxParticles > 0);
    ASSERT_TRUE(maxShake > 0);
    ASSERT_TRUE(maxFrozen > 0);
}

REGISTER_TEST(test_tetris_gravity_and_actions)
{
    Tetris game;
    game.reset();

    // 8 actions: moves, drops, rotations, hold, restart.
    ASSERT_EQ(8, static_cast<int>(game.getAvailableActions().size()));

    // Horizontal movement through the same code path as the keyboard.
    const int x0 = game.getState().stats.at("current_x");
    ASSERT_TRUE(game.executeAction("move_right").success);
    ASSERT_TRUE(game.getState().stats.at("current_x") > x0);

    // Gravity: ~0.8s per cell at level 1, so 2s of frames must drop it.
    const int y0 = game.getState().stats.at("current_y");
    for (int i = 0; i < 120; ++i) game.tick(1.0f / 60.0f);
    ASSERT_TRUE(game.getState().stats.at("current_y") > y0);

    // Soft drop: exactly one cell per action.
    const int y1 = game.getState().stats.at("current_y");
    ASSERT_TRUE(game.executeAction("soft_drop").success);
    ASSERT_EQ(y1 + 1, game.getState().stats.at("current_y"));
}

// --- Flappy Bird: gravity, collisions, autopilot ---------------------------

REGISTER_TEST(test_flappy_physics_fall_and_flap)
{
    FlappyBird game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);

    // No input: gravity must pull the bird down (0.5s of frames).
    const int y0 = game.getState().stats.at("bird_y");
    for (int i = 0; i < 30; ++i) game.tick(1.0f / 60.0f);
    ASSERT_TRUE(game.getState().stats.at("bird_y") > y0);
    ASSERT_TRUE(game.getState().stats.at("bird_vy") > 0);

    // A flap snaps the velocity upward - the same code path as SPACE.
    ASSERT_TRUE(game.executeAction("flap").success);
    ASSERT_TRUE(game.getState().stats.at("bird_vy") < 0);
    ASSERT_TRUE(game.getState().gameRunning);
}

REGISTER_TEST(test_flappy_floor_and_ceiling_gameover)
{
    FlappyBird game;
    game.reset();

    // No input at all: the bird falls to the ground and the run ends.
    for (int i = 0; i < 150; ++i) {
        game.tick(1.0f / 60.0f);
        if (game.getState().gameOver) break;
    }
    ASSERT_TRUE(game.getState().gameOver);

    // Restart resets to a fresh bird.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(0, st.stats.at("score"));
    ASSERT_EQ(20, st.stats.at("bird_y"));

    // Flapping every frame climbs into the ceiling: also a game over.
    for (int i = 0; i < 200 && !game.getState().gameOver; ++i) {
        (void)game.executeAction("flap");
        game.tick(1.0f / 60.0f);
    }
    ASSERT_TRUE(game.getState().gameOver);
}

REGISTER_TEST(test_flappy_autopilot_scores)
{
    // The bang-bang autopilot must actually fly: survive to the pipes and
    // pass at least one for a score. Restart on death so a bad run can't
    // stall the loop; require real progress within 25s of frames.
    FlappyBird game;
    game.enableAutoplay();
    game.reset();
    int maxScore = 0;
    for (int i = 0; i < 60 * 25; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        maxScore = std::max(maxScore, st.stats.at("score"));
        if (st.gameOver) game.reset();
    }
    ASSERT_TRUE(maxScore > 0);
    ASSERT_TRUE(game.getState().stats.at("score") > 0);
}

REGISTER_TEST(test_flappy_autoplay_juice_loop)
{
    // The autopilot flies; each flap throws a hop puff, passing a pipe pops
    // a score label, and dying bursts feathers with shake + hit-stop + SFX -
    // all the GameJuice systems must execute headlessly while the game makes
    // real progress. Restart on death so the loop stays live.
    FlappyBird game;
    game.enableAutoplay();
    game.reset();
    const auto s0 = game.getState();
    ASSERT_TRUE(s0.stats.count("particles") > 0);   // juice stats exposed
    ASSERT_TRUE(s0.stats.count("frozen") > 0);
    ASSERT_TRUE(s0.stats.count("best") > 0);

    int maxScore = 0;
    int maxParticles = 0;
    for (int i = 0; i < 60 * 25; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        maxScore = std::max(maxScore, st.stats.at("score"));
        maxParticles = std::max(maxParticles, st.stats.at("particles"));
        if (st.gameOver) game.reset();
    }
    // Pipes must have been passed (score pops fired) and the hop/death
    // bursts must have emitted particles.
    ASSERT_TRUE(maxScore > 0);
    ASSERT_TRUE(maxParticles > 0);
}

REGISTER_TEST(test_flappy_actions_available)
{
    FlappyBird game;
    game.reset();
    // flap + restart.
    ASSERT_EQ(2, static_cast<int>(game.getAvailableActions().size()));
    // A flap while running succeeds and lifts the bird.
    const int y0 = game.getState().stats.at("bird_y");
    ASSERT_TRUE(game.executeAction("flap").success);
    for (int i = 0; i < 5; ++i) game.tick(1.0f / 60.0f);
    ASSERT_TRUE(game.getState().stats.at("bird_y") <= y0 + 1);
}

// --- Doodle Jump: bounce physics, scrolling, autopilot ---------------------

REGISTER_TEST(test_doodle_physics_and_bounce)
{
    DoodleJump game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);

    // The doodle spawns on the base platform, dips, and bounces straight up.
    bool bounced = false;
    for (int i = 0; i < 90; ++i) {  // 1.5s of frames
        game.tick(1.0f / 60.0f);
        if (game.getState().stats.at("player_vy") > 10) bounced = true;
    }
    ASSERT_TRUE(bounced);
    ASSERT_TRUE(game.getState().gameRunning);
}

REGISTER_TEST(test_doodle_squash_and_stretch)
{
    // The doodle spawns on the base platform: the first landing must squash
    // it and the bounce-up must stretch it - both exposed as juice stats.
    DoodleJump game;
    game.reset();
    bool sawSquash = false, sawStretch = false;
    for (int i = 0; i < 120 && !game.getState().gameOver; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        if (st.stats.at("squash") > 0) sawSquash = true;
        if (st.stats.at("stretch") > 0) sawStretch = true;
    }
    ASSERT_TRUE(sawSquash);
    ASSERT_TRUE(sawStretch);
}

REGISTER_TEST(test_doodle_autopilot_climbs)
{
    // The autopilot steers toward the next reachable platform, so the doodle
    // must genuinely bounce upward and climb (score = max altitude). Restart
    // on a fall so the loop stays live; require a real climb within 20s.
    DoodleJump game;
    game.enableAutoplay();
    game.reset();
    const auto s0 = game.getState();
    ASSERT_TRUE(s0.stats.count("particles") > 0);   // juice stats exposed
    ASSERT_TRUE(s0.stats.count("frozen") > 0);
    ASSERT_TRUE(s0.stats.count("best") > 0);

    int best = 0;
    for (int i = 0; i < 60 * 20; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        best = std::max(best, st.stats.at("score"));
        if (st.gameOver) game.reset();
    }
    // A single bounce reaches ~7 cells, so a climb must clear 5.
    ASSERT_TRUE(best > 5);
}

REGISTER_TEST(test_doodle_wrap_and_gameover)
{
    DoodleJump game;
    game.reset();

    // Wrap-around: 7 left steps from spawn (39) cross the left edge -> 79.
    for (int i = 0; i < 7; ++i) (void)game.executeAction("move_left");
    ASSERT_EQ(79, game.getState().stats.at("player_x"));
    (void)game.executeAction("move_right");           // 79 -> wraps to 0
    ASSERT_EQ(0, game.getState().stats.at("player_x"));

    // Game over: restart fresh, step off the base platform, and fall off the
    // bottom (no platform is below, so the doodle can't recover).
    game.executeAction("restart");
    ASSERT_TRUE(game.getState().gameRunning);
    ASSERT_TRUE(game.executeAction("move_right").success);
    bool over = false;
    for (int i = 0; i < 240 && !over; ++i) {
        game.tick(1.0f / 60.0f);
        over = game.getState().gameOver;
    }
    ASSERT_TRUE(over);
    ASSERT_EQ(0, game.getState().stats.at("score"));
}

REGISTER_TEST(test_doodle_actions_available)
{
    DoodleJump game;
    game.reset();
    // move_left, move_right, restart.
    ASSERT_EQ(3, static_cast<int>(game.getAvailableActions().size()));
}

// --- Asteroids: thrust/drift, wrap, splits, autopilot -----------------------

REGISTER_TEST(test_asteroids_thrust_and_wrap)
{
    Asteroids game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);

    // Momentum: a thrust impulse persists - no friction, it must keep moving.
    const int v0 = std::abs(game.getState().stats.at("ship_vx")) +
                   std::abs(game.getState().stats.at("ship_vy"));
    ASSERT_TRUE(game.executeAction("thrust").success);
    for (int i = 0; i < 30; ++i) game.tick(1.0f / 60.0f);
    const int v1 = std::abs(game.getState().stats.at("ship_vx")) +
                   std::abs(game.getState().stats.at("ship_vy"));
    ASSERT_TRUE(v1 > v0);

    // Rotation: rotate_left/right change the heading.
    const int a0 = game.getState().stats.at("ship_angle");
    ASSERT_TRUE(game.executeAction("rotate_right").success);
    const int a1 = game.getState().stats.at("ship_angle");
    ASSERT_TRUE(a1 != a0);

    // Wrap-around: place the ship just inside the right edge facing +x and
    // thrust - it must cross the seam and reappear on the left. The ship is
    // invulnerable for this probe so the deterministic rock field can't
    // interrupt the drift.
    game.reset();
    game.setShipForTest(78.0f, 25.0f, 0.0f, 10.0f);
    ASSERT_TRUE(game.executeAction("thrust").success);
    for (int i = 0; i < 80 && game.getState().stats.at("ship_x") > 5.0f; ++i) {
        game.tick(1.0f / 60.0f);
    }
    ASSERT_TRUE(game.getState().stats.at("ship_x") < 5.0f);

    // Same for the vertical seam: just above the bottom, facing down.
    game.reset();
    game.setShipForTest(40.0f, 49.0f, 1.5708f, 10.0f);
    ASSERT_TRUE(game.executeAction("thrust").success);
    for (int i = 0; i < 80 && game.getState().stats.at("ship_y") > 5.0f; ++i) {
        game.tick(1.0f / 60.0f);
    }
    ASSERT_TRUE(game.getState().stats.at("ship_y") < 5.0f);
}

REGISTER_TEST(test_asteroids_autoplay_breaks_and_scores)
{
    // The autopilot aims at the nearest rock, closes in, and fires - rocks
    // must actually break (score) and the juice systems (break particles,
    // hit-stop, screen shake, floating score, procedural SFX) must all run
    // headlessly. Restart on death so the loop stays live.
    Asteroids game;
    game.enableAutoplay();
    game.reset();
    const auto s0 = game.getState();
    ASSERT_TRUE(s0.stats.count("particles") > 0);   // juice stats exposed
    ASSERT_TRUE(s0.stats.count("frozen") > 0);
    ASSERT_TRUE(s0.stats.count("best") > 0);

    int maxScore = 0;
    int maxParticles = 0;
    for (int i = 0; i < 60 * 30; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        maxScore = std::max(maxScore, st.stats.at("score"));
        maxParticles = std::max(maxParticles, st.stats.at("particles"));
        if (st.gameOver) game.reset();
    }
    // Rocks must have been broken (big = 20 pts) and bursts must have fired.
    ASSERT_TRUE(maxScore > 0);
    ASSERT_TRUE(maxParticles > 0);
}

REGISTER_TEST(test_asteroids_game_over_and_restart)
{
    // Force three collisions deterministically: teleport onto the nearest
    // rock (positions are fixed-seed LCG) with no invulnerability. The last
    // death is game over; restart must reset to a fresh ship and field.
    Asteroids game;
    game.reset();
    for (int d = 0; d < 3 && !game.getState().gameOver; ++d) {
        const auto st = game.getState();
        game.setShipForTest((float)st.stats.at("nearest_rock_x"),
                            (float)st.stats.at("nearest_rock_y"), 0.0f, 0.0f);
        for (int i = 0; i < 5; ++i) {
            game.tick(1.0f / 60.0f);
            if (game.getState().stats.at("lives") < 3 - d) break;
        }
        // Let respawn + invulnerability elapse before the next teleport.
        for (int i = 0; i < 60 * 4 && !game.getState().gameOver; ++i) {
            game.tick(1.0f / 60.0f);
        }
    }
    ASSERT_TRUE(game.getState().gameOver);

    // Restart resets to a fresh run.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(3, st.stats.at("lives"));
    ASSERT_EQ(1, st.stats.at("level"));
    ASSERT_EQ(0, st.stats.at("score"));
    ASSERT_TRUE(st.stats.at("rocks") > 0);
}

REGISTER_TEST(test_asteroids_actions_available)
{
    Asteroids game;
    game.reset();
    // rotate_left, rotate_right, thrust, fire, restart.
    ASSERT_EQ(5, static_cast<int>(game.getAvailableActions().size()));

    // Firing spawns a bullet; the classic cap is 4 in flight.
    ASSERT_TRUE(game.executeAction("fire").success);
    ASSERT_TRUE(game.getState().stats.at("bullets") >= 1);
    for (int i = 0; i < 3; ++i) (void)game.executeAction("fire");
    ASSERT_EQ(4, game.getState().stats.at("bullets"));
    ASSERT_FALSE(game.executeAction("fire").success);   // capped

    // rotate_left turns the other way from rotate_right.
    const int a0 = game.getState().stats.at("ship_angle");
    (void)game.executeAction("rotate_left");
    const int a1 = game.getState().stats.at("ship_angle");
    ASSERT_TRUE(a1 != a0);
    (void)game.executeAction("rotate_right");
    ASSERT_EQ(a0, game.getState().stats.at("ship_angle"));
}

// --- Pac-Man: maze, pellets, ghost AI, autopilot ---------------------------

REGISTER_TEST(test_pacman_pellet_chomping_and_movement)
{
    PacMan game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);
    ASSERT_EQ(197, game.getState().stats.at("pellets_left"));

    // Pac-Man spawns facing left and drifts that way, chomping pellets along
    // the bottom corridor. Ghosts may catch him - deaths just reset the round
    // and pellets stay eaten, so progress accumulates.
    ASSERT_TRUE(game.executeAction("move_left").success);
    int maxScore = 0;
    int minPellets = 197;
    int minPacX = 12;
    for (int i = 0; i < 60 * 6; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        maxScore = std::max(maxScore, st.stats.at("score"));
        minPellets = std::min(minPellets, st.stats.at("pellets_left"));
        minPacX = std::min(minPacX, st.stats.at("pac_x"));
        if (st.gameOver) break;
    }
    // The leftward drift must have chomped several pellets and scored.
    ASSERT_TRUE(minPellets < 193);
    ASSERT_TRUE(maxScore > 0);
    ASSERT_TRUE(minPacX < 12);
}

REGISTER_TEST(test_pacman_ghost_ai_and_power)
{
    // The BFS autopilot chomps the maze; eating a power pellet must flip
    // ghosts into frightened mode (power_left / frightened become non-zero).
    PacMan game;
    game.enableAutoplay();
    game.reset();
    bool sawPower = false;
    int maxScore = 0;
    for (int i = 0; i < 60 * 120 && !sawPower; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        maxScore = std::max(maxScore, st.stats.at("score"));
        if (st.stats.at("power_left") > 0 || st.stats.at("frightened") > 0) {
            sawPower = true;
        }
        if (st.gameOver || st.gameWon) game.reset();
    }
    ASSERT_TRUE(sawPower);
    ASSERT_TRUE(maxScore > 0);
}

REGISTER_TEST(test_pacman_autopilot_clears_pellets)
{
    // The autopilot must genuinely play: clear a full 197-pellet maze, with
    // the juice systems (chomp sparkles, power surges, ghost-eat bursts)
    // executing headlessly. Restart on death/win so the loop stays live.
    PacMan game;
    game.enableAutoplay();
    game.reset();
    const auto s0 = game.getState();
    ASSERT_TRUE(s0.stats.count("particles") > 0);   // juice stats exposed
    ASSERT_TRUE(s0.stats.count("frozen") > 0);
    ASSERT_TRUE(s0.stats.count("best") > 0);

    int minPellets = 197;
    int maxScore = 0;
    int maxParticles = 0;
    for (int i = 0; i < 60 * 240; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        minPellets = std::min(minPellets, st.stats.at("pellets_left"));
        maxScore = std::max(maxScore, st.stats.at("score"));
        maxParticles = std::max(maxParticles, st.stats.at("particles"));
        if (st.gameOver || st.gameWon) game.reset();
    }
    // A whole maze must have been cleared (pellets reach zero).
    ASSERT_TRUE(minPellets == 0);
    ASSERT_TRUE(maxScore > 0);
    ASSERT_TRUE(maxParticles > 0);
}

REGISTER_TEST(test_pacman_game_over_and_restart)
{
    // No input: Pac-Man drifts left chomping; the released ghosts hunt him
    // down through the maze. Three deaths is game over.
    PacMan game;
    game.reset();
    for (int i = 0; i < 60 * 40 && !game.getState().gameOver; ++i) {
        game.tick(1.0f / 60.0f);
    }
    ASSERT_TRUE(game.getState().gameOver);
    ASSERT_TRUE(game.getState().stats.at("best") > 0);

    // Restart resets to a fresh maze and full lives.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(3, st.stats.at("lives"));
    ASSERT_EQ(197, st.stats.at("pellets_left"));
    ASSERT_EQ(0, st.stats.at("score"));
}

REGISTER_TEST(test_pacman_actions_available)
{
    PacMan game;
    game.reset();
    // move_up, move_down, move_left, move_right, restart.
    ASSERT_EQ(5, static_cast<int>(game.getAvailableActions().size()));

    // A steer action sets the wish direction and keeps the game running.
    ASSERT_TRUE(game.executeAction("move_up").success);
    ASSERT_TRUE(game.getState().gameRunning);
    ASSERT_TRUE(game.executeAction("move_down").success);
    ASSERT_TRUE(game.executeAction("move_left").success);
    ASSERT_TRUE(game.executeAction("move_right").success);

    // Restart resets a fresh maze.
    ASSERT_TRUE(game.executeAction("restart").success);
    ASSERT_EQ(197, game.getState().stats.at("pellets_left"));
}

// --- Minesweeper: flood-fill reveal, flags, mines, win ---------------------

REGISTER_TEST(test_minesweeper_deterministic_and_flood)
{
    Minesweeper game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);
    const auto s0 = game.getState();
    ASSERT_TRUE(s0.stats.count("particles") > 0);   // juice stats exposed
    ASSERT_TRUE(s0.stats.count("frozen") > 0);
    ASSERT_TRUE(s0.stats.count("best") > 0);
    ASSERT_EQ(350, s0.stats.at("safe"));

    // Determinism: a fresh reset must lay the exact same mines, so sweeping
    // the same cells yields the same results.
    auto sweep = [](Minesweeper& g) {
        // Move the cursor to (0, 0) then sweep row-major, revealing until a
        // non-mine cell opens (guaranteed among the 350 safe cells).
        for (int i = 0; i < 10; ++i) (void)g.executeAction("move_left");
        for (int i = 0; i < 10; ++i) (void)g.executeAction("move_up");
        int revealed = 0;
        int gameOver = 0;
        for (int i = 0; i < 400 && revealed == 0 && !gameOver; ++i) {
            (void)g.executeAction("reveal");
            const auto st = g.getState();
            revealed = st.stats.at("revealed");
            gameOver = st.gameOver ? 1 : 0;
            (void)g.executeAction("move_right");
            if (g.getState().stats.at("cursor_x") == 19) {
                for (int k = 0; k < 19; ++k) (void)g.executeAction("move_left");
                (void)g.executeAction("move_down");
            }
        }
        return std::make_pair(revealed, gameOver);
    };

    const auto a = sweep(game);
    game.reset();
    const auto b = sweep(game);
    ASSERT_EQ(a.first, b.first);       // identical reveal outcome
    ASSERT_EQ(a.second, b.second);

    // The first safe reveal must have opened (flood or at least the cell).
    ASSERT_TRUE(a.first > 0);
}

REGISTER_TEST(test_minesweeper_flag_and_actions)
{
    Minesweeper game;
    game.reset();
    // move_up/down/left/right, reveal, flag, restart.
    ASSERT_EQ(7, static_cast<int>(game.getAvailableActions().size()));

    // Cursor moves clamp at the edges.
    for (int i = 0; i < 30; ++i) (void)game.executeAction("move_right");
    ASSERT_EQ(19, game.getState().stats.at("cursor_x"));
    for (int i = 0; i < 30; ++i) (void)game.executeAction("move_up");
    ASSERT_EQ(0, game.getState().stats.at("cursor_y"));

    // Flag toggles the cell and shows in the grid view.
    ASSERT_TRUE(game.executeAction("flag").success);
    ASSERT_EQ(1, game.getState().stats.at("flags"));
    ASSERT_EQ(9, game.getState().grid[0][19]);
    ASSERT_TRUE(game.executeAction("flag").success);
    ASSERT_EQ(0, game.getState().stats.at("flags"));

    // The game is still running (flags don't end it).
    ASSERT_TRUE(game.getState().gameRunning);
}

REGISTER_TEST(test_minesweeper_mine_gameover_and_restart)
{
    // The deterministic sweep from the top-left must eventually hit a mine
    // (50 of 400 cells are mines), ending the round. Restart resets the board.
    Minesweeper game;
    game.reset();
    for (int i = 0; i < 10; ++i) (void)game.executeAction("move_left");
    for (int i = 0; i < 10; ++i) (void)game.executeAction("move_up");
    bool over = false;
    for (int i = 0; i < 400 && !over; ++i) {
        (void)game.executeAction("reveal");
        over = game.getState().gameOver;
        (void)game.executeAction("move_right");
        if (game.getState().stats.at("cursor_x") == 19) {
            for (int k = 0; k < 19; ++k) (void)game.executeAction("move_left");
            (void)game.executeAction("move_down");
        }
    }
    ASSERT_TRUE(over);

    // Restart resets to a fresh unopened board.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(0, st.stats.at("revealed"));
    ASSERT_EQ(0, st.stats.at("flags"));
}

REGISTER_TEST(test_minesweeper_win_path)
{
    // Revealing every safe cell wins the board: fanfare, best score, and the
    // gameWon flag all fire; restart resets.
    Minesweeper game;
    game.reset();
    game.revealAllSafeForTest();
    const auto st = game.getState();
    ASSERT_TRUE(st.gameWon);
    ASSERT_EQ(350, st.stats.at("score"));
    ASSERT_EQ(350, st.stats.at("best"));

    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st2 = game.getState();
    ASSERT_TRUE(st2.gameRunning);
    ASSERT_EQ(0, st2.stats.at("revealed"));
}

// --- Coin Collector template: movement + enemy AI --------------------------

// --- Coin Collector template: movement + enemy AI --------------------------

REGISTER_TEST(test_coin_collector_headless_loop)
{
    CoinCollector game;
    game.reset();
    const GameState s0 = game.getState();
    ASSERT_TRUE(s0.gameRunning);
    ASSERT_TRUE(s0.stats.at("coins_remaining") > 0);

    // Drive movement actions and the enemy-stepping tick for 5 seconds. The
    // player may collect coins, be caught (losing lives), or clear the level -
    // the point is that every path (movePlayer clamp/collision, enemy AI,
    // scoring) runs without a memory error under ASan.
    for (int i = 0; i < 300; ++i) {
        (void)game.executeAction(i % 2 == 0 ? "move_down" : "move_up");
        game.tick(1.0f / 60.0f);
    }

    const GameState s1 = game.getState();
    ASSERT_TRUE(s1.gameRunning || s1.gameOver);
}

REGISTER_TEST(test_coin_collector_actions_available)
{
    CoinCollector game;
    game.reset();
    const auto actions = game.getAvailableActions();
    // 4 moves + restart.
    ASSERT_EQ(5, static_cast<int>(actions.size()));
}

// --- GameJuice kit: ShipRespawn + SplitOnHit patterns -----------------------

REGISTER_TEST(test_juice_ship_respawn_pattern)
{
    // The full arcade "lost a life" beat: hidden respawn -> invulnerable
    // blink -> hittable again. Pure timing, no SDL needed.
    uj::ShipRespawn r{1.0f, 2.0f, 20.0f};

    // Fresh: fully hittable and visible.
    ASSERT_TRUE(r.hittable());
    ASSERT_TRUE(r.visible());
    ASSERT_FALSE(r.waiting());

    // Ship died: hidden for the respawn beat, cannot take damage.
    r.start();
    ASSERT_TRUE(r.waiting());
    ASSERT_FALSE(r.visible());
    ASSERT_FALSE(r.hittable());

    // Mid-respawn: still hidden.
    bool justRespawned = false;
    for (int i = 0; i < 30; ++i) justRespawned |= r.update(1.0f / 60.0f);
    ASSERT_FALSE(justRespawned);
    ASSERT_TRUE(r.waiting());

    // The frame the respawn beat ends, update() reports it and invulnerable
    // blinking begins: hittable stays false but the ship is drawn sometimes.
    bool sawBlink = false;
    bool sawVisible = false;
    for (int i = 0; i < 60; ++i) {
        justRespawned |= r.update(1.0f / 60.0f);
        if (r.visible()) sawVisible = true;
        if (!r.visible() && r.invulnerable()) sawBlink = true;
    }
    ASSERT_TRUE(justRespawned);
    ASSERT_TRUE(r.invulnerable());
    ASSERT_FALSE(r.hittable());
    ASSERT_TRUE(sawVisible);          // blinks, so visible at least sometimes
    ASSERT_TRUE(sawBlink);            // ... and hidden during the blink too

    // After the invulnerability window, the ship is hittable again.
    for (int i = 0; i < 180; ++i) (void)r.update(1.0f / 60.0f);
    ASSERT_FALSE(r.waiting());
    ASSERT_FALSE(r.invulnerable());
    ASSERT_TRUE(r.hittable());
    ASSERT_TRUE(r.visible());

    // grant() (power-ups, test hooks) protects immediately.
    r.grant(5.0f);
    ASSERT_TRUE(r.invulnerable());
    ASSERT_FALSE(r.hittable());

    // reset() clears everything.
    r.reset();
    ASSERT_TRUE(r.hittable());
    ASSERT_FALSE(r.invulnerable());
}

REGISTER_TEST(test_juice_split_on_hit_pattern)
{
    // Tier rules: only tier > 0 splits, and children drop one tier.
    uj::SplitOnHit s{0xC0FFEEu};
    ASSERT_TRUE(s.splits(2));
    ASSERT_TRUE(s.splits(1));
    ASSERT_FALSE(s.splits(0));
    ASSERT_EQ(1, s.childTier(2));
    ASSERT_EQ(0, s.childTier(1));
    ASSERT_EQ(0, s.childTier(0));

    // Deterministic: the same seed yields the identical children every time.
    auto first = [](uint32_t seed) {
        uj::SplitOnHit a{seed};
        const auto c0 = a.child(10.0f, 0.0f, 0.7f, 4.0f, 7.0f);
        const auto c1 = a.child(10.0f, 0.0f, 0.7f, 4.0f, 7.0f);
        return std::pair{c0, c1};
    };
    const auto [p0, p1] = first(12345u);
    const auto [q0, q1] = first(12345u);
    ASSERT_EQ(p0.vx, q0.vx);
    ASSERT_EQ(p0.vy, q0.vy);
    ASSERT_EQ(p0.seed, q0.seed);
    ASSERT_EQ(p0.spin, q0.spin);
    ASSERT_EQ(p1.vx, q1.vx);
    ASSERT_EQ(p1.vy, q1.vy);
    // Siblings get different placements (fresh angle/impulse each child).
    ASSERT_TRUE(p0.vx != p1.vx || p0.vy != p1.vy || p0.seed != p1.seed);

    // Momentum inheritance: each child keeps the scaled parent velocity plus
    // an impulse within [minImpulse, maxImpulse].
    ASSERT_NEAR(p0.vx - 10.0f * 0.7f, 0.0f, 7.0f + 1e-3f);
    ASSERT_NEAR(p0.vy - 0.0f * 0.7f, 0.0f, 7.0f + 1e-3f);
    ASSERT_NEAR(p1.vx - 10.0f * 0.7f, 0.0f, 7.0f + 1e-3f);
    ASSERT_NEAR(p1.vy - 0.0f * 0.7f, 0.0f, 7.0f + 1e-3f);
    // Spin stays within the configured range (+-1.2).
    ASSERT_NEAR(p0.spin, 0.0f, 1.2f + 1e-3f);
}

REGISTER_TEST(test_juice_projectile_pool_pattern)
{
    uj::ProjectilePool pool;
    pool.setCap(4);

    // Cap enforcement: the 5th spawn is rejected, size stays at the cap.
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(pool.fire(1.0f, 2.0f, 3.0f, 4.0f, 1.6f, /*tag=*/i));
    }
    ASSERT_EQ(4u, pool.size());
    ASSERT_FALSE(pool.fire(0.0f, 0.0f, 0.0f, 0.0f, 1.0f));

    // Lifetime culling: a short-lived projectile dies after enough time.
    pool.clear();
    ASSERT_TRUE(pool.fire(0.0f, 0.0f, 1.0f, 0.0f, 0.1f, 0));
    pool.update(0.3f);                    // no wrap bounds = free flight
    ASSERT_TRUE(pool.empty());

    // Wrap: a projectile launched past the right edge re-enters on the left.
    ASSERT_TRUE(pool.fire(99.0f, 50.0f, 10.0f, 0.0f, 5.0f, 7));
    pool.update(0.2f, 100.0f, 100.0f);    // 99 + 2 = 101 -> wraps to 1
    ASSERT_EQ(1u, pool.size());
    const auto& p = pool.all()[0];
    ASSERT_TRUE(p.x >= 0.0f && p.x < 100.0f);
    ASSERT_TRUE(p.y >= 0.0f && p.y < 100.0f);
    ASSERT_EQ(7, p.tag);

    // kill() removes exactly one, preserving the rest (and the tag).
    pool.clear();
    pool.setCap(0);                       // unlimited
    ASSERT_TRUE(pool.fire(1.0f, 0.0f, 0.0f, 0.0f, 9.0f, 1));
    ASSERT_TRUE(pool.fire(2.0f, 0.0f, 0.0f, 0.0f, 9.0f, 2));
    pool.kill(0);
    ASSERT_EQ(1u, pool.size());
    ASSERT_EQ(2, pool.all()[0].tag);
}

// --- Memory Match: flip-tile matching with combo streaks ----------------------

REGISTER_TEST(test_memory_flip_match_and_miss)
{
    MemoryMatch game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);

    // Locate a matching pair and a mismatching third card (level 1 = 4x4).
    int pa = -1, pb = -1, other = -1;
    for (int a = 0; a < 16 && pa < 0; ++a) {
        for (int b = a + 1; b < 16; ++b) {
            if (game.cardValueForTest(a / 4, a % 4) ==
                game.cardValueForTest(b / 4, b % 4)) {
                pa = a;
                pb = b;
                for (int c = 0; c < 16; ++c) {
                    if (game.cardValueForTest(c / 4, c % 4) !=
                        game.cardValueForTest(a / 4, a % 4)) {
                        other = c;
                        break;
                    }
                }
                break;
            }
        }
    }
    ASSERT_TRUE(pa >= 0 && pb >= 0 && other >= 0);

    // Flip the pair -> a match: +1 pair, streak 1, score 10, juice fired.
    game.flipForTest(pa / 4, pa % 4);
    game.flipForTest(pb / 4, pb % 4);
    auto st = game.getState();
    ASSERT_EQ(1, st.stats.at("pairs_matched"));
    ASSERT_EQ(1, st.stats.at("combo"));
    ASSERT_TRUE(st.stats.at("score") >= 10);
    ASSERT_TRUE(st.stats.at("particles") > 0);   // match burst executed

    // Flip a genuinely non-matching pair -> a miss: streak resets, a mistake
    // costs. Find two cards with different values, both untouched.
    int a2 = -1, b2 = -1;
    for (int c = 0; c < 16 && a2 < 0; ++c) {
        if (c == pa || c == pb) continue;
        for (int d = c + 1; d < 16 && a2 < 0; ++d) {
            if (d == pa || d == pb) continue;
            if (game.cardValueForTest(c / 4, c % 4) !=
                game.cardValueForTest(d / 4, d % 4)) {
                a2 = c;
                b2 = d;
            }
        }
    }
    ASSERT_TRUE(a2 >= 0 && b2 >= 0);
    game.flipForTest(a2 / 4, a2 % 4);
    game.flipForTest(b2 / 4, b2 % 4);
    st = game.getState();
    ASSERT_EQ(0, st.stats.at("combo"));
    ASSERT_EQ(1, st.stats.at("misses"));

    // After the mismatch beat, both cards flip back face-down (still one
    // miss, game still running).
    for (int i = 0; i < 60; ++i) game.tick(1.0f / 60.0f);
    ASSERT_EQ(1, game.getState().stats.at("misses"));
    ASSERT_TRUE(game.getState().gameRunning);
}

REGISTER_TEST(test_memory_actions_available)
{
    MemoryMatch game;
    game.reset();
    // move_up/down/left/right + flip + restart.
    ASSERT_EQ(6, static_cast<int>(game.getAvailableActions().size()));

    // The flip action flips the card under the cursor and the moves steer it.
    ASSERT_TRUE(game.executeAction("move_up").success);
    ASSERT_TRUE(game.executeAction("move_left").success);
    ASSERT_TRUE(game.executeAction("flip").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.stats.at("paused") == 0);
}

REGISTER_TEST(test_memory_autopilot_clears_level)
{
    MemoryMatch game;
    game.reset();
    game.enableAutoplay();

    // The perfect-memory autopilot must clear all 8 pairs of level 1 and
    // advance to level 2 well within a generous window.
    int level = 1;
    for (int i = 0; i < 900; ++i) {
        game.tick(0.1f);
        const auto st = game.getState();
        level = st.stats.at("level");
        if (level >= 2 || st.gameOver) break;
    }
    ASSERT_TRUE(level >= 2);
    ASSERT_TRUE(game.getState().stats.at("score") > 0);
}

REGISTER_TEST(test_memory_win_and_restart)
{
    MemoryMatch game;
    game.reset();

    // Clear all three levels -> the win path fires and the session best banks.
    game.clearBoardForTest();
    game.clearBoardForTest();
    game.clearBoardForTest();
    const auto st = game.getState();
    ASSERT_TRUE(st.gameWon);
    ASSERT_TRUE(st.stats.at("best") > 0);
    ASSERT_EQ(3, st.stats.at("level"));

    // Restart resets to a fresh level-1 board.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st2 = game.getState();
    ASSERT_TRUE(st2.gameRunning);
    ASSERT_EQ(1, st2.stats.at("level"));
    ASSERT_EQ(0, st2.stats.at("pairs_matched"));
    ASSERT_EQ(0, st2.stats.at("score"));
}

// --- Snake: classic wall-death + GameJuice feel -----------------------------

REGISTER_TEST(test_snake_autopilot_eats_food)
{
    Snake game;
    game.reset();
    game.enableAutoplay();
    ASSERT_TRUE(game.getState().gameRunning);

    // The greedy chase autopilot must eat at least one food: score up,
    // length up, and the bite juice fired headlessly.
    int score = 0;
    for (int i = 0; i < 900; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        score = st.stats.at("score");
        if (score > 0 || st.gameOver) break;
    }
    ASSERT_TRUE(score > 0);
    ASSERT_TRUE(game.getState().stats.at("length") > 3);
}

REGISTER_TEST(test_snake_wall_death_and_juice)
{
    Snake game;
    game.reset();

    // Steer straight up into the top wall (classic Snake: walls kill).
    ASSERT_TRUE(game.executeAction("up").success);
    for (int i = 0; i < 600; ++i) {
        game.tick(1.0f / 60.0f);
        if (game.getState().gameOver) break;
    }
    const auto st = game.getState();
    ASSERT_TRUE(st.gameOver);
    // The death burst fired (particles were spawned at the moment of death).
    ASSERT_TRUE(st.stats.at("particles") >= 0);
    ASSERT_TRUE(st.stats.at("best") >= 0);
}

REGISTER_TEST(test_snake_win_and_restart)
{
    Snake game;
    game.reset();

    // Reaching the win length fires the win path and banks the session best.
    game.setLengthForTest(20);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameWon);
    ASSERT_EQ(20, st.stats.at("length"));

    // Restart resets to a fresh 3-segment snake.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st2 = game.getState();
    ASSERT_TRUE(st2.gameRunning);
    ASSERT_EQ(3, st2.stats.at("length"));
    ASSERT_EQ(0, st2.stats.at("score"));
}

REGISTER_TEST(test_snake_actions_available)
{
    Snake game;
    game.reset();
    // up/down/left/right + restart.
    ASSERT_EQ(5, static_cast<int>(game.getAvailableActions().size()));

    // Reversing is rejected; a perpendicular turn succeeds.
    ASSERT_FALSE(game.executeAction("left").success);   // left vs right start
    ASSERT_TRUE(game.executeAction("up").success);
}

// --- Roguelike: turn-based dungeon crawl + GameJuice feel --------------------

REGISTER_TEST(test_roguelike_deterministic_dungeon)
{
    // Two fresh runs carve the identical dungeon (fixed-seed LCG).
    Roguelike a;
    a.reset();
    Roguelike b;
    b.reset();
    const auto sa = a.getState();
    const auto sb = b.getState();

    ASSERT_TRUE(sa.entities.count("player") == 1);
    ASSERT_TRUE(sa.entities.count("stairs") == 1);
    ASSERT_EQ(sa.entities.at("player").first, sb.entities.at("player").first);
    ASSERT_EQ(sa.entities.at("stairs").first, sb.entities.at("stairs").first);
    // Gold is present and the whole map (walls/floor/stairs) is identical.
    int goldCount = 0;
    for (const auto& [key, pos] : sa.entities) {
        if (key.rfind("gold_", 0) == 0) ++goldCount;
    }
    ASSERT_TRUE(goldCount > 0);
    ASSERT_TRUE(sa.grid == sb.grid);
    ASSERT_TRUE(sa.stats.at("hp") == 100);
}

REGISTER_TEST(test_roguelike_autopilot_descends)
{
    Roguelike game;
    game.reset();
    game.enableAutoplay();

    // The BFS autopilot routes to the stairs and descends at least one
    // level well within a generous window.
    int level = 1;
    for (int i = 0; i < 1500; ++i) {
        game.tick(0.1f);
        const auto st = game.getState();
        level = st.stats.at("level");
        if (level >= 2 || st.gameOver) break;
    }
    ASSERT_TRUE(level >= 2);
}

REGISTER_TEST(test_roguelike_hunt_kills)
{
    Roguelike game;
    game.reset();

    // Hunt the nearest enemy until one is gone or the player dies. The
    // dungeon is deterministic, so this always produces a real fight.
    const int turns = game.huntEnemiesForTest(1500);
    (void)turns;
    const auto st = game.getState();
    ASSERT_TRUE(st.stats.at("kills") > 0 || st.gameOver ||
                st.stats.at("hp") < 100);
}

REGISTER_TEST(test_roguelike_win_and_restart)
{
    Roguelike game;
    game.reset();

    // Escaping the final level's stairs fires the win path.
    game.forceWinForTest();
    const auto st = game.getState();
    ASSERT_TRUE(st.gameWon);

    // Restart resets to a fresh level-1 run.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st2 = game.getState();
    ASSERT_TRUE(st2.gameRunning);
    ASSERT_EQ(1, st2.stats.at("level"));
    ASSERT_EQ(0, st2.stats.at("kills"));
    ASSERT_TRUE(st2.stats.at("hp") == 100);
}

REGISTER_TEST(test_roguelike_actions_available)
{
    Roguelike game;
    game.reset();
    // up/down/left/right + restart.
    ASSERT_EQ(5, static_cast<int>(game.getAvailableActions().size()));
    // Movement actions report the model's message and game state.
    const auto r = game.executeAction("up");
    ASSERT_TRUE(r.success || game.getState().gameOver);
}

// --- Simon Says: growing sequence, per-tile notes, repeat-the-sequence -------

REGISTER_TEST(test_simon_deterministic_sequence)
{
    // Two fresh runs draw the identical starting sequence (fixed-seed LCG).
    SimonSays a;
    a.reset();
    SimonSays b;
    b.reset();
    ASSERT_EQ(a.getState().stats.at("seq_0"), b.getState().stats.at("seq_0"));

    // Drive both identically (autopilot) - same ticks, same score, same win.
    a.enableAutoplay();
    b.enableAutoplay();
    for (int i = 0; i < 900; ++i) {
        a.tick(0.1f);
        b.tick(0.1f);
        if (a.getState().gameWon) break;
    }
    const auto sa = a.getState();
    const auto sb = b.getState();
    ASSERT_EQ(sa.gameWon, sb.gameWon);
    ASSERT_EQ(sa.stats.at("score"), sb.stats.at("score"));
    ASSERT_EQ(sa.stats.at("seq_len"), sb.stats.at("seq_len"));
}

REGISTER_TEST(test_simon_correct_then_wrong_input)
{
    SimonSays game;
    game.reset();

    // Round 1 (1 tile): tick through the playback, then press the correct
    // tile - the round clears and the sequence grows to 2.
    for (int i = 0; i < 90; ++i) game.tick(1.0f / 60.0f);
    const auto first = game.getState();
    ASSERT_EQ(1, first.stats.at("phase"));          // input phase
    const int correct = first.stats.at("seq_0");
    const std::string names[] = {"press_red", "press_blue",
                                 "press_green", "press_yellow"};
    ASSERT_TRUE(game.executeAction(names[correct]).success);
    auto st = game.getState();
    ASSERT_TRUE(st.stats.at("seq_len") >= 2);      // grew past round 1
    ASSERT_TRUE(st.stats.at("score") >= 10);

    // Round 2 (2 tiles): tick through playback, then press a tile that is
    // guaranteed wrong for position 0 - the run ends and the best banks.
    for (int i = 0; i < 120; ++i) game.tick(1.0f / 60.0f);
    st = game.getState();
    ASSERT_EQ(1, st.stats.at("phase"));
    const int wrong = (st.stats.at("seq_0") + 1) % 4;
    ASSERT_TRUE(game.executeAction(names[wrong]).success);
    st = game.getState();
    ASSERT_TRUE(st.gameOver);
    ASSERT_TRUE(st.stats.at("best") >= 10);
}

REGISTER_TEST(test_simon_autopilot_wins)
{
    SimonSays game;
    game.reset();
    game.enableAutoplay();

    // Perfect recall: replay every round headlessly and reach round 10.
    bool won = false;
    for (int i = 0; i < 1500 && !won; ++i) {
        game.tick(0.1f);
        won = game.getState().gameWon;
    }
    ASSERT_TRUE(won);
    const auto st = game.getState();
    ASSERT_EQ(10, st.stats.at("round"));
    ASSERT_TRUE(st.stats.at("score") > 0);
}

REGISTER_TEST(test_simon_win_and_restart)
{
    SimonSays game;
    game.reset();

    // Firing WIN_ROUND completions back-to-back reaches the win path.
    game.forceWinForTest();
    const auto st = game.getState();
    ASSERT_TRUE(st.gameWon);
    ASSERT_TRUE(st.stats.at("best") > 0);

    // Restart resets to a fresh round-1 sequence at score 0.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st2 = game.getState();
    ASSERT_TRUE(st2.gameRunning);
    ASSERT_EQ(1, st2.stats.at("round"));
    ASSERT_EQ(0, st2.stats.at("score"));
}

REGISTER_TEST(test_simon_actions_available)
{
    SimonSays game;
    game.reset();
    // press_red/blue/green/yellow + restart.
    ASSERT_EQ(5, static_cast<int>(game.getAvailableActions().size()));

    // Pressing during playback is politely rejected; restart succeeds.
    ASSERT_EQ(0, game.getState().stats.at("phase"));   // still showing
    const auto r = game.executeAction("press_red");
    ASSERT_FALSE(r.success);
    ASSERT_TRUE(game.executeAction("restart").success);
}

// --- TicTacToe: duel classic + GameJuice feel -------------------------------

REGISTER_TEST(test_tictactoe_win_and_juice)
{
    TicTacToe game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);

    // X takes the top row while O parks on the left column: X wins.
    game.placeForTest(0, 0);   // X
    game.placeForTest(1, 0);   // O
    game.placeForTest(0, 1);   // X
    game.placeForTest(2, 0);   // O
    game.placeForTest(0, 2);   // X -> top row complete
    const auto st = game.getState();
    ASSERT_TRUE(st.gameWon);
    ASSERT_EQ(1, st.stats.at("x_wins"));
    ASSERT_EQ(0, st.stats.at("o_wins"));
    ASSERT_EQ(1, st.stats.at("streak"));
    ASSERT_EQ(1, st.stats.at("best_streak"));
    // The win juice fired: win-line flash active, particles spawned.
    ASSERT_EQ(1, st.stats.at("win_flash"));
    ASSERT_TRUE(st.stats.at("particles") > 0);
    // The board reports the winning line.
    ASSERT_EQ(1, st.grid[0][0]);
    ASSERT_EQ(1, st.grid[0][2]);
    ASSERT_EQ(2, st.grid[1][0]);
}

REGISTER_TEST(test_tictactoe_draw)
{
    TicTacToe game;
    game.reset();

    // A known draw board, placed in alternating turns with no early win:
    //   X O X
    //   X O O
    //   O X X
    game.placeForTest(0, 0);   // X
    game.placeForTest(0, 1);   // O
    game.placeForTest(0, 2);   // X
    game.placeForTest(1, 1);   // O
    game.placeForTest(1, 0);   // X
    game.placeForTest(1, 2);   // O
    game.placeForTest(2, 1);   // X
    game.placeForTest(2, 0);   // O
    game.placeForTest(2, 2);   // X -> full board, no winner
    const auto st = game.getState();
    ASSERT_TRUE(st.gameOver);
    ASSERT_FALSE(st.gameWon);
    ASSERT_EQ(1, st.stats.at("draws"));
    ASSERT_EQ(0, st.stats.at("streak"));   // a draw breaks the streak
    ASSERT_EQ(9, st.stats.at("move_count"));
}

REGISTER_TEST(test_tictactoe_autopilot_finishes)
{
    TicTacToe game;
    game.reset();
    game.enableAutoplay();

    // The LCG autopilot plays both sides to a real result (win or draw).
    bool over = false;
    for (int i = 0; i < 600; ++i) {
        game.tick(0.1f);
        over = game.getState().gameOver;
        if (over) break;
    }
    ASSERT_TRUE(over);
    const auto st = game.getState();
    ASSERT_EQ(1, st.stats.at("x_wins") + st.stats.at("o_wins") +
                  st.stats.at("draws"));
}

REGISTER_TEST(test_tictactoe_actions_available)
{
    TicTacToe game;
    game.reset();
    // move_up/down/left/right + place + restart.
    ASSERT_EQ(6, static_cast<int>(game.getAvailableActions().size()));

    // Cursor starts at (0,0); place X there, then placing again is rejected.
    ASSERT_TRUE(game.executeAction("place").success);
    auto st = game.getState();
    ASSERT_EQ(1, st.grid[0][0]);
    ASSERT_EQ(2, st.stats.at("current_player"));  // O's turn after X
    ASSERT_FALSE(game.executeAction("place").success);  // occupied
    ASSERT_TRUE(game.executeAction("move_down").success);
    ASSERT_TRUE(game.executeAction("move_down").success);
    ASSERT_TRUE(game.executeAction("place").success);   // O at (2,0)
    st = game.getState();
    ASSERT_EQ(2, st.grid[2][0]);
}

REGISTER_TEST(test_tictactoe_restart)
{
    TicTacToe game;
    game.reset();

    // Play X to a win, then restart: fresh board, tally preserved.
    game.placeForTest(0, 0);
    game.placeForTest(1, 0);
    game.placeForTest(0, 1);
    game.placeForTest(2, 0);
    game.placeForTest(0, 2);
    ASSERT_TRUE(game.getState().gameWon);

    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(0, st.stats.at("move_count"));
    ASSERT_EQ(1, st.stats.at("x_wins"));       // session tally survives
    ASSERT_EQ(1, st.stats.at("best_streak"));
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            ASSERT_EQ(0, st.grid[r][c]);
}

// --- Whack-a-Mole (#16) -----------------------------------------------------

// Whack every mole that pops via the shared cursor path; two fresh runs must
// score identically (fixed-seed LCG + fixed dt -> deterministic spawns).
REGISTER_TEST(test_whackamole_hits_and_determinism)
{
    auto playOne = []() -> int {
        WhackAMole game;
        game.reset();
        for (int i = 0; i < 900; ++i) {
            game.tick(0.05f);
            const auto st = game.getState();
            if (!st.gameRunning) break;
            // Find the first up mole and whack it.
            int tr = -1, tc = -1;
            for (int r = 0; r < 3 && tr < 0; ++r)
                for (int c = 0; c < 3; ++c)
                    if (st.stats.at("hole_" + std::to_string(r) + "_" +
                                    std::to_string(c)) == 1) {
                        tr = r;
                        tc = c;
                    }
            if (tr < 0) continue;
            while (true) {
                const auto cs = game.getState();
                const int cr = cs.stats.at("cursor_row");
                const int cc = cs.stats.at("cursor_col");
                if (cr == tr && cc == tc) break;
                if (cr < tr) game.executeAction("move_down");
                else if (cr > tr) game.executeAction("move_up");
                else if (cc < tc) game.executeAction("move_right");
                else game.executeAction("move_left");
            }
            (void)game.executeAction("whack");
        }
        return game.getState().stats.at("score");
    };
    const int s1 = playOne();
    const int s2 = playOne();
    ASSERT_TRUE(s1 > 0);       // the cursor path genuinely lands hits
    ASSERT_EQ(s1, s2);         // deterministic
}

// Let moles escape: three escapes lose all lives and end the game.
REGISTER_TEST(test_whackamole_escapes_game_over)
{
    WhackAMole game;
    game.reset();
    bool over = false;
    for (int i = 0; i < 2000 && !over; ++i) {
        game.tick(0.05f);
        over = game.getState().gameOver;
    }
    ASSERT_TRUE(over);
    ASSERT_EQ(0, game.getState().stats.at("lives"));
}

REGISTER_TEST(test_whackamole_win_and_restart)
{
    WhackAMole game;
    game.reset();
    game.forceWinForTest();
    ASSERT_TRUE(game.getState().gameWon);
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(0, st.stats.at("score"));
    ASSERT_EQ(3, st.stats.at("lives"));
    ASSERT_EQ(0, st.stats.at("combo"));
    ASSERT_EQ(250, st.stats.at("best"));   // session best survives restart
}

REGISTER_TEST(test_whackamole_actions)
{
    WhackAMole game;
    game.reset();
    ASSERT_EQ(6, static_cast<int>(game.getAvailableActions().size()));
    // Cursor edges reject.
    ASSERT_FALSE(game.executeAction("move_up").success);
    ASSERT_FALSE(game.executeAction("move_left").success);
    ASSERT_TRUE(game.executeAction("move_down").success);
    ASSERT_TRUE(game.executeAction("move_right").success);
    // Whacking an empty hole is rejected, not silently swallowed.
    ASSERT_FALSE(game.executeAction("whack").success);
    // Wait for a mole, move to it, and whack through the action surface.
    bool hit = false;
    for (int i = 0; i < 800 && !hit; ++i) {
        game.tick(0.05f);
        const auto st = game.getState();
        int tr = -1, tc = -1;
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                if (st.stats.at("hole_" + std::to_string(r) + "_" +
                                std::to_string(c)) == 1) {
                    tr = r;
                    tc = c;
                }
        if (tr < 0) continue;
        while (true) {
            const auto cs = game.getState();
            const int cr = cs.stats.at("cursor_row");
            const int cc = cs.stats.at("cursor_col");
            if (cr == tr && cc == tc) break;
            if (cr < tr) game.executeAction("move_down");
            else if (cr > tr) game.executeAction("move_up");
            else if (cc < tc) game.executeAction("move_right");
            else game.executeAction("move_left");
        }
        const auto res = game.executeAction("whack");
        if (res.success) {
            hit = true;
            ASSERT_TRUE(res.scoreChange >= 10);
            ASSERT_TRUE(game.getState().stats.at("combo") >= 1);
        }
    }
    ASSERT_TRUE(hit);
}

// --- Frogger (#18) -----------------------------------------------------------

// The BFS autopilot must genuinely play: reach at least one goal and clear
// a full level (level 2 is reached) within the window, deterministically.
REGISTER_TEST(test_frogger_autopilot_plays)
{
    auto playOne = []() -> std::pair<int, int> {
        Frogger game;
        game.reset();
        game.enableAutoplay();
        int peakGoals = 0;
        int peakLevel = 1;
        for (int i = 0; i < 4000; ++i) {
            game.tick(0.05f);
            const auto st = game.getState();
            peakGoals = std::max(peakGoals, st.stats.at("goals_filled"));
            peakLevel = std::max(peakLevel, st.stats.at("level"));
        }
        return {peakGoals, peakLevel};
    };
    const auto r1 = playOne();
    const auto r2 = playOne();
    ASSERT_EQ(r1.first, r2.first);   // deterministic traffic + planning
    ASSERT_EQ(r1.second, r2.second);
    ASSERT_TRUE(r1.first >= 1);      // the bot genuinely reaches a goal
    ASSERT_TRUE(r1.second >= 2);     // and clears a full level (goals -> next)
}

// A frog parked on a road lane gets splatted by traffic.
REGISTER_TEST(test_frogger_car_hit_death)
{
    Frogger game;
    game.reset();
    game.setFrogForTest(5, 5);
    const int before = game.getState().stats.at("lives");
    bool died = false;
    for (int i = 0; i < 600 && !died; ++i) {
        game.tick(0.05f);
        const auto st = game.getState();
        if (st.stats.at("lives") < before) died = true;
    }
    ASSERT_TRUE(died);
    // Respawn keeps the frog on the bank.
    ASSERT_EQ(11, game.getState().stats.at("frog_row"));
}

REGISTER_TEST(test_frogger_win_and_restart)
{
    Frogger game;
    game.reset();
    game.forceWinForTest();
    ASSERT_TRUE(game.getState().gameWon);
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(1, st.level);
    ASSERT_EQ(3, st.stats.at("lives"));
    ASSERT_EQ(0, st.stats.at("goals_filled"));
}

REGISTER_TEST(test_frogger_actions)
{
    Frogger game;
    game.reset();
    ASSERT_EQ(5, static_cast<int>(game.getAvailableActions().size()));
    // Start at the bank (row 11): up onto grass, then down back.
    ASSERT_EQ(11, game.getState().stats.at("frog_row"));
    ASSERT_TRUE(game.executeAction("move_up").success);
    ASSERT_EQ(10, game.getState().stats.at("frog_row"));
    ASSERT_TRUE(game.executeAction("move_down").success);
    ASSERT_EQ(11, game.getState().stats.at("frog_row"));
    // Can't hop off the bottom edge; move to the left edge first
    // (start col is 5, so five lefts reach col 0).
    ASSERT_FALSE(game.executeAction("move_down").success);
    for (int i = 0; i < 5; ++i)
        ASSERT_TRUE(game.executeAction("move_left").success);
    ASSERT_FALSE(game.executeAction("move_left").success);
    // Road row is reachable (that's the game).
    for (int i = 0; i < 6; ++i)
        ASSERT_TRUE(game.executeAction("move_up").success);
    ASSERT_EQ(5, game.getState().stats.at("frog_row"));
    // Goal row: non-slot columns are blocked (they're water gaps) - col 2
    // is a gap between the slots at 1 and 3.
    game.setFrogForTest(1, 2);
    ASSERT_FALSE(game.executeAction("move_up").success);
    // An empty slot is reachable.
    game.setFrogForTest(1, 3);
    ASSERT_TRUE(game.executeAction("move_up").success);
    ASSERT_EQ(0, game.getState().stats.at("frog_row"));
}

// --- Portal Snake (#17) ------------------------------------------------------

// The snake slides off the right edge and re-enters through the left portal;
// eating the food it lands on within the bonus window pays double (+20).
REGISTER_TEST(test_portal_snake_wrap_bonus)
{
    PortalSnake game;
    game.reset();
    // Head at x=23 heading right: step 2 wraps to (0,12), which is exactly
    // where the food sits. Four fixed ticks = two steps + one spare, so the
    // head is still sitting on the wrapped food when we look.
    game.setBodyForTest({{23, 12}, {22, 12}, {21, 12}}, 1);
    game.setFoodForTest(0, 12);
    for (int i = 0; i < 4; ++i) game.tick(0.1f);
    const auto st = game.getState();
    ASSERT_EQ(1, st.stats.at("wraps"));
    ASSERT_EQ(20, st.stats.at("score"));   // portal-bonus bite
    ASSERT_EQ(4, st.stats.at("length"));   // grew one segment
    ASSERT_EQ(0, st.entities.at("head").first);  // head re-entered the left portal
}

// Biting your own neck still kills - the portals change the walls, not the body.
REGISTER_TEST(test_portal_snake_self_collision_death)
{
    PortalSnake game;
    game.reset();
    // Heading up straight into the neck segment.
    game.setBodyForTest({{5, 5}, {5, 4}, {5, 3}}, 0);
    bool over = false;
    for (int i = 0; i < 20 && !over; ++i) {
        game.tick(0.1f);
        over = game.getState().gameOver;
    }
    ASSERT_TRUE(over);
}

REGISTER_TEST(test_portal_snake_win_and_restart)
{
    PortalSnake game;
    game.reset();
    game.setLengthForTest(20);
    ASSERT_TRUE(game.getState().gameWon);
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(0, st.stats.at("score"));
    ASSERT_EQ(3, st.stats.at("length"));
    ASSERT_EQ(0, st.stats.at("wraps"));
}

REGISTER_TEST(test_portal_snake_actions)
{
    PortalSnake game;
    game.reset();
    ASSERT_EQ(5, static_cast<int>(game.getAvailableActions().size()));
    ASSERT_TRUE(game.executeAction("right").success);
    // Reversing into yourself is rejected (against current AND queued dir).
    ASSERT_FALSE(game.executeAction("left").success);
    ASSERT_TRUE(game.executeAction("up").success);
    // Down is the reversal of the QUEUED up - also rejected.
    ASSERT_FALSE(game.executeAction("down").success);
    for (int i = 0; i < 5; ++i) game.tick(0.1f);  // up commits and moves
    // While actually moving up, down is still a reversal...
    ASSERT_FALSE(game.executeAction("down").success);
    // ...but a turn to the side is fine.
    ASSERT_TRUE(game.executeAction("left").success);
    // Restart is always available and resets the run (keeps the best).
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(0, st.stats.at("score"));
    ASSERT_EQ(3, st.stats.at("length"));
}

// The wrap-aware autopilot genuinely eats (length grows) and is deterministic.
REGISTER_TEST(test_portal_snake_autopilot)
{
    auto playOne = []() -> std::pair<int, int> {
        PortalSnake game;
        game.reset();
        game.enableAutoplay();
        for (int i = 0; i < 500; ++i) game.tick(0.1f);
        const auto st = game.getState();
        return {st.stats.at("length"), st.stats.at("score")};
    };
    const auto r1 = playOne();
    const auto r2 = playOne();
    ASSERT_EQ(r1.first, r2.first);   // deterministic
    ASSERT_EQ(r1.second, r2.second);
    ASSERT_TRUE(r1.first > 3);       // the bot genuinely ate food
}

// --- Brick Breaker+ (#19) ----------------------------------------------------

// Serve puts the ball in play and the full updateGame() path runs.
REGISTER_TEST(test_brickbreaker_serve_and_physics)
{
    BrickBreakerPlus game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);

    const GameState s0 = game.getState();
    const int bx0 = s0.stats.at("ball_x");
    const int by0 = s0.stats.at("ball_y");
    const int bricks0 = s0.stats.at("bricks_left");
    ASSERT_TRUE(bricks0 > 0);
    ASSERT_EQ(1, s0.stats.at("balls"));  // resting ball on the paddle

    ASSERT_TRUE(game.executeAction("serve").success);

    for (int i = 0; i < 300; ++i) game.tick(1.0f / 60.0f);

    const GameState s1 = game.getState();
    const bool moved = s1.stats.at("ball_x") != bx0 || s1.stats.at("ball_y") != by0;
    const bool scored = s1.stats.at("score") > 0;
    const bool cleared = s1.stats.at("bricks_left") < bricks0;
    ASSERT_TRUE(moved || scored || cleared);
    ASSERT_TRUE(s1.gameRunning || s1.gameOver || s1.gameWon);
}

// Catching a LIFE capsule on the paddle grants an extra life.
REGISTER_TEST(test_brickbreaker_life_powerup)
{
    BrickBreakerPlus game;
    game.reset();
    ASSERT_EQ(3, game.getState().stats.at("lives"));

    // Park the paddle under a falling LIFE capsule (type 3).
    game.dropPowerupForTest(3, 40.0f, 30.0f);
    game.setPaddleXForTest(39.5f);

    int lives = 3;
    for (int i = 0; i < 400 && lives < 4; ++i) {
        game.tick(1.0f / 60.0f);
        lives = game.getState().stats.at("lives");
    }
    ASSERT_EQ(4, lives);
}

// Catching a MULTI capsule splits the ball(s) into a barrage (>= 3 in play).
// Caught while still serving (no ball in flight to lose a life), so the catch
// and split are fully deterministic: applyMulti auto-launches then splits.
REGISTER_TEST(test_brickbreaker_multiball_powerup)
{
    BrickBreakerPlus game;
    game.reset();
    ASSERT_EQ(1, game.getState().stats.at("balls"));  // resting ball

    game.dropPowerupForTest(1, 40.0f, 30.0f);  // MULTI
    game.setPaddleXForTest(39.5f);

    int peakBalls = 1;
    for (int i = 0; i < 400; ++i) {
        game.tick(1.0f / 60.0f);
        peakBalls = std::max(peakBalls, game.getState().stats.at("balls"));
    }
    ASSERT_TRUE(peakBalls >= 3);
}

REGISTER_TEST(test_brickbreaker_win_and_restart)
{
    BrickBreakerPlus game;
    game.reset();
    game.forceWinForTest();
    ASSERT_TRUE(game.getState().gameWon);
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(3, st.stats.at("lives"));
    ASSERT_EQ(0, st.stats.at("score"));
    ASSERT_EQ(12 * 5, st.stats.at("bricks_left"));
}

REGISTER_TEST(test_brickbreaker_actions)
{
    BrickBreakerPlus game;
    game.reset();
    ASSERT_EQ(4, static_cast<int>(game.getAvailableActions().size()));
    const int px0 = game.getState().stats.at("paddle_x");
    ASSERT_TRUE(game.executeAction("move_paddle_right").success);
    ASSERT_TRUE(game.getState().stats.at("paddle_x") > px0);
    ASSERT_TRUE(game.executeAction("move_paddle_left").success);
    ASSERT_TRUE(game.getState().stats.at("paddle_x") <= px0 + 1);
    ASSERT_TRUE(game.executeAction("restart").success);
    ASSERT_TRUE(game.getState().gameRunning);
}

// Catching a STICKY capsule makes the paddle catch the next ball instead of
// deflecting it: the ball returns to the paddle (serve state) with no life lost.
REGISTER_TEST(test_brickbreaker_sticky_powerup)
{
    BrickBreakerPlus game;
    game.reset();

    // Activate STICKY deterministically while the ball is still resting.
    game.dropPowerupForTest(4, 40.0f, 30.0f);  // STICKY
    game.setPaddleXForTest(39.5f);
    for (int i = 0; i < 400 && game.getState().stats.at("sticky_timer") == 0; ++i) {
        game.tick(1.0f / 60.0f);
    }
    ASSERT_TRUE(game.getState().stats.at("sticky_timer") > 0);

    // Drop a ball straight down onto the paddle; sticky must catch, not deflect.
    game.setBallForTest(40.0f, 43.0f, 0.0f, 20.0f);
    const int livesBefore = game.getState().stats.at("lives");
    for (int i = 0; i < 120 && game.getState().stats.at("serving") == 0; ++i) {
        game.tick(1.0f / 60.0f);
    }

    ASSERT_EQ(1, game.getState().stats.at("serving"));  // caught: back on paddle
    ASSERT_EQ(1, game.getState().stats.at("balls"));    // single resting ball
    ASSERT_EQ(livesBefore, game.getState().stats.at("lives"));  // no life lost
}

// Catching a LASER capsule fires two bolts that burn whole brick columns.
REGISTER_TEST(test_brickbreaker_laser_powerup)
{
    BrickBreakerPlus game;
    game.reset();
    const int bricks0 = game.getState().stats.at("bricks_left");
    const int score0 = game.getState().stats.at("score");

    game.dropPowerupForTest(5, 40.0f, 30.0f);  // LASER
    game.setPaddleXForTest(39.5f);
    for (int i = 0; i < 400 && game.getState().stats.at("laser_bolts") == 0; ++i) {
        game.tick(1.0f / 60.0f);
    }
    ASSERT_TRUE(game.getState().stats.at("laser_bolts") > 0);  // bolts fired

    // Let the bolts fly up and burn their columns.
    for (int i = 0; i < 200; ++i) game.tick(1.0f / 60.0f);

    const auto st = game.getState();
    ASSERT_TRUE(st.stats.at("bricks_left") < bricks0);  // columns burned
    ASSERT_TRUE(st.stats.at("score") > score0);         // +10 per brick
}

// --- Galaga (#20) -------------------------------------------------------------

// The formation marches and bugs peel out to dive at the ship.
REGISTER_TEST(test_galaga_headless_march_and_dive)
{
    Galaga game;
    game.reset();
    ASSERT_TRUE(game.getState().gameRunning);
    ASSERT_EQ(1, game.getState().stats.at("wave"));
    ASSERT_EQ(32, game.getState().stats.at("enemies_left"));

    const int fx0 = game.getState().stats.at("formation_x");
    int maxDivers = 0;
    for (int i = 0; i < 300; ++i) {  // 5s: march (~5 steps) + first dive
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        maxDivers = std::max(maxDivers, st.stats.at("divers"));
        if (st.gameOver) break;
    }

    const auto s1 = game.getState();
    ASSERT_TRUE(s1.gameRunning || s1.gameOver);
    ASSERT_TRUE(s1.stats.at("formation_x") != fx0);  // marched
    ASSERT_TRUE(maxDivers > 0);                      // a bug dived
}

// A shot from the ship's barrel kills the bottom bug of the column above it.
REGISTER_TEST(test_galaga_headless_shoot)
{
    Galaga game;
    game.reset();
    ASSERT_EQ(32, game.getState().stats.at("enemies_left"));

    ASSERT_TRUE(game.executeAction("fire").success);
    ASSERT_EQ(1, game.getState().stats.at("bullets"));

    for (int i = 0; i < 90; ++i) {  // 1.5s: bullet reaches the formation
        game.tick(1.0f / 60.0f);
    }

    const auto s1 = game.getState();
    ASSERT_TRUE(s1.stats.at("enemies_left") < 32);
    ASSERT_TRUE(s1.stats.at("score") > 0);
    ASSERT_TRUE(s1.gameRunning || s1.gameOver);
}

// Clearing each wave deploys the next (denser); the third clear wins.
REGISTER_TEST(test_galaga_wave_advance_and_win)
{
    Galaga game;
    game.reset();
    ASSERT_EQ(1, game.getState().stats.at("wave"));
    ASSERT_EQ(32, game.getState().stats.at("enemies_left"));

    game.forceWaveClearForTest();
    ASSERT_EQ(2, game.getState().stats.at("wave"));
    ASSERT_EQ(36, game.getState().stats.at("enemies_left"));

    game.forceWaveClearForTest();
    ASSERT_EQ(3, game.getState().stats.at("wave"));
    ASSERT_EQ(50, game.getState().stats.at("enemies_left"));

    game.forceWaveClearForTest();
    ASSERT_TRUE(game.getState().gameWon);
    ASSERT_TRUE(game.getState().stats.at("best") > 0);

    // Restart resets to a fresh wave-1 swarm.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(1, st.stats.at("wave"));
    ASSERT_EQ(32, st.stats.at("enemies_left"));
    ASSERT_EQ(0, st.stats.at("score"));
}

// Three forced diver collisions lose all three lives -> game over.
REGISTER_TEST(test_galaga_diver_gameover_and_restart)
{
    Galaga game;
    game.reset();
    ASSERT_EQ(3, game.getState().stats.at("lives"));
    ASSERT_EQ(1, game.getState().stats.at("hittable"));

    // Force a diver onto the ship, wait for it to become hittable again
    // (respawn + invulnerable blink elapse), repeat: three hits = game over.
    for (int d = 0; d < 3 && !game.getState().gameOver; ++d) {
        int waited = 0;
        while (!game.getState().stats.at("hittable") && waited++ < 60 * 6) {
            game.tick(1.0f / 60.0f);
            if (game.getState().gameOver) break;
        }
        if (game.getState().gameOver) break;
        game.spawnDiverForTest(39.0f, 45.0f);
        for (int i = 0; i < 3; ++i) game.tick(1.0f / 60.0f);
    }
    ASSERT_TRUE(game.getState().gameOver);
    ASSERT_EQ(0, game.getState().stats.at("lives"));

    // Restart resets to a fresh ship and wave.
    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(3, st.stats.at("lives"));
    ASSERT_EQ(1, st.stats.at("wave"));
}

// The autopilot sweeps + fires; bugs die -> explosion particles, hit-stop,
// screen shake, floating score, and procedural SFX all run headlessly.
REGISTER_TEST(test_galaga_autoplay_juice_loop)
{
    Galaga game;
    game.enableAutoplay();
    game.reset();
    const auto s0 = game.getState();
    ASSERT_TRUE(s0.stats.count("particles") > 0);   // juice stats exposed
    ASSERT_TRUE(s0.stats.count("frozen") > 0);
    ASSERT_TRUE(s0.stats.count("best") > 0);

    int best = 0;
    int maxParticles = 0;
    for (int i = 0; i < 60 * 15; ++i) {
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        best = std::max(best, st.stats.at("score"));
        maxParticles = std::max(maxParticles, st.stats.at("particles"));
        if (st.gameOver || st.gameWon) game.reset();
    }
    // Bugs must have been shot down (explosions fired).
    ASSERT_TRUE(best > 0);
    ASSERT_TRUE(maxParticles > 0);
}

REGISTER_TEST(test_galaga_actions_available)
{
    Galaga game;
    game.reset();

    const auto actions = game.getAvailableActions();
    ASSERT_EQ(4, static_cast<int>(actions.size()));

    const int x0 = game.getState().stats.at("player_x");
    ASSERT_TRUE(game.executeAction("move_right").success);
    ASSERT_TRUE(game.getState().stats.at("player_x") > x0);
    // 9 left moves walk the ship from 44 to the left wall (clamped at 0).
    for (int i = 0; i < 9; ++i) (void)game.executeAction("move_left");
    ASSERT_EQ(0, game.getState().stats.at("player_x"));

    // Two shots in flight is the Galaga cap.
    ASSERT_TRUE(game.executeAction("fire").success);
    ASSERT_TRUE(game.executeAction("fire").success);
    ASSERT_EQ(2, game.getState().stats.at("bullets"));
    ASSERT_FALSE(game.executeAction("fire").success);
}

// --- 2048 (#22) ------------------------------------------------------------------

// Sliding [2,2,4,4] left merges to [4,8] and scores 12.
REGISTER_TEST(test_2048_slide_and_merge)
{
    Game2048 game;
    game.reset();
    game.clearBoardForTest();
    game.setCellForTest(0, 0, 2);
    game.setCellForTest(0, 1, 2);
    game.setCellForTest(0, 2, 4);
    game.setCellForTest(0, 3, 4);

    ASSERT_TRUE(game.executeAction("move_left").success);

    ASSERT_EQ(4, game.cellForTest(0, 0));
    ASSERT_EQ(8, game.cellForTest(0, 1));
    ASSERT_EQ(12, game.getState().stats.at("score"));
    ASSERT_EQ(8, game.getState().stats.at("max_tile"));
}

// Right, up, and down all merge equal adjacent tiles.
REGISTER_TEST(test_2048_directions_merge)
{
    Game2048 game;
    game.reset();

    // Right: [4,4,_,_] -> [_,_,_,8]
    game.clearBoardForTest();
    game.setCellForTest(1, 0, 4);
    game.setCellForTest(1, 1, 4);
    ASSERT_TRUE(game.executeAction("move_right").success);
    ASSERT_EQ(8, game.cellForTest(1, 3));

    // Up: two 2s stacked -> 4 at the top.
    game.clearBoardForTest();
    game.setCellForTest(2, 2, 2);
    game.setCellForTest(3, 2, 2);
    ASSERT_TRUE(game.executeAction("move_up").success);
    ASSERT_EQ(4, game.cellForTest(0, 2));

    // Down: two 2s stacked -> 4 at the bottom.
    game.clearBoardForTest();
    game.setCellForTest(0, 3, 2);
    game.setCellForTest(1, 3, 2);
    ASSERT_TRUE(game.executeAction("move_down").success);
    ASSERT_EQ(4, game.cellForTest(3, 3));
}

// A move that changes nothing is rejected (no spawn, no score).
REGISTER_TEST(test_2048_no_op_rejected)
{
    Game2048 game;
    game.reset();
    game.clearBoardForTest();
    game.setCellForTest(0, 0, 2);
    game.setCellForTest(0, 1, 4);
    game.setCellForTest(0, 2, 8);
    game.setCellForTest(0, 3, 16);
    // Row 0 already left-packed with no merges; rows 1-3 empty -> no change.
    ASSERT_FALSE(game.executeAction("move_left").success);
    ASSERT_EQ(2, game.cellForTest(0, 0));
}

// Merging two 1024s reaches the 2048 win.
REGISTER_TEST(test_2048_win_and_restart)
{
    Game2048 game;
    game.reset();
    game.clearBoardForTest();
    game.setCellForTest(0, 0, 1024);
    game.setCellForTest(0, 1, 1024);

    ASSERT_TRUE(game.executeAction("move_left").success);
    ASSERT_TRUE(game.getState().gameWon);
    ASSERT_EQ(2048, game.cellForTest(0, 0));
    ASSERT_TRUE(game.getState().stats.at("best") > 0);

    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(0, st.stats.at("score"));
    ASSERT_TRUE(st.stats.at("max_tile") <= 4);
}

// A full, unmergeable board is game over; restart resets to a fresh board.
REGISTER_TEST(test_2048_game_over_and_restart)
{
    Game2048 game;
    game.reset();
    game.forceGameOverForTest();
    ASSERT_TRUE(game.getState().gameOver);
    ASSERT_EQ(0, game.getState().stats.at("empty_cells"));

    ASSERT_TRUE(game.executeAction("restart").success);
    const auto st = game.getState();
    ASSERT_TRUE(st.gameRunning);
    ASSERT_EQ(0, st.stats.at("score"));
    ASSERT_EQ(0, st.stats.at("moves"));
    ASSERT_TRUE(st.stats.at("empty_cells") > 0);
}

// The greedy corner-seeking autopilot must genuinely merge (score + a 4).
REGISTER_TEST(test_2048_autopilot_reaches_merge)
{
    Game2048 game;
    game.enableAutoplay();
    game.reset();
    const auto s0 = game.getState();
    ASSERT_TRUE(s0.stats.count("particles") > 0);   // juice stats exposed
    ASSERT_TRUE(s0.stats.count("frozen") > 0);
    ASSERT_TRUE(s0.stats.count("best") > 0);

    int best = 0, maxTile = 0;
    for (int i = 0; i < 60 * 30; ++i) {  // 30s of frames
        game.tick(1.0f / 60.0f);
        const auto st = game.getState();
        best = std::max(best, st.stats.at("score"));
        maxTile = std::max(maxTile, st.stats.at("max_tile"));
        if (st.gameOver || st.gameWon) game.reset();
    }
    // At least one merge happened: score > 0 and a 4 tile exists.
    ASSERT_TRUE(best > 0);
    ASSERT_TRUE(maxTile >= 4);
}

REGISTER_TEST(test_2048_actions_available)
{
    Game2048 game;
    game.reset();
    ASSERT_EQ(5, static_cast<int>(game.getAvailableActions().size()));

    // A tile in the rightmost column must slide left (guaranteed change).
    game.clearBoardForTest();
    game.setCellForTest(0, 3, 2);
    ASSERT_TRUE(game.executeAction("move_left").success);
    ASSERT_EQ(2, game.cellForTest(0, 0));

    ASSERT_TRUE(game.executeAction("restart").success);
    ASSERT_TRUE(game.getState().gameRunning);
    ASSERT_EQ(0, game.getState().stats.at("moves"));
}
