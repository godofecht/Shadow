// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Whack-a-Mole - the arcade classic, game #16 of the 100-game program.
//
// Moles pop up from a 3x3 warren at a pace that ramps as you score. Whack
// them before they hide: every hit pays 10 x combo and detonates a burst
// of dirt with shake, hit-stop, and a rising "+N". A mole that escapes
// costs a life and resets your combo; lose all three lives and the game
// ends with a gold "NEW BEST!" celebration on a session record. Reach 25
// whacks to win.
//
// Shipped with the GameJuice kit from day one (Engine/Core/GameJuice.h) -
// whack bursts, mole-pop puffs, escape misses, the game-over explosion,
// and the win fanfare are all synthesized in memory, identical native /
// WASM / headless. Spawns flow through a fixed-seed LCG, so every run is
// deterministic and unit-testable. Pause (P) and a session best included.
//
// One code path serves human input and the LLM: mouse clicks, the arrow
// keys + Space/Enter, and the move/whack actions all drive the exact same
// hole cursor. getState() exposes the warren (0 empty / 1 mole up), the
// mole timers, score/best/lives/combo and the cursor, so an agent can play
// like a human - the moles it can see are exactly the moles in state.
//
// The smoke autopilot reads the visible warren and whacks any mole that is
// up, walking the cursor toward the nearest one. With the engine's smoke
// auto-restart it loops through hits, escapes, game overs, and wins for
// its whole window.
//
// Controls: click a hole (or arrows + Space/Enter) | P pause | R restart.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

class WhackAMole : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int WINDOW_W = 640;
    static constexpr int WINDOW_H = 640;
    static constexpr int TOP_BAND = 36;        // HUD strip
    static constexpr int COLS = 3;
    static constexpr int ROWS = 3;
    static constexpr int HOLE_W = 180;
    static constexpr int HOLE_H = 170;
    static constexpr int GAP_X = 20;
    static constexpr int GAP_Y = 14;
    static constexpr int WIN_SCORE = 25;       // whacks to beat the game
    static constexpr int START_LIVES = 3;
    static constexpr float BASE_VISIBLE = 2.0f; // mole stays up (easy)
    static constexpr float MIN_VISIBLE = 0.8f;  // mole stays up (hard)
    static constexpr float BASE_SPAWN = 0.9f;   // gap between pops (easy)
    static constexpr float MIN_SPAWN = 0.4f;    // gap between pops (hard)
    static constexpr float BOT_INTERVAL = 0.18f;

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- State ------------------------------------------------------------
    bool moleUp[ROWS][COLS] = {};     // a mole is showing
    float moleTimer[ROWS][COLS] = {}; // seconds left before it hides
    int score = 0;                    // whacks landed (score target)
    int bestScore = 0;                // session best; survives restarts
    int lives = START_LIVES;
    int combo = 0;
    int bestCombo = 0;
    int curRow = 0;                   // cursor (LLM + keyboard path)
    int curCol = 0;
    float spawnTimer = BASE_SPAWN;
    float botTimer = 0.0f;

    // Fixed-seed LCG: spawns are deterministic and unit-testable.
    uint32_t lcgState = 0x9E3779B9u;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;
    std::string statusText;

