// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Minesweeper - the flood-fill classic, game #3 of the 100-game program,
// retrofitted to the AAA-feel bar with the GameJuice kit.
//
// A 20x20 field hides 50 mines. Left-click reveals a cell (a zero-adjacency
// cell floods its empty region), right-click flags a suspected mine. Reveal
// every safe cell to win; hit a mine and the board shakes, hit-stops, and
// bursts - all mines are then revealed.
//
// Feel (GameJuice, Engine/Core/GameJuice.h): every revealed tile pops a few
// sparks with a rate-limited pip, flags thock, a mine detonates with a big
// orange burst + heavy screen shake + hit-stop + a "BOOM!" label, and a win
// fires a confetti fanfare - all sound synthesized in memory, identical
// native / WASM / headless. Pause (P) and a session best score included.
//
// Mine placement flows through a fixed-seed LCG, so a given board is fully
// deterministic and reproducible - for tests, CI, and LLM agents alike.
//
// One code path serves human input and the LLM: mouse clicks and the
// move_up/down/left/right + reveal/flag actions share the exact same reveal()
// and toggleFlag(). The LLM gets the board as a grid (0 hidden, 1..8 revealed
// neighbor count, 9 flagged - mines stay hidden until the end).
//
// Controls: left-click reveal | right-click flag | P pause | R restart.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

class Minesweeper : public Game2D {
    static constexpr int ROWS = 20, COLS = 20;
    static constexpr int MINE_COUNT = 50;

    struct Cell {
        bool mine = false;
        bool revealed = false;
        bool flagged = false;
        int adj = 0;
    };

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- World ------------------------------------------------------------
    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;
    std::vector<std::vector<Cell>> cells;
    int revealedCount = 0;
    int flagsCount = 0;
    int bestScore = 0;           // session best: cells revealed on a win
    int cursorX = COLS / 2, cursorY = ROWS / 2;
    float smokeTimer = 0.0f;
    float revealSfxTimer = 0.0f; // rate-limits the pip during a flood

    // Fixed-seed LCG: mine layout and the smoke sweep are deterministic.
    uint32_t lcgState = 0x51EEEED5u;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;

