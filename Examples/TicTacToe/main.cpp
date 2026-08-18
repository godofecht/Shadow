// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Tic Tac Toe - the duel classic, game #2 of the 100-game program,
// retrofitted to the AAA-feel bar with the GameJuice kit.
//
// Two players take turns placing X (blue) and O (red) on a 3x3 board; three
// in a row wins. A full board with no winner is a draw. The session keeps a
// running tally (X wins / O wins / draws) plus the longest consecutive-win
// streak either side has managed.
//
// Feel (GameJuice, Engine/Core/GameJuice.h): every placement pops a burst of
// the player's color with a firm thock (X) or ping (O); a win flashes the
// winning line white with a shake + hit-stop, fires the victory fanfare and a
// confetti burst along the line, and raises a "X WINS!" (with a gold "NEW
// BEST!" celebration when the streak record falls); a draw settles quietly
// with the descend tone and a soft gray burst. All sound is synthesized in
// memory, identical native / WASM / headless. Pause (P) and the session
// streak tally included.
//
// One code path serves human input and the LLM: mouse clicks, cursor keys,
// and the move_up/down/left/right + place actions all call the exact
// doPlace(). getState() reports the board as a grid (0 empty, 1 X, 2 O) plus
// the tally and streak, so an LLM plays the exact game a human plays.
//
// The smoke autopilot plays both sides with a fixed-seed LCG, so a headless
// run keeps exercising placement pops, win flashes, and draws (and the
// engine's smoke restart) for its whole window.
//
// Controls: click a cell or cursor keys + Enter | P pause | R restart.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class TicTacToe : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int WINDOW_W = 700;
    static constexpr int WINDOW_H = 700;
    static constexpr int BOARD_X = 65;       // board origin (top-left)
    static constexpr int BOARD_Y = 90;
    static constexpr int TILE = 190;         // cell size
    static constexpr float BOT_INTERVAL = 0.10f; // autopilot move cadence

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- State ------------------------------------------------------------
    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;
    int board[3][3] = {{0}};                 // 0 empty, 1 X, 2 O
    int currentPlayer = 1;                   // 1 X (blue), 2 O (red)
    int moveCount = 0;
    int cursorX = 0, cursorY = 0;            // keyboard/LLM cursor
    int xWins = 0, oWins = 0, draws = 0;     // session tally; survive restarts
    int winStreak = 0;                       // current consecutive wins
    int bestStreak = 0;                      // session record
    std::pair<int, int> winLine[3];          // the winning three cells
    float winFlashTimer = 0.0f;              // win-line flash decay
    float botTimer = 0.0f;
    std::pair<int, int> botTarget = {0, 0};  // committed autopilot target
    bool hasBotTarget = false;

    // Fixed-seed LCG: the autopilot's cell choices are deterministic and
    // unit-testable. (The board game itself has no randomness.)
    uint32_t lcgState = 0x0A5A5EEDu;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;