public:
    WhackAMole() : Game2D("Whack-a-Mole", WINDOW_W, WINDOW_H, 20) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hook: land whacks back-to-back until the win path fires.
    // Not used by gameplay.
    void forceWinForTest() {
        for (int i = 0; i < WIN_SCORE + 1 && gameRunning; ++i) {
            score += 10;
            bestScore = std::max(bestScore, score);
            if (score >= WIN_SCORE * 10) {
                winGame();
                return;
            }
        }
    }

    void initGame() override {
        score = 0;
        paused = false;
        lives = START_LIVES;
        combo = 0;
        curRow = 0;
        curCol = 0;
        spawnTimer = BASE_SPAWN;
        botTimer = 0.0f;
        particles.clear();
        floatTexts = uj::FloatingText{};
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c) {
                moleUp[r][c] = false;
                moleTimer[r][c] = 0.0f;
            }

        hud = createText(10, 8, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, WINDOW_H - 22, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("Whack the moles before they hide!");

        registerAction("move_up", [this]() { return moveCursor(-1, 0); });
        registerAction("move_down", [this]() { return moveCursor(1, 0); });
        registerAction("move_left", [this]() { return moveCursor(0, -1); });
        registerAction("move_right", [this]() { return moveCursor(0, 1); });
        registerAction("whack", [this]() { return whackAction(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindMouse(MOUSE_LEFT).onPress([this]() { handleClick(); });
        bindKey(KEY_UP).onPress([this]() { (void)moveCursor(-1, 0); });
        bindKey(KEY_DOWN).onPress([this]() { (void)moveCursor(1, 0); });
        bindKey(KEY_LEFT).onPress([this]() { (void)moveCursor(0, -1); });
        bindKey(KEY_RIGHT).onPress([this]() { (void)moveCursor(0, 1); });
        bindKey(KEY_SPACE).onPress([this]() { (void)whackAtCursor(); });
        bindKey(KEY_ENTER).onPress([this]() { (void)whackAtCursor(); });
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

        // Spawn the next mole (one at a time, ramping to a possible second).
        spawnTimer -= dt;
        if (spawnTimer <= 0.0f) {
            spawnTimer = spawnDelay();
            spawnMole();
            // Higher scores sometimes have two moles up at once.
            if (score >= 12 && (lcgNext() % 100) < 35) spawnMole();
        }

        // Moles hide on their own: each escape costs a life.
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                if (!moleUp[r][c]) continue;
                moleTimer[r][c] -= dt;
                if (moleTimer[r][c] <= 0.0f) {
                    moleEscaped(r, c);
                }
            }
        }

        // Autopilot: whack any mole that is up.
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
        SDL_Rect band = {0, 0, WINDOW_W, TOP_BAND};
        SDL_RenderFillRect(sdl, &band);

        // The warren (shakes).
        SDL_SetRenderDrawColor(sdl, 26, 32, 24, 255);
        SDL_Rect lawn = {8 + sx, TOP_BAND + 6 + sy, WINDOW_W - 16,
                         WINDOW_H - TOP_BAND - 12};
        SDL_RenderFillRect(sdl, &lawn);

        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                const int x = holeX(c) + sx;
                const int y = holeY(r) + sy;
                drawHole(sdl, x, y, r, c);
            }
        }

        particles.render(sdl, sx, sy);
        floatTexts.render(getRenderer());

        if (paused) {
            SDL_Rect veil = {0, 0, WINDOW_W, WINDOW_H};
            SDL_SetRenderDrawColor(sdl, 0, 0, 0, 140);
            SDL_RenderFillRect(sdl, &veil);
        }

        // Message strip at the bottom (stable).
        SDL_SetRenderDrawColor(sdl, 12, 14, 22, 180);
        SDL_Rect strip = {0, WINDOW_H - 30, WINDOW_W, 30};
        SDL_RenderFillRect(sdl, &strip);

        for (auto& t : textDisplays) t->render(getRenderer());
    }

    // ---- LLM state ----------------------------------------------------------
    GameState getState() const override {
        GameState state = Game2D::getState();
        state.score = score;
        state.level = 1;
        state.message = statusText;
        state.gridWidth = COLS;
        state.gridHeight = ROWS;
        state.grid.assign(static_cast<std::size_t>(ROWS),
                          std::vector<int>(static_cast<std::size_t>(COLS), 0));
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c)
                state.grid[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] =
                    moleUp[r][c] ? 1 : 0;
        state.stats["score"] = score;
        state.stats["best"] = std::max(bestScore, score);
        state.stats["target"] = WIN_SCORE * 10;
        state.stats["lives"] = lives;
        state.stats["combo"] = combo;
        state.stats["best_combo"] = std::max(bestCombo, combo);
        state.stats["cursor_row"] = curRow;
        state.stats["cursor_col"] = curCol;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c) {
                state.stats["hole_" + std::to_string(r) + "_" +
                           std::to_string(c)] = moleUp[r][c] ? 1 : 0;
                state.stats["timer_" + std::to_string(r) + "_" +
                           std::to_string(c)] =
                    static_cast<int>(moleTimer[r][c] * 10.0f);
            }
        return state;
    }