    Cell& cellAt(int r, int c) {
        return cells[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
    }
    const Cell& cellAt(int r, int c) const {
        return cells[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)];
    }

public:
    Minesweeper() : Game2D("Minesweeper", 700, 700, 35) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Headless-test hook: reveal every safe cell, which wins the board and
    // exercises the win path deterministically. Not used by gameplay.
    void revealAllSafeForTest() {
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                if (!cellAt(r, c).mine && !cellAt(r, c).revealed) reveal(c, r);
            }
        }
    }

    void initGame() override {
        createGrid(COLS, ROWS, tileSize);
        cells.assign(static_cast<std::size_t>(ROWS),
                     std::vector<Cell>(static_cast<std::size_t>(COLS)));

        // Place mines deterministically.
        int placed = 0;
        while (placed < MINE_COUNT) {
            const int r = (int)(lcgNext() % static_cast<uint32_t>(ROWS));
            const int c = (int)(lcgNext() % static_cast<uint32_t>(COLS));
            if (!cellAt(r, c).mine) {
                cellAt(r, c).mine = true;
                ++placed;
            }
        }
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                if (!cellAt(r, c).mine) cellAt(r, c).adj = countNeighbors(r, c);
            }
        }

        revealedCount = 0;
        flagsCount = 0;
        cursorX = COLS / 2;
        cursorY = ROWS / 2;
        paused = false;
        smokeTimer = 0.0f;
        revealSfxTimer = 0.0f;
        particles.clear();
        floatTexts = uj::FloatingText{};

        hud = createText(10, 6, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, 700 - 26, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("Left-click reveal | Right-click flag | P pause | R restart");

        registerAction("move_up", [this]() { return moveCursor(0, -1); });
        registerAction("move_down", [this]() { return moveCursor(0, 1); });
        registerAction("move_left", [this]() { return moveCursor(-1, 0); });
        registerAction("move_right", [this]() { return moveCursor(1, 0); });
        registerAction("reveal", [this]() { return revealAtCursor(); });
        registerAction("flag", [this]() { return flagAtCursor(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindMouse(MOUSE_LEFT).onPress([this]() { handleReveal(); });
        bindMouse(MOUSE_RIGHT).onPress([this]() { handleFlag(); });
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

        if (revealSfxTimer > 0.0f) revealSfxTimer -= dt;

        // Headless smoke mode: sweep the board row-major from the cursor,
        // revealing cells until a mine detonates. The engine auto-restarts on
        // game over, so the dummy-driver run keeps exercising reveal -> pop ->
        // explosion -> shake -> restart for its whole window.
        if (smokeMode) {
            smokeTimer -= dt;
            if (smokeTimer <= 0.0f) {
                smokeTimer = 0.03f;   // ~33 reveals/s
                (void)smokeStep();
            }
        }

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        // The board shakes with the world; HUD and floating text do not.
        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                const Cell& cell = cellAt(r, c);
                const int px = c * tileSize + sx;
                const int py = r * tileSize + sy;
                SDL_Rect tile = {px, py, tileSize, tileSize};

                if (cell.revealed) {
                    // Revealed: flat light tile (mines show after a loss).
                    SDL_SetRenderDrawColor(sdl, 195, 195, 215, 255);
                    SDL_RenderFillRect(sdl, &tile);
                    SDL_SetRenderDrawColor(sdl, 160, 160, 185, 255);
                    SDL_RenderDrawRect(sdl, &tile);
                    if (cell.mine) drawMine(sdl, px, py);
                } else {
                    // Hidden: raised bevel.
                    SDL_SetRenderDrawColor(sdl, 140, 140, 165, 255);
                    SDL_RenderFillRect(sdl, &tile);
                    SDL_SetRenderDrawColor(sdl, 110, 110, 135, 255);
                    SDL_RenderDrawRect(sdl, &tile);
                    if (cell.flagged) drawFlag(sdl, px, py);
                }
            }
        }

        // Cursor highlight (also where the LLM reveal/flag actions act).
        SDL_Rect cur = {cursorX * tileSize + sx, cursorY * tileSize + sy,
                        tileSize, tileSize};
        SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
        SDL_RenderDrawRect(sdl, &cur);

        // Numbers on revealed cells (classic color coding by count).
        drawNumbers(sx, sy);

        // Particles live in world space (they shake with it); floating text
        // stays screen-stable.
        particles.render(sdl, sx, sy);
        floatTexts.render(getRenderer());

        if (paused) {
            SDL_Rect veil = {0, 0, 700, 700};
            SDL_SetRenderDrawColor(sdl, 0, 0, 0, 140);
            SDL_RenderFillRect(sdl, &veil);
        }

        for (auto& t : textDisplays) t->render(getRenderer());
    }

    // ---- LLM state ----------------------------------------------------------
    GameState getState() const override {
        GameState state = Game2D::getState();
        state.score = revealedCount;
        state.message = statusText;
        state.stats["score"] = revealedCount;
        state.stats["best"] = bestScore;
        state.stats["revealed"] = revealedCount;
        state.stats["flags"] = flagsCount;
        state.stats["mines"] = MINE_COUNT;
        state.stats["safe"] = ROWS * COLS - MINE_COUNT;
        state.stats["cursor_x"] = cursorX;
        state.stats["cursor_y"] = cursorY;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        // The board: 0 hidden, 1..8 revealed neighbor count, 9 flagged.
        // Mines stay 0 until the end, so the LLM gets no information leak.
        state.gridWidth = COLS;
        state.gridHeight = ROWS;
        state.grid.assign(static_cast<std::size_t>(ROWS),
                          std::vector<int>(static_cast<std::size_t>(COLS), 0));
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                const Cell& cell = cellAt(r, c);
                if (cell.flagged) {
                    state.grid[static_cast<std::size_t>(r)]
                              [static_cast<std::size_t>(c)] = 9;
                } else if (cell.revealed && !cell.mine) {
                    state.grid[static_cast<std::size_t>(r)]
                              [static_cast<std::size_t>(c)] = cell.adj;
                }
            }
        }
        state.entities["cursor"] = {cursorX, cursorY};
        return state;
    }

