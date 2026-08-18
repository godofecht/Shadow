// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// 2048 - the slide-and-merge number puzzle, game #22 of the 100-game program.
//
// A 4x4 board of tiles. Swipe (or press an arrow / WASD) to slide every tile
// as far as it goes in that direction; two equal tiles that collide merge
// into their sum. Each move spawns a fresh 2 (or occasionally a 4) in a
// random empty cell. Reach the 2048 tile to win, or fill the board with no
// merges left and lose.
//
// It ships to the AAA-feel bar (see GAMES.md) via Engine/Core/GameJuice.h:
// every merge bursts the tile's color with a pitched chime that rises with
// the value, screen shake and hit-stop scale with the size of the merge, and
// a floating "+N" pops over the result. A spawn pops a small spark, the 2048
// win fires a confetti fanfare, and a loss shakes the board with a falling
// tone. All sound is synthesized in memory - identical native / WASM /
// headless.
//
// One code path serves human input and the LLM: arrow keys, WASD, and swipe
// gestures all call the same doMove() as the move_up/down/left/right
// actions, so an LLM plays the exact game a human plays.
//
// Controls: arrows or WASD = slide, swipe = slide, P = pause, R = restart.
//
// The spawn "randomness" (which empty cell, 2 vs 4) flows through a
// fixed-seed LCG, so a given playthrough is fully deterministic and
// unit-testable. A greedy corner-seeking solver powers PONG_SMOKE autoplay.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Game2048 : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int SIZE = 4;
    static constexpr int WINDOW_W = 520;
    static constexpr int WINDOW_H = 640;
    static constexpr int TILE = 100;             // tile size (px)
    static constexpr int GAP = 12;               // gap between tiles
    static constexpr int BOARD_X = 42;           // centered: (520 - 436)/2
    static constexpr int BOARD_Y = 140;
    static constexpr int WIN_TILE = 2048;
    static constexpr float BOT_INTERVAL = 0.1f;  // autopilot move cadence

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

    int board[SIZE][SIZE];        // 0 = empty, else the tile value (2..2048)
    int score = 0;
    int bestScore = 0;            // session best; survives restarts
    int maxTile = 0;
    int moves = 0;
    bool won = false;

    // Fixed-seed LCG: spawn positions and 2-vs-4 are reproducible.
    uint32_t lcgState = 0x20482048u;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;
    float smokeTimer = 0.0f;

    struct TileStyle { SDL_Color bg, fg; };
    struct Merge { int r, c, value; };
    struct MoveResult {
        bool changed = false;
        int gain = 0;
        std::vector<Merge> merges;
        int b[SIZE][SIZE];
    };