private:
    // ---- Geometry -------------------------------------------------------------
    static int holeX(int c) {
        const int gridW = COLS * HOLE_W + (COLS - 1) * GAP_X;
        return (WINDOW_W - gridW) / 2 + c * (HOLE_W + GAP_X);
    }

    static int holeY(int r) {
        const int gridH = ROWS * HOLE_H + (ROWS - 1) * GAP_Y;
        return TOP_BAND + (WINDOW_H - TOP_BAND - gridH) / 2 + r * (HOLE_H + GAP_Y);
    }

    // ---- Spawning --------------------------------------------------------------
    float visibleTime() const {
        const float t = BASE_VISIBLE -
                        (BASE_VISIBLE - MIN_VISIBLE) *
                            static_cast<float>(score) / 250.0f;
        return std::max(MIN_VISIBLE, t);
    }

    float spawnDelay() const {
        const float t = BASE_SPAWN -
                        (BASE_SPAWN - MIN_SPAWN) *
                            static_cast<float>(score) / 250.0f;
        return std::max(MIN_SPAWN, t);
    }

    void spawnMole() {
        // Pick a random empty hole, restarting the sweep if full.
        for (int tries = 0; tries < 16; ++tries) {
            const int r = static_cast<int>(lcgNext() % ROWS);
            const int c = static_cast<int>(lcgNext() % COLS);
            if (!moleUp[r][c]) {
                moleUp[r][c] = true;
                moleTimer[r][c] = visibleTime();
                sfx.play(uj::Sfx::Ping);
                particles.burst((float)holeX(c) + HOLE_W / 2.0f,
                                (float)holeY(r) + HOLE_H / 2.0f, 4,
                                {120, 90, 60, 255}, 3.0f, 0.25f, 2.5f);
                return;
            }
        }
    }

    // ---- Whack / escape ----------------------------------------------------------
    void whack(int r, int c) {
        if (!gameRunning || paused) return;
        if (!moleUp[r][c]) return;   // whiff: empty hole, no penalty

        const int cx = holeX(c) + HOLE_W / 2;
        const int cy = holeY(r) + HOLE_H / 2;
        moleUp[r][c] = false;
        moleTimer[r][c] = 0.0f;
        ++combo;
        bestCombo = std::max(bestCombo, combo);
        const int gain = 10 * combo;
        score += gain;

        // ---- Juice: whack! -----------------------------------------------------
        sfx.play(uj::Sfx::Thock);
        shake.add(0.12f);
        hitStop.trigger(0.03f);
        particles.burst((float)cx, (float)cy, 10, {160, 120, 70, 255},
                        6.0f, 0.35f, 3.5f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
                             cx - 24, cy - 30,
                             "+" + std::to_string(gain) +
                                 (combo > 1 ? " x" + std::to_string(combo) : "")),
                         cx - 24, cy - 30);

        if (score >= WIN_SCORE * 10) {
            winGame();
            return;
        }
        updateHUD();
        setMessage("Whack! Combo x" + std::to_string(combo) +
                   " - " + std::to_string(WIN_SCORE * 10 - score) +
                   " points to win");
    }

    void moleEscaped(int r, int c) {
        moleUp[r][c] = false;
        moleTimer[r][c] = 0.0f;
        combo = 0;
        --lives;

        // ---- Juice: the mole got away -------------------------------------------
        sfx.play(uj::Sfx::Lose);
        shake.add(0.25f);
        const int cx = holeX(c) + HOLE_W / 2;
        const int cy = holeY(r) + HOLE_H / 2;
        particles.burst((float)cx, (float)cy, 8, {210, 160, 100, 255},
                        4.0f, 0.4f, 3.0f);
        floatTexts.spawn(std::make_shared<TextDisplay>(cx - 20, cy - 24, "MISS"),
                         cx - 20, cy - 24);

        if (lives <= 0) {
            loseGame();
            return;
        }
        updateHUD();
        setMessage("Mole escaped! " + std::to_string(lives) +
                   " lives left - combo reset");
    }

    // ---- Win / lose -------------------------------------------------------------
    void loseGame() {
        const bool newBest = score > bestScore;
        bestScore = std::max(bestScore, score);
        // ---- Juice: game over explosion ----------------------------------------
        sfx.play(uj::Sfx::Lose);
        shake.add(0.6f);
        hitStop.trigger(0.15f);
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c)
                if (moleUp[r][c]) {
                    particles.burst((float)holeX(c) + HOLE_W / 2.0f,
                                    (float)holeY(r) + HOLE_H / 2.0f, 12,
                                    {200, 60, 50, 255}, 8.0f, 0.6f, 4.5f);
                    moleUp[r][c] = false;
                }
        if (newBest && score > 0) {
            sfx.play(uj::Sfx::Win);
            particles.burst((float)(WINDOW_W / 2), (float)(TOP_BAND + 200), 14,
                            {230, 200, 60, 255}, 8.0f, 0.7f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                                 WINDOW_W / 2 - 60, TOP_BAND + 120, "NEW BEST!"),
                             WINDOW_W / 2 - 60, TOP_BAND + 120);
        }
        setMessage("GAME OVER - " + std::to_string(score) +
                   " pts, best " + std::to_string(bestScore) +
                   " - Press R to restart");
        updateHUD();
        gameWon = false;
        endGame();
    }

    void winGame() {
        bestScore = std::max(bestScore, score);
        // ---- Juice: fanfare + confetti ------------------------------------------
        sfx.play(uj::Sfx::Win);
        shake.add(0.5f);
        for (int i = 0; i < 4; ++i) {
            particles.burst((float)(WINDOW_W / 2 + (i - 1) * 90),
                            (float)(TOP_BAND + 160), 20,
                            i % 3 == 0 ? SDL_Color{255, 220, 60, 255} :
                            i % 3 == 1 ? SDL_Color{80, 220, 255, 255} :
                                         SDL_Color{140, 255, 120, 255},
                            10.0f, 0.9f, 6.0f);
        }
        floatTexts.spawn(std::make_shared<TextDisplay>(
                             WINDOW_W / 2 - 60, TOP_BAND + 100, "YOU WIN!"),
                         WINDOW_W / 2 - 60, TOP_BAND + 100);
        setMessage("YOU WIN! Best " + std::to_string(bestScore) +
                   " - Press R to play again");
        updateHUD();
        gameWon = true;
        endGame();
    }

    // ---- Autopilot ---------------------------------------------------------------
    void botStep() {
        if (!gameRunning) return;
        // Find the nearest up mole and commit: hop toward it, then whack.
        int bestR = -1, bestC = -1, bestDist = 1000;
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c)
                if (moleUp[r][c]) {
                    const int d = std::abs(r - curRow) + std::abs(c - curCol);
                    if (d < bestDist) {
                        bestDist = d;
                        bestR = r;
                        bestC = c;
                    }
                }
        if (bestR < 0) return;               // nothing up
        if (bestR == curRow && bestC == curCol) {
            whack(curRow, curCol);
            return;
        }
        if (bestR != curRow) {
            moveCursor(bestR > curRow ? 1 : -1, 0);
        } else {
            moveCursor(0, bestC > curCol ? 1 : -1);
        }
    }

    // ---- LLM actions -------------------------------------------------------------
    ActionResult moveCursor(int dr, int dc) {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        const int nr = curRow + dr, nc = curCol + dc;
        if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) {
            result.message = "Cursor is at the edge of the warren";
            return result;
        }
        curRow = nr;
        curCol = nc;
        result.success = true;
        result.message = "Cursor at hole (" + std::to_string(nr) + "," +
                         std::to_string(nc) + ")";
        return result;
    }

    ActionResult whackAction() {
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
        if (!moleUp[curRow][curCol]) {
            result.message = "No mole at the cursor hole";
            return result;
        }
        whack(curRow, curCol);
        result.success = true;
        result.message = "Whacked a mole for +" +
                         std::to_string(10 * combo) + " (combo x" +
                         std::to_string(combo) + ")";
        result.scoreChange = 10 * combo;
        result.gameOver = gameOver;
        result.gameWon = gameWon;
        return result;
    }

    void whackAtCursor() {
        if (!gameRunning || paused) return;
        whack(curRow, curCol);
    }

    // ---- Mouse --------------------------------------------------------------------
    void handleClick() {
        if (!gameRunning || paused) return;
        if (input.mouseY < TOP_BAND || input.mouseY >= WINDOW_H - 30) return;
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c) {
                const int x = holeX(c), y = holeY(r);
                if (input.mouseX >= x && input.mouseX < x + HOLE_W &&
                    input.mouseY >= y && input.mouseY < y + HOLE_H) {
                    curRow = r;
                    curCol = c;
                    whack(r, c);
                    return;
                }
            }
    }

    // ---- Rendering -----------------------------------------------------------------
    void drawHole(SDL_Renderer* sdl, int x, int y, int r, int c) const {
        // Hole mouth (dark ellipse).
        SDL_SetRenderDrawColor(sdl, 40, 30, 22, 255);
        SDL_Rect mouth = {x, y + HOLE_H - 34, HOLE_W, 34};
        SDL_RenderFillRect(sdl, &mouth);
        SDL_SetRenderDrawColor(sdl, 24, 18, 14, 255);
        SDL_Rect lip = {x + 6, y + HOLE_H - 26, HOLE_W - 12, 20};
        SDL_RenderFillRect(sdl, &lip);

        // Cursor ring.
        if (curRow == r && curCol == c && gameRunning && !paused) {
            SDL_SetRenderDrawColor(sdl, 255, 220, 120, 160);
            SDL_Rect ring = {x - 5, y - 5, HOLE_W + 10, HOLE_H + 10};
            SDL_RenderDrawRect(sdl, &ring);
        }

        if (!moleUp[r][c]) return;

        // Mole: body, ears, eyes.
        const int mx = x + HOLE_W / 2;
        const int my = y + HOLE_H - 26;
        SDL_SetRenderDrawColor(sdl, 122, 82, 52, 255);
        SDL_Rect body = {mx - 46, my - 64, 92, 66};
        SDL_RenderFillRect(sdl, &body);
        // Ears.
        SDL_SetRenderDrawColor(sdl, 122, 82, 52, 255);
        SDL_Rect earL = {mx - 46, my - 78, 22, 22};
        SDL_Rect earR = {mx + 24, my - 78, 22, 22};
        SDL_RenderFillRect(sdl, &earL);
        SDL_RenderFillRect(sdl, &earR);
        SDL_SetRenderDrawColor(sdl, 210, 150, 120, 255);
        SDL_Rect inL = {mx - 42, my - 74, 14, 14};
        SDL_Rect inR = {mx + 28, my - 74, 14, 14};
        SDL_RenderFillRect(sdl, &inL);
        SDL_RenderFillRect(sdl, &inR);
        // Eyes (white + pupil).
        SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
        SDL_Rect eyeL = {mx - 32, my - 50, 18, 20};
        SDL_Rect eyeR = {mx + 14, my - 50, 18, 20};
        SDL_RenderFillRect(sdl, &eyeL);
        SDL_RenderFillRect(sdl, &eyeR);
        SDL_SetRenderDrawColor(sdl, 30, 20, 12, 255);
        SDL_Rect pupL = {mx - 27, my - 44, 8, 10};
        SDL_Rect pupR = {mx + 19, my - 44, 8, 10};
        SDL_RenderFillRect(sdl, &pupL);
        SDL_RenderFillRect(sdl, &pupR);
        // Nose + whisker hints.
        SDL_SetRenderDrawColor(sdl, 60, 34, 20, 255);
        SDL_Rect nose = {mx - 9, my - 24, 18, 12};
        SDL_RenderFillRect(sdl, &nose);
    }

    // ---- Juice / HUD ----------------------------------------------------------------
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
        hud->setText("Score " + std::to_string(score) +
                     "/" + std::to_string(WIN_SCORE * 10) +
                     "    Lives " + std::to_string(lives) +
                     "    Combo x" + std::to_string(combo) +
                     "    Best " + std::to_string(std::max(bestScore, score)));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the WhackAMole class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static WhackAMole game;
#else
    WhackAMole game;
#endif
    game.run();
    return 0;
}
#endif