private:
    // ---- Board ---------------------------------------------------------------
    int countNeighbors(int row, int col) const {
        int count = 0;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                const int nr = row + dr, nc = col + dc;
                if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS &&
                    cellAt(nr, nc).mine) {
                    ++count;
                }
            }
        }
        return count;
    }

    void handleReveal() {
        if (gameOver || gameWon) return;
        int gx, gy;
        grid->screenToGrid(input.mouseX, input.mouseY, gx, gy);
        if (!grid->isInBounds(gx, gy)) return;
        reveal(gx, gy);
    }

    void handleFlag() {
        if (gameOver || gameWon) return;
        int gx, gy;
        grid->screenToGrid(input.mouseX, input.mouseY, gx, gy);
        if (!grid->isInBounds(gx, gy)) return;
        toggleFlag(gx, gy);
    }

    void reveal(int x, int y) {
        if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return;
        Cell& cell = cellAt(y, x);
        if (cell.revealed || cell.flagged) return;

        cell.revealed = true;
        ++revealedCount;
        // ---- Juice: tile-reveal pop ----------------------------------------
        revealPop(x, y);

        if (cell.mine) {
            // ---- Juice: the mine detonates ----------------------------------
            particles.burst((float)pixX(x), (float)pixY(y), 30, {255, 90, 30, 255},
                            12.0f, 0.8f, 6.0f);
            particles.burst((float)pixX(x), (float)pixY(y), 14, {255, 220, 60, 255},
                            8.0f, 0.6f, 5.0f);
            shake.add(0.7f);
            hitStop.trigger(0.18f);
            sfx.play(uj::Sfx::Explode);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                pixX(x) - 20, pixY(y) - 22, "BOOM!"),
                pixX(x) - 20, pixY(y) - 22);
            loseGame();
            return;
        }

        // Auto-reveal the empty region (classic flood fill).
        if (cell.adj == 0) {
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr != 0 || dc != 0) reveal(x + dc, y + dr);
                }
            }
        }

        if (revealedCount == ROWS * COLS - MINE_COUNT) winGame();
    }

    void toggleFlag(int x, int y) {
        if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return;
        Cell& cell = cellAt(y, x);
        if (cell.revealed) return;
        cell.flagged = !cell.flagged;
        flagsCount += cell.flagged ? 1 : -1;
        // ---- Juice: flag thock + a red tick --------------------------------
        sfx.play(uj::Sfx::Thock);
        if (cell.flagged) {
            particles.burst((float)pixX(x), (float)pixY(y), 4, {255, 100, 100, 255},
                            3.0f, 0.3f, 3.5f);
        }
        updateHUD();
    }

    void revealPop(int x, int y) {
        particles.burst((float)pixX(x), (float)pixY(y), 3, {215, 225, 245, 255},
                        3.0f, 0.3f, 3.5f);
        if (revealSfxTimer <= 0.0f) {
            sfx.play(uj::Sfx::Ping);
            revealSfxTimer = 0.05f;   // a flood shouldn't machine-gun
        }
    }

    void loseGame() {
        setMessage("BOOM! - Press R to restart");
        endGame();
    }

    void winGame() {
        bestScore = std::max(bestScore, revealedCount);
        // ---- Juice: fanfare + confetti --------------------------------------
        sfx.play(uj::Sfx::Win);
        shake.add(0.45f);
        for (int i = 0; i < 3; ++i) {
            particles.burst(200.0f + (float)i * 150.0f, 260.0f, 20,
                (i == 0) ? SDL_Color{255, 220, 60, 255} :
                (i == 1) ? SDL_Color{80, 220, 255, 255} :
                           SDL_Color{140, 255, 120, 255},
                9.0f, 0.8f, 6.0f);
        }
        floatTexts.spawn(std::make_shared<TextDisplay>(
            250, 120, "YOU WIN!"), 250, 120);
        setMessage("YOU WIN! Best " + std::to_string(bestScore) +
                   " - Press R to play again");
        updateHUD();
        gameWon = true;
        endGame();
    }

    // ---- Smoke autopilot ------------------------------------------------------
    // Row-major sweep from the cursor: reveal the next hidden cell. Returns
    // false once the game is over (a mine detonated and ended the round).
    bool smokeStep() {
        for (int i = 0; i < ROWS * COLS; ++i) {
            const int idx = (cursorY * COLS + cursorX + i) % (ROWS * COLS);
            const int r = idx / COLS, c = idx % COLS;
            if (!cellAt(r, c).revealed && !cellAt(r, c).flagged) {
                cursorX = c;
                cursorY = r;
                reveal(c, r);
                return gameRunning;
            }
        }
        return true;
    }

    // ---- LLM actions ---------------------------------------------------------
    ActionResult moveCursor(int dx, int dy) {
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
        cursorX = std::clamp(cursorX + dx, 0, COLS - 1);
        cursorY = std::clamp(cursorY + dy, 0, ROWS - 1);
        result.success = true;
        result.message = "Cursor at (" + std::to_string(cursorX) + ", " +
                         std::to_string(cursorY) + ")";
        return result;
    }

    ActionResult revealAtCursor() {
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
        const Cell& cell = cellAt(cursorY, cursorX);
        if (cell.revealed) {
            result.message = "Already revealed";
            return result;
        }
        if (cell.flagged) {
            result.message = "Flagged (flag again to unflag first)";
            return result;
        }
        reveal(cursorX, cursorY);
        result.success = true;
        result.message = cell.mine
            ? "BOOM! That was a mine"
            : "Revealed (" + std::to_string(cursorX) + ", " +
              std::to_string(cursorY) + ")";
        return result;
    }

    ActionResult flagAtCursor() {
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
        toggleFlag(cursorX, cursorY);
        result.success = true;
        result.message = cellAt(cursorY, cursorX).flagged
            ? "Flagged (" + std::to_string(cursorX) + ", " +
              std::to_string(cursorY) + ")"
            : "Unflagged";
        return result;
    }

    // ---- Helpers ---------------------------------------------------------------
    int pixX(int tx) const { return tx * tileSize + tileSize / 2; }
    int pixY(int ty) const { return ty * tileSize + tileSize / 2; }

    // ---- Rendering ------------------------------------------------------------
    void drawFlag(SDL_Renderer* sdl, int px, int py) const {
        // A little pixel flag: pole + red pennant + base.
        SDL_SetRenderDrawColor(sdl, 220, 220, 230, 255);
        SDL_Rect pole = {px + tileSize / 2 - 1, py + 6, 3, tileSize - 14};
        SDL_RenderFillRect(sdl, &pole);
        SDL_SetRenderDrawColor(sdl, 240, 60, 60, 255);
        SDL_Rect flag = {px + tileSize / 2 + 1, py + 7,
                         tileSize / 2 - 3, tileSize / 3};
        SDL_RenderFillRect(sdl, &flag);
        SDL_SetRenderDrawColor(sdl, 220, 220, 230, 255);
        SDL_Rect base = {px + tileSize / 2 - 6, py + tileSize - 8,
                         tileSize / 2 + 1, 4};
        SDL_RenderFillRect(sdl, &base);
    }

    void drawMine(SDL_Renderer* sdl, int px, int py) const {
        // A dark cell with a red pixel mine + white spec.
        SDL_SetRenderDrawColor(sdl, 70, 70, 90, 255);
        SDL_Rect body = {px + 6, py + 6, tileSize - 12, tileSize - 12};
        SDL_RenderFillRect(sdl, &body);
        SDL_SetRenderDrawColor(sdl, 220, 40, 40, 255);
        SDL_Rect core = {px + 10, py + 10, tileSize - 20, tileSize - 20};
        SDL_RenderFillRect(sdl, &core);
        SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
        SDL_Rect spec = {px + 12, py + 12, 5, 5};
        SDL_RenderFillRect(sdl, &spec);
    }

    void drawNumbers(int sx, int sy) {
        static SDL_Color numberColors[9] = {
            {0, 0, 0, 255},       // 0 unused
            {0, 90, 220, 255},    // 1 - blue
            {20, 140, 20, 255},   // 2 - green
            {200, 30, 30, 255},   // 3 - red
            {30, 30, 130, 255},   // 4 - dark blue
            {130, 30, 30, 255},   // 5 - dark red
            {20, 120, 120, 255},  // 6 - teal
            {100, 30, 120, 255},  // 7 - purple
            {90, 90, 20, 255},    // 8 - olive
        };
        Renderer* renderer = getRenderer();
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                const Cell& cell = cellAt(r, c);
                if (cell.revealed && !cell.mine && cell.adj > 0) {
                    wchar_t numText[4];
                    swprintf(numText, 4, L"%d", cell.adj);
                    Rect<float> bounds(
                        (float)(c * tileSize + sx + (int)(tileSize * 0.32f)),
                        (float)(r * tileSize + sy + (int)(tileSize * 0.20f)),
                        tileSize * 0.5f, tileSize * 0.5f);
                    renderer->getTextWriter()->drawTextToRenderer(
                        numText, renderer->renderer, bounds, "/default.ttf",
                        numberColors[std::min(cell.adj, 8)]);
                }
            }
        }
    }

    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Cells " + std::to_string(revealedCount) + "/" +
                     std::to_string(ROWS * COLS - MINE_COUNT) +
                     "    Mines " + std::to_string(MINE_COUNT) +
                     "    Flags " + std::to_string(flagsCount) +
                     "    Best " + std::to_string(bestScore));
    }
};

// UMBRA_GAME_NO_MAIN lets tests include this file to reach the Minesweeper
// class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main()
{
#ifdef __EMSCRIPTEN__
    static Minesweeper game;
#else
    Minesweeper game;
#endif
    game.run();
    return 0;
}
#endif