public:
    TicTacToe() : Game2D("Tic Tac Toe", WINDOW_W, WINDOW_H, 28) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hook: place for the current player and run the exact post-move
    // logic (win/draw check, juice, tally). Not used by gameplay.
    void placeForTest(int r, int c) { doPlace(r, c); }

    void initGame() override {
        for (auto& row : board)
            for (int& cell : row) cell = 0;
        currentPlayer = 1;
        moveCount = 0;
        cursorX = 0;
        cursorY = 0;
        winFlashTimer = 0.0f;
        paused = false;
        botTimer = 0.0f;
        hasBotTarget = false;
        particles.clear();
        floatTexts = uj::FloatingText{};

        hud = createText(10, 8, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, WINDOW_H - 26, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("Click a cell or cursor keys + Enter | X's turn");

        registerAction("move_up", [this]() {
            cursorY = std::max(0, cursorY - 1);
            return ActionResult{true, "Cursor up"};
        });
        registerAction("move_down", [this]() {
            cursorY = std::min(2, cursorY + 1);
            return ActionResult{true, "Cursor down"};
        });
        registerAction("move_left", [this]() {
            cursorX = std::max(0, cursorX - 1);
            return ActionResult{true, "Cursor left"};
        });
        registerAction("move_right", [this]() {
            cursorX = std::min(2, cursorX + 1);
            return ActionResult{true, "Cursor right"};
        });
        registerAction("place", [this]() { return placeAction(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindMouse(MOUSE_LEFT).onPress([this]() { handleClick(); });
        bindKey(KEY_UP).onPress([this]() {
            cursorY = std::max(0, cursorY - 1);
        });
        bindKey(KEY_DOWN).onPress([this]() {
            cursorY = std::min(2, cursorY + 1);
        });
        bindKey(KEY_LEFT).onPress([this]() {
            cursorX = std::max(0, cursorX - 1);
        });
        bindKey(KEY_RIGHT).onPress([this]() {
            cursorX = std::min(2, cursorX + 1);
        });
        bindKey(KEY_ENTER).onPress([this]() { (void)placeAction(); });
        bindKey(KEY_SPACE).onPress([this]() { (void)placeAction(); });
        bindKey(KEY_P).onPress([this]() { paused = !paused; });
        bindKey(KEY_R).onPress([this]() {
            if (gameOver || gameWon) startGame();
        });
    }

    void updateGame(float dt) override {
        if (!gameRunning) return;

        if (paused) {
            if (message) message->setText("PAUSED - press P to resume");
            return;
        }

        if (hitStop.frozen()) {
            hitStop.update(dt);
            return;
        }

        if (smokeMode) {
            botTimer -= dt;
            if (botTimer <= 0.0f) {
                botTimer = BOT_INTERVAL;
                botStep();
            }
        }

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // HUD band (stable).
        SDL_SetRenderDrawColor(sdl, 12, 14, 22, 255);
        SDL_Rect band = {0, 0, WINDOW_W, 46};
        SDL_RenderFillRect(sdl, &band);

        // Board background + grid (the board shakes).
        SDL_SetRenderDrawColor(sdl, 24, 28, 44, 255);
        SDL_Rect bg = {BOARD_X + sx, BOARD_Y + sy, TILE * 3, TILE * 3};
        SDL_RenderFillRect(sdl, &bg);
        SDL_SetRenderDrawColor(sdl, 60, 66, 96, 255);
        for (int i = 1; i < 3; ++i) {
            const int x = BOARD_X + sx + i * TILE;
            SDL_RenderDrawLine(sdl, x, BOARD_Y + sy, x,
                               BOARD_Y + sy + TILE * 3);
            const int y = BOARD_Y + sy + i * TILE;
            SDL_RenderDrawLine(sdl, BOARD_X + sx, y,
                               BOARD_X + sx + TILE * 3, y);
        }

        // Cursor highlight (thin bright outline).
        SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
        SDL_Rect cur = {BOARD_X + sx + cursorX * TILE + 4,
                        BOARD_Y + sy + cursorY * TILE + 4,
                        TILE - 8, TILE - 8};
        SDL_RenderDrawRect(sdl, &cur);

        // Pieces + win-line flash.
        const float flash = winFlashTimer > 0.0f
            ? winFlashTimer / 0.8f : 0.0f;      // 1 -> 0
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                if (board[r][c] == 0) continue;
                const bool onLine = isOnWinLine(r, c);
                drawPiece(sdl, r, c, board[r][c],
                          onLine ? flash : 0.0f, sx, sy);
            }
        }
        // Thick line through the winning cells while the flash lasts.
        if (winFlashTimer > 0.0f) drawWinLine(sdl, sx, sy);

        particles.render(sdl, sx, sy);
        floatTexts.render(getRenderer());

        if (paused) {
            SDL_Rect veil = {0, 0, WINDOW_W, WINDOW_H};
            SDL_SetRenderDrawColor(sdl, 0, 0, 0, 140);
            SDL_RenderFillRect(sdl, &veil);
        }

        // Message strip at the bottom (stable).
        SDL_SetRenderDrawColor(sdl, 12, 14, 22, 180);
        SDL_Rect strip = {0, WINDOW_H - 34, WINDOW_W, 34};
        SDL_RenderFillRect(sdl, &strip);

        for (auto& t : textDisplays) t->render(getRenderer());
    }

    // ---- LLM state ----------------------------------------------------------
    GameState getState() const override {
        GameState state = Game2D::getState();
        state.level = currentPlayer;
        state.message = statusText;
        state.stats["current_player"] = currentPlayer;
        state.stats["move_count"] = moveCount;
        state.stats["x_wins"] = xWins;
        state.stats["o_wins"] = oWins;
        state.stats["draws"] = draws;
        state.stats["streak"] = winStreak;
        state.stats["best_streak"] = std::max(bestStreak, winStreak);
        state.stats["cursor_x"] = cursorX;
        state.stats["cursor_y"] = cursorY;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.stats["win_flash"] = winFlashTimer > 0.0f ? 1 : 0;
        state.gridWidth = 3;
        state.gridHeight = 3;
        state.grid.assign(3, std::vector<int>(3, 0));
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                state.grid[static_cast<std::size_t>(r)]
                          [static_cast<std::size_t>(c)] = board[r][c];
        return state;
    }

private:
    // ---- The move (one shared code path) --------------------------------------
    void doPlace(int r, int c) {
        if (!gameRunning || paused) return;
        if (r < 0 || r > 2 || c < 0 || c > 2) return;
        if (board[r][c] != 0) return;
        if (moveCount >= 9) return;

        board[r][c] = currentPlayer;
        ++moveCount;

        // ---- Juice: placement pop in the player's color ----------------------
        const SDL_Color col = playerColor(currentPlayer);
        sfx.play(currentPlayer == 1 ? uj::Sfx::Thock : uj::Sfx::Ping);
        particles.burst((float)cellX(c), (float)cellY(r), 6, col,
                        5.0f, 0.35f, 4.0f);

        if (findWin(currentPlayer)) {
            winGame();
            return;
        }
        if (moveCount >= 9) {
            drawGame();
            return;
        }
        currentPlayer = currentPlayer == 1 ? 2 : 1;
        updateHUD();
        setMessage("Player " + std::string(currentPlayer == 1 ? "X" : "O") +
                   "'s turn");
    }

    // ---- Win / draw ------------------------------------------------------------
    void winGame() {
        ++winStreak;
        const bool newBest = winStreak > bestStreak;
        bestStreak = std::max(bestStreak, winStreak);
        if (currentPlayer == 1) ++xWins; else ++oWins;

        // ---- Juice: flash the line, shake, hit-stop, fanfare ------------------
        winFlashTimer = 0.8f;
        sfx.play(uj::Sfx::Win);
        shake.add(0.45f);
        hitStop.trigger(0.12f);
        // Confetti along the winning line's midpoint.
        const int mx = (cellX(winLine[0].second) + cellX(winLine[2].second)) / 2;
        const int my = (cellY(winLine[0].first) + cellY(winLine[2].first)) / 2;
        for (int i = 0; i < 3; ++i) {
            particles.burst((float)mx, (float)my, 14,
                i == 0 ? SDL_Color{255, 220, 60, 255} :
                i == 1 ? SDL_Color{80, 220, 255, 255} :
                         SDL_Color{140, 255, 120, 255},
                8.0f, 0.7f, 5.0f);
        }
        const std::string who = currentPlayer == 1 ? "X" : "O";
        floatTexts.spawn(std::make_shared<TextDisplay>(
            WINDOW_W / 2 - 60, BOARD_Y - 40, who + " WINS!"),
            WINDOW_W / 2 - 60, BOARD_Y - 40);
        if (newBest) {
            particles.burst((float)(WINDOW_W / 2), (float)(BOARD_Y + 60), 14,
                            {230, 200, 60, 255}, 8.0f, 0.7f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                WINDOW_W / 2 - 60, BOARD_Y + 110, "NEW BEST!"),
                WINDOW_W / 2 - 60, BOARD_Y + 110);
        }
        setMessage(who + " WINS! Best streak " + std::to_string(bestStreak) +
                   " - Press R to play again");
        updateHUD();
        gameWon = true;
        endGame();
    }

    void drawGame() {
        ++draws;
        winStreak = 0;                       // a draw breaks the streak
        // ---- Juice: settle quietly -------------------------------------------
        sfx.play(uj::Sfx::Descend);
        shake.add(0.15f);
        particles.burst((float)(WINDOW_W / 2), (float)(BOARD_Y + 285), 12,
                        {160, 165, 185, 255}, 6.0f, 0.5f, 4.0f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            WINDOW_W / 2 - 35, BOARD_Y + 230, "DRAW!"),
            WINDOW_W / 2 - 35, BOARD_Y + 230);
        setMessage("DRAW! - Press R to play again");
        updateHUD();
        endGame();
    }

    // ---- Win detection -----------------------------------------------------------
    bool findWin(int player) {
        // Rows
        for (int r = 0; r < 3; ++r) {
            if (board[r][0] == player && board[r][1] == player &&
                board[r][2] == player) {
                winLine[0] = {r, 0};
                winLine[1] = {r, 1};
                winLine[2] = {r, 2};
                return true;
            }
        }
        // Columns
        for (int c = 0; c < 3; ++c) {
            if (board[0][c] == player && board[1][c] == player &&
                board[2][c] == player) {
                winLine[0] = {0, c};
                winLine[1] = {1, c};
                winLine[2] = {2, c};
                return true;
            }
        }
        // Diagonals
        if (board[0][0] == player && board[1][1] == player &&
            board[2][2] == player) {
            winLine[0] = {0, 0};
            winLine[1] = {1, 1};
            winLine[2] = {2, 2};
            return true;
        }
        if (board[0][2] == player && board[1][1] == player &&
            board[2][0] == player) {
            winLine[0] = {0, 2};
            winLine[1] = {1, 1};
            winLine[2] = {2, 0};
            return true;
        }
        return false;
    }

    bool isOnWinLine(int r, int c) const {
        for (const auto& [wr, wc] : winLine) {
            if (wr == r && wc == c) return true;
        }
        return false;
    }

    // ---- LLM actions -------------------------------------------------------------
    ActionResult placeAction() {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        if (paused) {
            result.message = "Paused (press P to resume)";
            return result;
        }
        if (board[cursorY][cursorX] != 0) {
            result.message = "Cell occupied - move the cursor first";
            return result;
        }
        const int placed = currentPlayer;
        doPlace(cursorY, cursorX);
        result.success = true;
        result.message = std::string("Placed ") +
            (placed == 1 ? "X" : "O") + " at " +
            std::to_string(cursorY) + "," + std::to_string(cursorX);
        result.gameOver = gameOver;
        result.gameWon = gameWon;
        return result;
    }

    // ---- Mouse --------------------------------------------------------------------
    void handleClick() {
        if (!gameRunning || paused) return;
        if (input.mouseY < BOARD_Y || input.mouseY >= BOARD_Y + TILE * 3) return;
        if (input.mouseX < BOARD_X || input.mouseX >= BOARD_X + TILE * 3) return;
        const int c = (input.mouseX - BOARD_X) / TILE;
        const int r = (input.mouseY - BOARD_Y) / TILE;
        doPlace(r, c);
    }

    // ---- Autopilot (both sides, deterministic) --------------------------------------
    void botStep() {
        // Commit to one target cell until it is placed - re-rolling every
        // tick would make the cursor wander instead of ever arriving.
        if (!hasBotTarget) {
            std::vector<std::pair<int, int>> empties;
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    if (board[r][c] == 0) empties.push_back({r, c});
            if (empties.empty()) return;
            botTarget = empties[static_cast<std::size_t>(
                lcgNext() % empties.size())];
            hasBotTarget = true;
        }

        const auto [tr, tc] = botTarget;
        // Walk the cursor toward the target, then place when it arrives.
        if (cursorY < tr) ++cursorY;
        else if (cursorY > tr) --cursorY;
        else if (cursorX < tc) ++cursorX;
        else if (cursorX > tc) --cursorX;
        else {
            if (board[tr][tc] == 0) doPlace(tr, tc);
            hasBotTarget = false;   // placed (or blocked); move on
        }
    }

    // ---- Rendering helpers ------------------------------------------------------------
    int cellX(int c) const { return BOARD_X + c * TILE + TILE / 2; }
    int cellY(int r) const { return BOARD_Y + r * TILE + TILE / 2; }

    static SDL_Color playerColor(int p) {
        return p == 1 ? SDL_Color{100, 200, 255, 255}   // X blue
                      : SDL_Color{255, 100, 100, 255};  // O red
    }

    void drawPiece(SDL_Renderer* sdl, int r, int c, int player,
                   float flash, int sx, int sy) const {
        const SDL_Color base = playerColor(player);
        // Flash blends toward white for the winning line.
        const float f = flash;
        const SDL_Color col = {(uint8_t)(base.r + (255 - base.r) * f),
                               (uint8_t)(base.g + (255 - base.g) * f),
                               (uint8_t)(base.b + (255 - base.b) * f),
                               255};
        SDL_SetRenderDrawColor(sdl, col.r, col.g, col.b, 255);
        const int px = cellX(c) + sx;
        const int py = cellY(r) + sy;
        const int half = TILE * 3 / 10;      // glyph half-size
        if (player == 1) {
            // X: two thick diagonals (3 parallel lines each for weight).
            for (int off = -1; off <= 1; ++off) {
                SDL_RenderDrawLine(sdl, px - half, py - half + off,
                                   px + half, py + half + off);
                SDL_RenderDrawLine(sdl, px + half, py - half + off,
                                   px - half, py + half + off);
            }
        } else {
            // O: a 24-gon ring.
            for (int i = 0; i < 24; ++i) {
                const float a0 = (float)i * 6.2831853f / 24.0f;
                const float a1 = (float)(i + 1) * 6.2831853f / 24.0f;
                SDL_RenderDrawLine(sdl,
                    px + (int)(std::cos(a0) * half),
                    py + (int)(std::sin(a0) * half),
                    px + (int)(std::cos(a1) * half),
                    py + (int)(std::sin(a1) * half));
            }
            // Inner ring so the O reads as a ring, not a disc.
            const int inner = half / 2;
            for (int i = 0; i < 24; ++i) {
                const float a0 = (float)i * 6.2831853f / 24.0f;
                const float a1 = (float)(i + 1) * 6.2831853f / 24.0f;
                SDL_RenderDrawLine(sdl,
                    px + (int)(std::cos(a0) * inner),
                    py + (int)(std::sin(a0) * inner),
                    px + (int)(std::cos(a1) * inner),
                    py + (int)(std::sin(a1) * inner));
            }
        }
    }

    void drawWinLine(SDL_Renderer* sdl, int sx, int sy) const {
        const int x0 = cellX(winLine[0].second) + sx;
        const int y0 = cellY(winLine[0].first) + sy;
        const int x1 = cellX(winLine[2].second) + sx;
        const int y1 = cellY(winLine[2].first) + sy;
        SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
        for (int off = -2; off <= 2; ++off) {
            SDL_RenderDrawLine(sdl, x0, y0 + off, x1, y1 + off);
            SDL_RenderDrawLine(sdl, x0 + off, y0, x1 + off, y1);
        }
    }

    // ---- Juice / HUD ----------------------------------------------------------------
    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
        if (winFlashTimer > 0.0f) winFlashTimer -= dt;
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("X " + std::to_string(xWins) +
                     "   O " + std::to_string(oWins) +
                     "   Draws " + std::to_string(draws) +
                     "    Streak " + std::to_string(winStreak) +
                     "    Best " + std::to_string(std::max(bestStreak, winStreak)));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the TicTacToe class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static TicTacToe game;
#else
    TicTacToe game;
#endif
    game.run();
    return 0;
}
#endif
