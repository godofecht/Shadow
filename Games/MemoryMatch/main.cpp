// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Memory Match - the flip-and-find classic, game #14 of the 100-game program.
//
// A grid of face-down cards hides matching pairs. Flip two cards a turn; a
// match locks them face-up and pays out, a miss flips them back and costs a
// mistake. Three levels (4x4, 6x4, 6x6) ramp the difficulty; clear all three
// to win, or burn your mistake budget and lose. Consecutive matches build a
// combo streak that multiplies the score.
//
// Shipped with the GameJuice kit from day one (Engine/Core/GameJuice.h): a
// flip pops sparks with a ping, a match bursts each card's own color with
// shake + hit-stop + a floating "+N", a miss thocks and resets the streak,
// and a win fires a confetti fanfare - all sound synthesized in memory,
// identical native / WASM / headless. The card layout comes from a fixed-seed
// LCG Fisher-Yates shuffle, so a given board is deterministic and testable.
//
// One code path serves human input and the LLM: mouse clicks, arrow keys +
// Enter, and the move_up/down/left/right + flip actions all call the same
// flipCell(). The LLM sees the board as 0 hidden / 1 face-up / 2 matched -
// card values stay hidden, so an agent must remember like a human.
//
// The smoke autopilot plays with perfect memory: it remembers every card it
// has seen face-up and greedily matches known pairs, so it clears boards
// headlessly without reading the hidden values.
//
// Controls: click a card (or arrows + Enter) | P pause | R restart.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class MemoryMatch : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int WINDOW_W = 640;
    static constexpr int WINDOW_H = 640;
    static constexpr int TILE = 80;
    static constexpr int MAX_LEVEL = 3;
    static constexpr int LEVEL_COLS[3] = {4, 6, 6};
    static constexpr int LEVEL_ROWS[3] = {4, 4, 6};
    static constexpr float MISMATCH_TIME = 0.9f;   // beat before a miss flips back
    static constexpr float BOT_INTERVAL = 0.15f;   // autopilot flip cadence

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
    std::vector<int> values;      // per-cell symbol index (hidden from the LLM)
    std::vector<int> cardState;   // 0 face-down, 1 face-up, 2 matched
    std::vector<int> faceUp;      // indices of the 1..2 cards currently up
    std::vector<int> botMem;      // autopilot memory: value seen, or -1 unknown

    int cols = 4, rows = 4;
    int boardX = 0, boardY = 0;   // pixel origin of the (centered) board
    int cursorX = 2, cursorY = 2;
    int level = 1;
    int score = 0;
    int bestScore = 0;            // session best; survives restarts
    int matchedPairs = 0;
    int combo = 0;                // consecutive-match streak (multiplies score)
    int misses = 0;               // mismatches this level
    bool resolving = false;       // a mismatched pair is showing, about to flip back
    bool pendingLoss = false;     // lose once the current mismatch resolves
    float resolveTimer = 0.0f;
    float botTimer = 0.0f;

    // Fixed-seed LCG: card layout is deterministic and unit-testable.
    uint32_t lcgState = 0x0FACADE5u;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;