public:
    Game2048() : Game2D("2048", WINDOW_W, WINDOW_H, TILE) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // ---- Test hooks ---------------------------------------------------------
    void clearBoardForTest() {
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                board[r][c] = 0;
        score = 0;
        maxTile = 0;
        moves = 0;
        won = false;
    }
    void setCellForTest(int r, int c, int v) {
        board[r][c] = v;
        maxTile = std::max(maxTile, v);
    }
    int cellForTest(int r, int c) const { return board[r][c]; }
    void forceGameOverForTest() {
        // Fill a full, unmergeable checkerboard, then run the shared path.
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                board[r][c] = ((r + c) % 2 == 0) ? 2 : 4;
        maxTile = 4;
        triggerGameOver();
    }

    void initGame() override {
        score = 0;
        maxTile = 0;
        moves = 0;
        won = false;
        paused = false;
        smokeTimer = 0.0f;
        particles.clear();
        floatTexts = uj::FloatingText{};
        shake = uj::ScreenShake{};
        hitStop = uj::HitStop{};
        lcgState = 0x20482048u;   // deterministic run each reset
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                board[r][c] = 0;
        spawnTile();
        spawnTile();

        hud = createText(20, 16, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(20, WINDOW_H - 30, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("Swipe or arrows/WASD to slide - reach 2048!");

        registerAction("move_up", [this]() { return doMove(2); });
        registerAction("move_down", [this]() { return doMove(3); });
        registerAction("move_left", [this]() { return doMove(0); });
        registerAction("move_right", [this]() { return doMove(1); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_UP).onPress([this]() { (void)doMove(2); });
        bindKey(KEY_DOWN).onPress([this]() { (void)doMove(3); });
        bindKey(KEY_LEFT).onPress([this]() { (void)doMove(0); });
        bindKey(KEY_RIGHT).onPress([this]() { (void)doMove(1); });
        bindKey(KEY_W).onPress([this]() { (void)doMove(2); });
        bindKey(KEY_S).onPress([this]() { (void)doMove(3); });
        bindKey(KEY_A).onPress([this]() { (void)doMove(0); });
        bindKey(KEY_D).onPress([this]() { (void)doMove(1); });
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

        // Smoke autopilot: greedy corner-seeking solver (see autoplayStep).
        if (smokeMode) {
            smokeTimer -= dt;
            if (smokeTimer <= 0.0f) {
                smokeTimer = BOT_INTERVAL;
                autoplayStep();
                if (!gameRunning) return;
            }
        }

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        SDL_Renderer* sdl = getRenderer()->renderer;

        // Backdrop.
        SDL_SetRenderDrawColor(sdl, 44, 40, 34, 255);
        SDL_Rect bg = {0, 0, WINDOW_W, WINDOW_H};
        SDL_RenderFillRect(sdl, &bg);

        // Title (screen space, does not shake).
        drawSimpleText(sdl, BOARD_X, 52, "2048", {238, 228, 218, 255}, 2.2f);

        // The board shakes; the HUD and floating text stay put.
        const auto [sx, sy] = shake.offset();
        SDL_Rect panel = {BOARD_X + sx, BOARD_Y + sy,
                          SIZE * TILE + (SIZE - 1) * GAP,
                          SIZE * TILE + (SIZE - 1) * GAP};
        SDL_SetRenderDrawColor(sdl, 60, 54, 46, 255);
        SDL_RenderFillRect(sdl, &panel);

        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                drawCell(sdl, r, c, sx, sy);

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
        state.level = 1;
        state.message = statusText;
        state.stats["score"] = score;
        state.stats["best"] = std::max(bestScore, score);
        state.stats["max_tile"] = maxTile;
        state.stats["moves"] = moves;
        state.stats["empty_cells"] = emptyCount();
        state.stats["won"] = won ? 1 : 0;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.stats["shake"] = static_cast<int>(shake.level() * 100.0f);
        // Board: 0 = empty, else the tile value (2..2048).
        state.gridWidth = SIZE;
        state.gridHeight = SIZE;
        state.grid.assign(static_cast<std::size_t>(SIZE),
                          std::vector<int>(static_cast<std::size_t>(SIZE), 0));
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                state.grid[static_cast<std::size_t>(r)]
                          [static_cast<std::size_t>(c)] = board[r][c];
        return state;
    }

private:
    // ---- Core slide/merge ---------------------------------------------------
    // Slide a 4-cell line toward index 0, merging equal adjacent tiles. Adds
    // each merged value to `gain` and records the merged tile's final index
    // (in the slid line) into `mergeIdx`.
    static void slideLine(int line[4], int& gain, std::vector<int>& mergeIdx) {
        int tmp[4];
        int n = 0;
        for (int i = 0; i < 4; ++i)
            if (line[i] != 0) tmp[n++] = line[i];

        int merged[4] = {0, 0, 0, 0};
        int m = 0;
        int i = 0;
        while (i < n) {
            if (i + 1 < n && tmp[i] == tmp[i + 1]) {
                merged[m] = tmp[i] * 2;
                gain += merged[m];
                mergeIdx.push_back(m);
                ++m;
                i += 2;
            } else {
                merged[m++] = tmp[i++];
            }
        }
        for (int k = 0; k < 4; ++k) line[k] = merged[k];
    }

    // Simulate a slide in `dir` (0 left, 1 right, 2 up, 3 down) on a copy,
    // reporting whether anything changed, the score gained, and each merge.
    MoveResult simulateMove(int dir) const {
        MoveResult res;
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                res.b[r][c] = board[r][c];

        for (int k = 0; k < SIZE; ++k) {
            int line[4];
            std::vector<int> mergeIdx;
            if (dir == 0) {           // left: row k
                for (int c = 0; c < SIZE; ++c) line[c] = res.b[k][c];
                slideLine(line, res.gain, mergeIdx);
                for (int m : mergeIdx) res.merges.push_back({k, m, line[m]});
                for (int c = 0; c < SIZE; ++c) res.b[k][c] = line[c];
            } else if (dir == 1) {    // right: row k reversed
                for (int c = 0; c < SIZE; ++c) line[c] = res.b[k][SIZE - 1 - c];
                slideLine(line, res.gain, mergeIdx);
                for (int m : mergeIdx) res.merges.push_back({k, SIZE - 1 - m, line[m]});
                for (int c = 0; c < SIZE; ++c) res.b[k][SIZE - 1 - c] = line[c];
            } else if (dir == 2) {    // up: column k top-to-bottom
                for (int r = 0; r < SIZE; ++r) line[r] = res.b[r][k];
                slideLine(line, res.gain, mergeIdx);
                for (int m : mergeIdx) res.merges.push_back({m, k, line[m]});
                for (int r = 0; r < SIZE; ++r) res.b[r][k] = line[r];
            } else {                  // down: column k bottom-to-top
                for (int r = 0; r < SIZE; ++r) line[r] = res.b[SIZE - 1 - r][k];
                slideLine(line, res.gain, mergeIdx);
                for (int m : mergeIdx) res.merges.push_back({SIZE - 1 - m, k, line[m]});
                for (int r = 0; r < SIZE; ++r) res.b[SIZE - 1 - r][k] = line[r];
            }
        }

        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                if (res.b[r][c] != board[r][c]) res.changed = true;
        return res;
    }

    ActionResult doMove(int dir) {
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

        const MoveResult mv = simulateMove(dir);
        if (!mv.changed) {
            result.message = "Nothing moved";
            return result;
        }

        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                board[r][c] = mv.b[r][c];
        score += mv.gain;
        ++moves;
        for (const Merge& mg : mv.merges) {
            maxTile = std::max(maxTile, mg.value);
            mergeJuice(mg.r, mg.c, mg.value);
        }
        updateHUD();
        result.success = true;
        result.scoreChange = mv.gain;
        result.message = std::string("Slid ") + dirName(dir) +
                         (mv.gain > 0 ? " (+" + std::to_string(mv.gain) + ")"
                                      : std::string());

        if (maxTile >= WIN_TILE && !won) {
            won = true;
            winGame();
            return result;
        }

        spawnTile();
        updateHUD();
        if (noMovesLeft()) {
            triggerGameOver();
        }
        return result;
    }

    void spawnTile() {
        std::vector<std::pair<int, int>> empty;
        empty.reserve(SIZE * SIZE);
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                if (board[r][c] == 0) empty.push_back({r, c});
        if (empty.empty()) return;

        const auto [r, c] = empty[lcgNext() % empty.size()];
        const int value = (lcgNext() % 10 == 0) ? 4 : 2;  // 10% four
        board[r][c] = value;
        particles.burst(static_cast<float>(cellCenterX(c)),
                        static_cast<float>(cellCenterY(r)), 4,
                        {240, 235, 225, 255}, 3.0f, 0.3f, 3.0f);
    }

    bool noMovesLeft() const {
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                if (board[r][c] == 0) return false;
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c) {
                if (r + 1 < SIZE && board[r][c] == board[r + 1][c]) return false;
                if (c + 1 < SIZE && board[r][c] == board[r][c + 1]) return false;
            }
        return true;
    }

    // ---- Juice / win / lose ---------------------------------------------------
    static int tileLog2(int value) {
        int lg = 0;
        while ((1 << lg) < value) ++lg;
        return lg;
    }

    static uj::Sfx pitchFor(int lg) {
        if (lg <= 1) return uj::Sfx::Note1;   // 2
        if (lg == 2) return uj::Sfx::Note2;   // 4
        if (lg == 3) return uj::Sfx::Note3;   // 8
        if (lg == 4) return uj::Sfx::Note4;   // 16
        if (lg <= 8) return uj::Sfx::Coin;    // 32..256
        return uj::Sfx::Clear;                // 512+
    }

    void mergeJuice(int r, int c, int value) {
        const int px = cellCenterX(c);
        const int py = cellCenterY(r);
        const int lg = tileLog2(value);
        const TileStyle ts = tileStyle(value);
        particles.burst(static_cast<float>(px), static_cast<float>(py),
                        10 + lg * 2, ts.bg, 7.0f, 0.5f, 5.0f);
        shake.add(0.10f + 0.03f * static_cast<float>(lg));
        hitStop.trigger(value >= 64 ? 0.06f : 0.03f);
        sfx.play(pitchFor(lg));
        floatTexts.spawn(std::make_shared<TextDisplay>(
            px - 24, py - 16, "+" + std::to_string(value)), px - 24, py - 16);
    }

    void winGame() {
        bestScore = std::max(bestScore, score);
        sfx.play(uj::Sfx::Win);
        shake.add(0.6f);
        hitStop.trigger(0.12f);
        for (int i = 0; i < 4; ++i) {
            particles.burst(static_cast<float>(WINDOW_W / 2 + (i - 1) * 90),
                            static_cast<float>(WINDOW_H / 2 - 80), 20,
                            confettiColor(i), 10.0f, 0.9f, 6.0f);
        }
        floatTexts.spawn(std::make_shared<TextDisplay>(
            WINDOW_W / 2 - 40, WINDOW_H / 2 - 120, "2048!"),
            WINDOW_W / 2 - 40, WINDOW_H / 2 - 120);
        setMessage("YOU WIN! Best " + std::to_string(bestScore) +
                   " - Press R to play again");
        updateHUD();
        gameWon = true;
        endGame();
    }

    void triggerGameOver() {
        bestScore = std::max(bestScore, score);
        sfx.play(uj::Sfx::Lose);
        shake.add(0.55f);
        hitStop.trigger(0.12f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            WINDOW_W / 2 - 60, WINDOW_H / 2 - 120, "GAME OVER"),
            WINDOW_W / 2 - 60, WINDOW_H / 2 - 120);
        setMessage("GAME OVER - Best " + std::to_string(bestScore) +
                   " - Press R to restart");
        updateHUD();
        endGame();
    }

    static SDL_Color confettiColor(int i) {
        if (i % 3 == 0) return {255, 220, 60, 255};
        if (i % 3 == 1) return {80, 220, 255, 255};
        return {140, 255, 120, 255};
    }

    // ---- Autopilot ------------------------------------------------------------
    // Greedy one-ply solver: try all four directions, prefer the move with the
    // most immediate merging, and otherwise push tiles toward the bottom-left
    // corner (a stable, non-oscillating preference). Deterministic and fast.
    static long cornerHeuristic(const int b[SIZE][SIZE]) {
        long sc = 0;
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                if (b[r][c] != 0)
                    sc += static_cast<long>(tileLog2(b[r][c])) *
                          (r * SIZE + (SIZE - c));  // bottom-left weighted
        if (b[SIZE - 1][0] != 0)
            sc += static_cast<long>(tileLog2(b[SIZE - 1][0])) * 1000;
        return sc;
    }

    void autoplayStep() {
        int bestDir = -1;
        long bestScoreMove = -1;
        for (int d = 0; d < 4; ++d) {
            const MoveResult mv = simulateMove(d);
            if (!mv.changed) continue;
            const long sc = static_cast<long>(mv.gain) * 1000 +
                            cornerHeuristic(mv.b);
            if (sc > bestScoreMove) {
                bestScoreMove = sc;
                bestDir = d;
            }
        }
        if (bestDir >= 0) (void)doMove(bestDir);
    }

    // ---- Rendering -------------------------------------------------------------
    static const char* dirName(int dir) {
        static const char* names[4] = {"left", "right", "up", "down"};
        return names[dir];
    }

    int cellX(int c) const { return BOARD_X + c * (TILE + GAP); }
    int cellY(int r) const { return BOARD_Y + r * (TILE + GAP); }
    int cellCenterX(int c) const { return cellX(c) + TILE / 2; }
    int cellCenterY(int r) const { return cellY(r) + TILE / 2; }

    static TileStyle tileStyle(int value) {
        switch (value) {
            case 2:    return {{238, 228, 218, 255}, {119, 110, 101, 255}};
            case 4:    return {{237, 224, 200, 255}, {119, 110, 101, 255}};
            case 8:    return {{242, 177, 121, 255}, {249, 246, 242, 255}};
            case 16:   return {{245, 149, 99, 255},  {249, 246, 242, 255}};
            case 32:   return {{246, 124, 95, 255},  {249, 246, 242, 255}};
            case 64:   return {{246, 94, 59, 255},   {249, 246, 242, 255}};
            case 128:  return {{237, 207, 114, 255}, {249, 246, 242, 255}};
            case 256:  return {{237, 204, 97, 255},  {249, 246, 242, 255}};
            case 512:  return {{237, 200, 80, 255},  {249, 246, 242, 255}};
            case 1024: return {{237, 197, 63, 255},  {249, 246, 242, 255}};
            case 2048: return {{237, 194, 46, 255},  {249, 246, 242, 255}};
            default:   return {{60, 58, 50, 255},    {249, 246, 242, 255}};
        }
    }

    void drawCell(SDL_Renderer* sdl, int r, int c, int sx, int sy) const {
        const int px = cellX(c) + sx;
        const int py = cellY(r) + sy;
        SDL_Rect cell = {px, py, TILE, TILE};
        const int value = board[r][c];
        if (value == 0) {
            SDL_SetRenderDrawColor(sdl, 74, 66, 56, 255);
            SDL_RenderFillRect(sdl, &cell);
            return;
        }
        const TileStyle ts = tileStyle(value);
        SDL_SetRenderDrawColor(sdl, ts.bg.r, ts.bg.g, ts.bg.b, 255);
        SDL_RenderFillRect(sdl, &cell);

        const std::string num = std::to_string(value);
        const float scale = value >= 1000 ? 1.4f : (value >= 100 ? 1.8f : 2.4f);
        const int fontPx = static_cast<int>(18.0f * scale);
        const int textW = static_cast<int>(
            static_cast<double>(num.size()) * fontPx * 0.55);
        // Center the label approximately (drawSimpleText is top-left origin).
        const int tx = px + (TILE - textW) / 2;
        const int ty = py + (TILE - fontPx) / 2;
        drawSimpleText(sdl, tx, ty, num, ts.fg, scale);
    }

    // ---- Misc --------------------------------------------------------------------
    int emptyCount() const {
        int n = 0;
        for (int r = 0; r < SIZE; ++r)
            for (int c = 0; c < SIZE; ++c)
                if (board[r][c] == 0) ++n;
        return n;
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
        hud->setText("Score " + std::to_string(score) +
                     "    Best " + std::to_string(std::max(bestScore, score)) +
                     "    Max " + std::to_string(maxTile) +
                     "    Moves " + std::to_string(moves));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the Game2048 class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static Game2048 game;
#else
    Game2048 game;
#endif
    game.run();
    return 0;
}
#endif