public:
    MemoryMatch() : Game2D("Memory Match", WINDOW_W, WINDOW_H, TILE) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hook: expose a card's symbol so tests can locate a pair.
    int cardValueForTest(int r, int c) const {
        return values[static_cast<std::size_t>(r * cols + c)];
    }

    // Test hook: flip an exact card (same code path as click / action).
    void flipForTest(int r, int c) { flipCell(r, c); }

    // Test hook: clear one full level by flipping matching pairs in order
    // (exercises flip -> match -> juice -> level-clear / win headlessly).
    // Returns after exactly one level (or on game over), so a test can call
    // it once per level to reach the win deterministically.
    void clearBoardForTest() {
        const int n = cols * rows;
        const int startLevel = level;
        for (int a = 0; a < n; ++a) {
            if (!gameRunning || level != startLevel) return;
            if (cardState[static_cast<std::size_t>(a)] != 0) continue;
            for (int b = a + 1; b < n; ++b) {
                if (cardState[static_cast<std::size_t>(b)] == 0 &&
                    values[static_cast<std::size_t>(a)] ==
                    values[static_cast<std::size_t>(b)]) {
                    flipCell(a / cols, a % cols);
                    flipCell(b / cols, b % cols);
                    break;
                }
            }
        }
    }

    void initGame() override {
        score = 0;
        bestScore = std::max(bestScore, 0);
        paused = false;
        particles.clear();
        floatTexts = uj::FloatingText{};
        initBoard(1);

        hud = createText(10, 8, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, WINDOW_H - 26, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("Click a card (or arrows + Enter) | P pause | R restart");

        registerAction("move_up", [this]() { return moveCursor(0, -1); });
        registerAction("move_down", [this]() { return moveCursor(0, 1); });
        registerAction("move_left", [this]() { return moveCursor(-1, 0); });
        registerAction("move_right", [this]() { return moveCursor(1, 0); });
        registerAction("flip", [this]() { return flipAtCursor(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindMouse(MOUSE_LEFT).onPress([this]() { handleClick(); });
        bindKey(KEY_UP).onPress([this]() { (void)moveCursor(0, -1); });
        bindKey(KEY_DOWN).onPress([this]() { (void)moveCursor(0, 1); });
        bindKey(KEY_LEFT).onPress([this]() { (void)moveCursor(-1, 0); });
        bindKey(KEY_RIGHT).onPress([this]() { (void)moveCursor(1, 0); });
        bindKey(KEY_ENTER).onPress([this]() { (void)flipAtCursor(); });
        bindKey(KEY_SPACE).onPress([this]() { (void)flipAtCursor(); });
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

        // Resolve a mismatched pair: flip both cards back after the beat.
        if (resolving) {
            resolveTimer -= dt;
            if (resolveTimer <= 0.0f) {
                resolving = false;
                for (int idx : faceUp) cardState[static_cast<std::size_t>(idx)] = 0;
                faceUp.clear();
                if (pendingLoss) {
                    pendingLoss = false;
                    loseGame();
                }
            }
        }

        // Smoke autopilot: a perfect-memory solver.
        if (smokeMode && !resolving && gameRunning) {
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
        // Table felt; the board shakes, HUD and floating text stay stable.
        SDL_Renderer* sdl = getRenderer()->renderer;
        SDL_SetRenderDrawColor(sdl, 22, 40, 32, 255);
        SDL_Rect bg = {0, 0, WINDOW_W, WINDOW_H};
        SDL_RenderFillRect(sdl, &bg);

        const auto [sx, sy] = shake.offset();
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) drawCard(sdl, r, c, sx, sy);
        }

        // Cursor highlight (where the LLM flip action acts).
        SDL_Rect cur = {boardX + cursorX * TILE + sx,
                        boardY + cursorY * TILE + sy, TILE, TILE};
        SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
        SDL_RenderDrawRect(sdl, &cur);

        particles.render(sdl, sx, sy);
        floatTexts.render(getRenderer());

        if (paused) {
            SDL_SetRenderDrawColor(sdl, 0, 0, 0, 140);
            SDL_RenderFillRect(sdl, &bg);
        }

        for (auto& t : textDisplays) t->render(getRenderer());
    }

    // ---- LLM state ----------------------------------------------------------
    GameState getState() const override {
        GameState state = Game2D::getState();
        state.score = score;
        state.level = level;
        state.message = statusText;
        state.stats["score"] = score;
        state.stats["best"] = std::max(bestScore, score);
        state.stats["level"] = level;
        state.stats["pairs_matched"] = matchedPairs;
        state.stats["pairs_total"] = totalPairs();
        state.stats["misses"] = misses;
        state.stats["misses_max"] = mistakesMax();
        state.stats["combo"] = combo;
        state.stats["cursor_x"] = cursorX;
        state.stats["cursor_y"] = cursorY;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        // Board: 0 hidden, 1 face-up, 2 matched. Card values stay hidden so
        // the LLM gets no information leak and must remember like a human.
        state.gridWidth = cols;
        state.gridHeight = rows;
        state.grid.assign(static_cast<std::size_t>(rows),
                          std::vector<int>(static_cast<std::size_t>(cols), 0));
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                state.grid[static_cast<std::size_t>(r)]
                          [static_cast<std::size_t>(c)] =
                    cardState[static_cast<std::size_t>(r * cols + c)];
            }
        }
        state.entities["cursor"] = {cursorX, cursorY};
        return state;
    }

private:
    // ---- Board --------------------------------------------------------------
    int totalPairs() const { return cols * rows / 2; }
    int mistakesMax() const { return totalPairs(); }  // forgiving, scales with size

    void initBoard(int lvl) {
        level = lvl;
        cols = LEVEL_COLS[lvl - 1];
        rows = LEVEL_ROWS[lvl - 1];
        const int n = cols * rows;
        const int pairs = n / 2;

        // Pairs of 0..pairs-1, then a deterministic Fisher-Yates shuffle.
        values.assign(static_cast<std::size_t>(n), 0);
        for (int i = 0; i < pairs; ++i) {
            values[static_cast<std::size_t>(2 * i)] = i;
            values[static_cast<std::size_t>(2 * i + 1)] = i;
        }
        for (int i = n - 1; i > 0; --i) {
            const int j = static_cast<int>(lcgNext() % static_cast<uint32_t>(i + 1));
            std::swap(values[static_cast<std::size_t>(i)],
                      values[static_cast<std::size_t>(j)]);
        }

        cardState.assign(static_cast<std::size_t>(n), 0);
        botMem.assign(static_cast<std::size_t>(n), -1);
        faceUp.clear();
        matchedPairs = 0;
        combo = 0;
        misses = 0;
        resolving = false;
        pendingLoss = false;
        resolveTimer = 0.0f;
        botTimer = 0.0f;
        boardX = (WINDOW_W - cols * TILE) / 2;
        boardY = (WINDOW_H - rows * TILE) / 2;
        cursorX = cols / 2;
        cursorY = rows / 2;
    }

    void flipCell(int r, int c) {
        if (!gameRunning || paused || resolving) return;
        if (r < 0 || r >= rows || c < 0 || c >= cols) return;
        const int idx = r * cols + c;
        if (cardState[static_cast<std::size_t>(idx)] != 0) return;

        cardState[static_cast<std::size_t>(idx)] = 1;
        faceUp.push_back(idx);

        // ---- Juice: flip pop + a soft ping --------------------------------
        particles.burst((float)pixX(idx), (float)pixY(idx), 3,
                        {220, 230, 255, 255}, 3.0f, 0.3f, 3.0f);
        sfx.play(uj::Sfx::Ping);

        if (faceUp.size() == 1) return;

        const int a = faceUp[0], b = faceUp[1];
        if (values[static_cast<std::size_t>(a)] ==
            values[static_cast<std::size_t>(b)]) {
            // ---- Match: lock both, score with the streak multiplier --------
            cardState[static_cast<std::size_t>(a)] = 2;
            cardState[static_cast<std::size_t>(b)] = 2;
            ++matchedPairs;
            ++combo;
            const int gain = 10 * combo;
            score += gain;
            faceUp.clear();

            sfx.play(uj::Sfx::Coin);
            shake.add(0.18f);
            hitStop.trigger(0.04f);
            const SDL_Color col = symbolColor(values[static_cast<std::size_t>(a)]);
            particles.burst((float)pixX(a), (float)pixY(a), 12, col,
                            8.0f, 0.5f, 5.0f);
            particles.burst((float)pixX(b), (float)pixY(b), 12, col,
                            8.0f, 0.5f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                pixX(a) - 14, pixY(a) - 20, "+" + std::to_string(gain)),
                pixX(a) - 14, pixY(a) - 20);
            if (combo >= 2) {
                floatTexts.spawn(std::make_shared<TextDisplay>(
                    pixX(b) - 20, pixY(b) + TILE / 2,
                    "STREAK x" + std::to_string(combo)),
                    pixX(b) - 20, pixY(b) + TILE / 2);
            }
            updateHUD();
            if (matchedPairs == totalPairs()) levelCleared();
            return;
        }

        // ---- Miss: reset the streak, cost a mistake, flip back soon --------
        combo = 0;
        ++misses;
        sfx.play(uj::Sfx::Thock);
        shake.add(0.10f);
        resolving = true;
        resolveTimer = MISMATCH_TIME;
        pendingLoss = (misses >= mistakesMax());
        updateHUD();
    }

    void levelCleared() {
        const int bonus = 50 * level;
        score += bonus;
        sfx.play(uj::Sfx::Win);
        shake.add(0.4f);
        for (int i = 0; i < 3; ++i) {
            particles.burst((float)(boardX + (i + 1) * cols * TILE / 4),
                            (float)(boardY + rows * TILE / 2), 18,
                            i == 0 ? SDL_Color{255, 220, 60, 255} :
                            i == 1 ? SDL_Color{80, 220, 255, 255} :
                                     SDL_Color{140, 255, 120, 255},
                            9.0f, 0.8f, 6.0f);
        }
        floatTexts.spawn(std::make_shared<TextDisplay>(
            boardX + cols * TILE / 2 - 60, boardY + rows * TILE / 2 - 30,
            "LEVEL " + std::to_string(level) + " CLEAR! +" +
                std::to_string(bonus)),
            boardX + cols * TILE / 2 - 60, boardY + rows * TILE / 2 - 30);
        if (level >= MAX_LEVEL) {
            winGame();
        } else {
            setMessage("Level " + std::to_string(level) + " clear! " +
                       std::to_string(LEVEL_COLS[level] * LEVEL_ROWS[level] / 2) +
                       " pairs next");
            initBoard(level + 1);
        }
    }

    void winGame() {
        bestScore = std::max(bestScore, score);
        sfx.play(uj::Sfx::Win);
        shake.add(0.5f);
        for (int i = 0; i < 4; ++i) {
            particles.burst((float)(WINDOW_W / 2 + (i - 1) * 90),
                            (float)(WINDOW_H / 2 - 80), 20,
                            i % 3 == 0 ? SDL_Color{255, 220, 60, 255} :
                            i % 3 == 1 ? SDL_Color{80, 220, 255, 255} :
                                         SDL_Color{140, 255, 120, 255},
                            10.0f, 0.9f, 6.0f);
        }
        floatTexts.spawn(std::make_shared<TextDisplay>(
            WINDOW_W / 2 - 60, WINDOW_H / 2 - 100, "YOU WIN!"),
            WINDOW_W / 2 - 60, WINDOW_H / 2 - 100);
        setMessage("YOU WIN! Best " + std::to_string(bestScore) +
                   " - Press R to play again");
        updateHUD();
        gameWon = true;
        endGame();
    }

    void loseGame() {
        bestScore = std::max(bestScore, score);
        sfx.play(uj::Sfx::Lose);
        shake.add(0.5f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            WINDOW_W / 2 - 60, WINDOW_H / 2 - 100, "GAME OVER"),
            WINDOW_W / 2 - 60, WINDOW_H / 2 - 100);
        setMessage("GAME OVER - " + std::to_string(misses) +
                   " misses - Press R to restart");
        updateHUD();
        endGame();
    }

    // ---- Autopilot (perfect memory) -----------------------------------------
    void rememberSeen() {
        for (int i = 0; i < cols * rows; ++i) {
            if (cardState[static_cast<std::size_t>(i)] != 0) {
                botMem[static_cast<std::size_t>(i)] =
                    values[static_cast<std::size_t>(i)];
            }
        }
    }

    int findKnownPartner(int value, int exclude) const {
        for (int i = 0; i < cols * rows; ++i) {
            if (i == exclude) continue;
            if (cardState[static_cast<std::size_t>(i)] == 0 &&
                botMem[static_cast<std::size_t>(i)] == value) {
                return i;
            }
        }
        return -1;
    }

    int pickUnseen(int exclude = -1) const {
        for (int i = 0; i < cols * rows; ++i) {
            if (i == exclude) continue;
            if (cardState[static_cast<std::size_t>(i)] == 0 &&
                botMem[static_cast<std::size_t>(i)] == -1) {
                return i;
            }
        }
        for (int i = 0; i < cols * rows; ++i) {
            if (i == exclude) continue;
            if (cardState[static_cast<std::size_t>(i)] == 0) return i;
        }
        return -1;
    }

    void botStep() {
        rememberSeen();
        int idx = -1;
        if (faceUp.size() == 1) {
            const int a = faceUp[0];
            const int partner =
                findKnownPartner(values[static_cast<std::size_t>(a)], a);
            idx = partner != -1 ? partner : pickUnseen(a);
        } else if (faceUp.empty()) {
            // Prefer finishing a known pair; otherwise reveal something new.
            for (int i = 0; i < cols * rows; ++i) {
                if (cardState[static_cast<std::size_t>(i)] != 0 ||
                    botMem[static_cast<std::size_t>(i)] == -1) {
                    continue;
                }
                if (findKnownPartner(botMem[static_cast<std::size_t>(i)], i) != -1) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) idx = pickUnseen();
        }
        if (idx != -1) {
            cursorX = idx % cols;
            cursorY = idx / cols;
            flipCell(cursorY, cursorX);
        }
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
        cursorX = std::clamp(cursorX + dx, 0, cols - 1);
        cursorY = std::clamp(cursorY + dy, 0, rows - 1);
        result.success = true;
        result.message = "Cursor at (" + std::to_string(cursorX) + ", " +
                         std::to_string(cursorY) + ")";
        return result;
    }

    ActionResult flipAtCursor() {
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
        if (resolving) {
            result.message = "Cards are still showing - wait a moment";
            return result;
        }
        const int idx = cursorY * cols + cursorX;
        if (cardState[static_cast<std::size_t>(idx)] != 0) {
            result.message = cardState[static_cast<std::size_t>(idx)] == 2
                ? "Already matched"
                : "Already face-up";
            return result;
        }
        flipCell(cursorY, cursorX);
        result.success = true;
        result.message = "Flipped (" + std::to_string(cursorX) + ", " +
                         std::to_string(cursorY) + ")";
        return result;
    }

    // ---- Mouse ---------------------------------------------------------------
    void handleClick() {
        if (!gameRunning || gameOver || gameWon) return;
        const int gx = (input.mouseX - boardX) / TILE;
        const int gy = (input.mouseY - boardY) / TILE;
        if (gx < 0 || gx >= cols || gy < 0 || gy >= rows) return;
        cursorX = gx;
        cursorY = gy;
        (void)flipAtCursor();
    }

    // ---- Rendering helpers ----------------------------------------------------
    int pixX(int idx) const { return boardX + (idx % cols) * TILE + TILE / 2; }
    int pixY(int idx) const { return boardY + (idx / cols) * TILE + TILE / 2; }

    static SDL_Color symbolColor(int symbol) {
        static const SDL_Color palette[18] = {
            {220, 60, 60, 255},   {60, 160, 255, 255}, {60, 200, 90, 255},
            {240, 200, 40, 255},  {200, 90, 240, 255}, {255, 130, 40, 255},
            {40, 220, 200, 255},  {240, 80, 160, 255}, {120, 200, 60, 255},
            {90, 90, 240, 255},   {240, 160, 60, 255}, {60, 220, 140, 255},
            {160, 90, 220, 255},  {255, 90, 90, 255},  {90, 180, 255, 255},
            {200, 220, 60, 255},  {255, 120, 200, 255},{80, 240, 180, 255},
        };
        return palette[symbol % 18];
    }

    void drawSymbol(SDL_Renderer* sdl, int cx, int cy, int symbol,
                    float bright = 1.0f) const {
        const SDL_Color c = symbolColor(symbol);
        SDL_SetRenderDrawColor(sdl, (Uint8)(c.r * bright), (Uint8)(c.g * bright),
                               (Uint8)(c.b * bright), 255);
        const int R = TILE / 3;
        const int shape = symbol % 6;
        switch (shape) {
            case 0: {  // diamond
                std::vector<SDL_Point> p = {{cx, cy - R}, {cx + R, cy},
                                            {cx, cy + R}, {cx - R, cy}};
                polyline(sdl, p);
                break;
            }
            case 1: {  // circle
                std::vector<SDL_Point> p;
                for (int i = 0; i < 16; ++i) {
                    const float a = 6.2831853f * (float)i / 16.0f;
                    p.push_back({cx + (int)(std::cos(a) * R),
                                 cy + (int)(std::sin(a) * R)});
                }
                polyline(sdl, p);
                break;
            }
            case 2: {  // triangle
                std::vector<SDL_Point> p = {{cx, cy - R}, {cx - R, cy + R},
                                            {cx + R, cy + R}};
                polyline(sdl, p);
                break;
            }
            case 3: {  // square
                SDL_Rect r = {cx - R, cy - R, 2 * R, 2 * R};
                SDL_RenderDrawRect(sdl, &r);
                break;
            }
            case 4: {  // 4-point star / sparkle
                std::vector<SDL_Point> p;
                for (int i = 0; i < 8; ++i) {
                    const float a = 1.5707963f * (float)i - 1.5707963f;
                    const float rad = (i % 2 == 0) ? (float)R : 0.4f * R;
                    p.push_back({cx + (int)(std::cos(a) * rad),
                                 cy + (int)(std::sin(a) * rad)});
                }
                polyline(sdl, p);
                break;
            }
            default: {  // cross
                SDL_Rect h = {cx - R, cy - R / 3, 2 * R, 2 * R / 3};
                SDL_Rect v = {cx - R / 3, cy - R, 2 * R / 3, 2 * R};
                SDL_RenderFillRect(sdl, &h);
                SDL_RenderFillRect(sdl, &v);
                break;
            }
        }
    }

    static void polyline(SDL_Renderer* sdl, std::vector<SDL_Point> p) {
        if (p.size() < 2) return;
        p.push_back(p.front());  // close the shape
        SDL_RenderDrawLines(sdl, p.data(), (int)p.size());
    }

    void drawCard(SDL_Renderer* sdl, int r, int c, int sx, int sy) const {
        const int idx = r * cols + c;
        const int px = boardX + c * TILE + sx;
        const int py = boardY + r * TILE + sy;
        const int inset = 3;
        SDL_Rect card = {px + inset, py + inset, TILE - 2 * inset,
                         TILE - 2 * inset};
        const int st = cardState[static_cast<std::size_t>(idx)];
        if (st == 0) {
            // Face-down back: raised blue bevel with a pale emblem.
            SDL_SetRenderDrawColor(sdl, 48, 74, 140, 255);
            SDL_RenderFillRect(sdl, &card);
            SDL_SetRenderDrawColor(sdl, 92, 124, 196, 255);
            SDL_RenderDrawRect(sdl, &card);
            SDL_SetRenderDrawColor(sdl, 150, 180, 240, 255);
            SDL_Rect emblem = {px + TILE / 2 - 7, py + TILE / 2 - 7, 14, 14};
            SDL_RenderFillRect(sdl, &emblem);
            SDL_SetRenderDrawColor(sdl, 48, 74, 140, 255);
            SDL_Rect hole = {px + TILE / 2 - 3, py + TILE / 2 - 3, 6, 6};
            SDL_RenderFillRect(sdl, &hole);
            return;
        }
        // Face-up (bright) vs matched (green tint); the symbol stays visible.
        const bool matched = (st == 2);
        SDL_SetRenderDrawColor(sdl, matched ? 120 : 236, matched ? 196 : 236,
                               matched ? 132 : 248, 255);
        SDL_RenderFillRect(sdl, &card);
        SDL_SetRenderDrawColor(sdl, matched ? 80 : 180, matched ? 150 : 190,
                               matched ? 100 : 215, 255);
        SDL_RenderDrawRect(sdl, &card);
        drawSymbol(sdl, px + TILE / 2, py + TILE / 2,
                   values[static_cast<std::size_t>(idx)], matched ? 0.6f : 1.0f);
    }

    // ---- Juice / HUD ----------------------------------------------------------
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
                     "    Best " + std::to_string(std::max(bestScore, score)) +
                     "    Lv " + std::to_string(level) +
                     "    Pairs " + std::to_string(matchedPairs) + "/" +
                     std::to_string(totalPairs()) +
                     "    Misses " + std::to_string(misses) + "/" +
                     std::to_string(mistakesMax()));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the MemoryMatch class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static MemoryMatch game;
#else
    MemoryMatch game;
#endif
    game.run();
    return 0;
}
#endif
