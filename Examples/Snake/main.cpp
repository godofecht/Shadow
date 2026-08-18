// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Snake - the arcade classic, game #1 of the 100-game program, retrofitted
// to the AAA-feel bar with the GameJuice kit.
//
// A green snake slithers across a 25x25 board eating red food, growing one
// segment (and speeding up) per bite. Hit a wall or your own body and it
// bursts; reach the target length to win. Food placement flows through a
// fixed-seed LCG, so a given run is deterministic and reproducible.
//
// Feel (GameJuice, Engine/Core/GameJuice.h): every food bite bursts green +
// red sparks with a coin chime, a small shake, and a rising "+10"; death
// detonates the snake with a heavy shake + hit-stop + falling tone (and a
// gold "NEW BEST!" celebration on a session record); a win fires a confetti
// fanfare. All sound is synthesized in memory, identical native / WASM /
// headless. Pause (P) and a session best score included.
//
// One code path serves human input and the LLM: arrow keys, WASD, and the
// up/down/left/right actions all call setDirection(), so an LLM plays the
// exact game a human plays. The board is reported as a grid (0 empty, 1
// body, 2 head, 3 food).
//
// The smoke autopilot greedily steers the head toward the food without
// reversing, so a headless run keeps exercising move -> eat -> grow -> juice
// (and death -> restart) for its whole window.
//
// Controls: arrows / WASD | P pause | R restart.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Snake : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 25;
    static constexpr int GRID_H = 25;
    static constexpr int WIN_LENGTH = 20;          // segments to win
    static constexpr float MOVE_START = 0.14f;     // s per cell at the start
    static constexpr float MOVE_MIN = 0.06f;       // speed cap
    static constexpr float MOVE_RAMP = 0.004f;     // faster per food eaten

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
    std::vector<std::pair<int, int>> body;   // body[0] = head
    int foodX = 0, foodY = 0;
    int direction = 1;                       // 0 up, 1 right, 2 down, 3 left
    int nextDirection = 1;
    int score = 0;
    int bestScore = 0;                       // session best; survives restarts
    float moveTimer = 0.0f;
    float moveInterval = MOVE_START;

    // Fixed-seed LCG: food placement is deterministic and unit-testable.
    uint32_t lcgState = 0x5E7A0DEu;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;

public:
    Snake() : Game2D("Snake", 700, 700, 28) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hook: rebuild the snake as a straight row of `n` segments and, if
    // that reaches the win length, fire the win path. Not used by gameplay.
    void setLengthForTest(int n) {
        body.clear();
        for (int i = 0; i < n; ++i) body.push_back({5 + i, 5});
        direction = 1;
        nextDirection = 1;
        updateHUD();
        if (static_cast<int>(body.size()) >= WIN_LENGTH && gameRunning) {
            winGame();
        }
    }

    void initGame() override {
        createGrid(GRID_W, GRID_H, tileSize);
        grid->fill({16, 20, 32, 255});
        grid->setBorderColor({12, 14, 22, 255});

        score = 0;
        paused = false;
        moveTimer = 0.0f;
        moveInterval = MOVE_START;
        particles.clear();
        floatTexts = uj::FloatingText{};

        // Snake at center, heading right, with two body segments behind.
        const int sx = GRID_W / 2, sy = GRID_H / 2;
        body = {{sx, sy}, {sx - 1, sy}, {sx - 2, sy}};
        direction = 1;
        nextDirection = 1;
        spawnFood();

        hud = createText(10, 6, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, 700 - 26, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("Arrows / WASD | P pause | R restart");

        registerAction("up", [this]() { return setDirection(0); });
        registerAction("down", [this]() { return setDirection(2); });
        registerAction("left", [this]() { return setDirection(3); });
        registerAction("right", [this]() { return setDirection(1); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_UP).onPress([this]() { (void)setDirection(0); });
        bindKey(KEY_DOWN).onPress([this]() { (void)setDirection(2); });
        bindKey(KEY_LEFT).onPress([this]() { (void)setDirection(3); });
        bindKey(KEY_RIGHT).onPress([this]() { (void)setDirection(1); });
        bindKey(KEY_W).onPress([this]() { (void)setDirection(0); });
        bindKey(KEY_S).onPress([this]() { (void)setDirection(2); });
        bindKey(KEY_A).onPress([this]() { (void)setDirection(3); });
        bindKey(KEY_D).onPress([this]() { (void)setDirection(1); });
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

        if (smokeMode) autopilot();

        moveTimer += dt;
        if (moveTimer >= moveInterval) {
            moveTimer -= moveInterval;
            step();
        }

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        renderGrid();   // the board background does not shake

        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // Food.
        drawCell(sdl, foodX, foodY, {255, 60, 50, 255}, sx, sy);

        // Body (draw the tail up toward the head so the head sits on top).
        for (int i = static_cast<int>(body.size()) - 1; i >= 1; --i) {
            drawCell(sdl, body[static_cast<std::size_t>(i)].first,
                     body[static_cast<std::size_t>(i)].second,
                     {40, 190, 70, 255}, sx, sy);
        }
        // Head: brighter, with a dark eye toward the travel direction.
        if (!body.empty()) {
            const auto& h = body[0];
            drawCell(sdl, h.first, h.second, {90, 240, 110, 255}, sx, sy);
            drawEye(sdl, h.first, h.second, sx, sy);
        }

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
        state.score = score;
        state.message = statusText;
        state.stats["score"] = score;
        state.stats["best"] = std::max(bestScore, score);
        state.stats["length"] = static_cast<int>(body.size());
        state.stats["length_goal"] = WIN_LENGTH;
        state.stats["direction"] = direction;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.gridWidth = GRID_W;
        state.gridHeight = GRID_H;
        state.grid.assign(static_cast<std::size_t>(GRID_H),
                          std::vector<int>(static_cast<std::size_t>(GRID_W), 0));
        for (std::size_t i = 1; i < body.size(); ++i) {
            state.grid[static_cast<std::size_t>(body[i].second)]
                      [static_cast<std::size_t>(body[i].first)] = 1;
        }
        if (!body.empty()) {
            state.grid[static_cast<std::size_t>(body[0].second)]
                      [static_cast<std::size_t>(body[0].first)] = 2;
            state.entities["head"] = {body[0].first, body[0].second};
        }
        state.grid[static_cast<std::size_t>(foodY)]
                  [static_cast<std::size_t>(foodX)] = 3;
        state.entities["food"] = {foodX, foodY};
        return state;
    }

private:
    // ---- Movement ------------------------------------------------------------
    static bool isOpposite(int a, int b) {
        return (a == 0 && b == 2) || (a == 2 && b == 0) ||
               (a == 1 && b == 3) || (a == 3 && b == 1);
    }

    ActionResult setDirection(int newDir) {
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
        if (isOpposite(newDir, direction) || isOpposite(newDir, nextDirection)) {
            result.message = "Cannot reverse direction";
            return result;
        }
        nextDirection = newDir;
        result.success = true;
        result.message = "Direction " + std::to_string(newDir);
        return result;
    }

    void step() {
        direction = nextDirection;
        int dx = 0, dy = 0;
        if (direction == 0) dy = -1;
        else if (direction == 1) dx = 1;
        else if (direction == 2) dy = 1;
        else dx = -1;

        const int nx = body[0].first + dx;
        const int ny = body[0].second + dy;

        // Wall death (classic Snake; wrap-around is game #17, Portal Snake).
        if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
            die();
            return;
        }

        // Self collision. When growing the tail stays put, so every segment
        // is solid; otherwise the tail moves away this tick and is exempt.
        const bool grow = (nx == foodX && ny == foodY);
        const std::size_t checkEnd = grow ? body.size() : body.size() - 1;
        for (std::size_t i = 0; i < checkEnd; ++i) {
            if (body[i].first == nx && body[i].second == ny) {
                die();
                return;
            }
        }

        body.insert(body.begin(), {nx, ny});
        if (grow) {
            score += 10;
            moveInterval = std::max(MOVE_MIN, moveInterval - MOVE_RAMP);
            // ---- Juice: food bite -----------------------------------------
            sfx.play(uj::Sfx::Coin);
            shake.add(0.12f);
            hitStop.trigger(0.03f);
            particles.burst((float)pixX(nx), (float)pixY(ny), 10,
                            {255, 60, 50, 255}, 8.0f, 0.45f, 5.0f);
            particles.burst((float)pixX(nx), (float)pixY(ny), 6,
                            {120, 240, 120, 255}, 6.0f, 0.4f, 4.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                pixX(nx) - 14, pixY(ny) - 20, "+10"),
                pixX(nx) - 14, pixY(ny) - 20);
            updateHUD();
            if (static_cast<int>(body.size()) >= WIN_LENGTH) {
                winGame();
                return;
            }
            spawnFood();
        } else {
            body.pop_back();
        }
    }

    // ---- Food -----------------------------------------------------------------
    bool isBodyAt(int x, int y) const {
        for (const auto& [bx, by] : body) {
            if (bx == x && by == y) return true;
        }
        return false;
    }

    void spawnFood() {
        for (int tries = 0; tries < 512; ++tries) {
            const int x = static_cast<int>(lcgNext() % GRID_W);
            const int y = static_cast<int>(lcgNext() % GRID_H);
            if (!isBodyAt(x, y)) {
                foodX = x;
                foodY = y;
                return;
            }
        }
        // Nearly full: scan the board for any free cell.
        for (int y = 0; y < GRID_H; ++y) {
            for (int x = 0; x < GRID_W; ++x) {
                if (!isBodyAt(x, y)) {
                    foodX = x;
                    foodY = y;
                    return;
                }
            }
        }
        // Board completely full -> that's a win.
        winGame();
    }

    // ---- Win / lose -----------------------------------------------------------
    void die() {
        const bool newBest = score > bestScore;
        bestScore = std::max(bestScore, score);
        // ---- Juice: the snake detonates at the head -------------------------
        const auto& h = body[0];
        particles.burst((float)pixX(h.first), (float)pixY(h.second), 26,
                        {90, 240, 110, 255}, 11.0f, 0.7f, 6.0f);
        particles.burst((float)pixX(h.first), (float)pixY(h.second), 10,
                        {255, 255, 255, 255}, 7.0f, 0.5f, 5.0f);
        shake.add(0.6f);
        hitStop.trigger(0.15f);
        if (newBest && score > 0) {
            sfx.play(uj::Sfx::Win);
            particles.burst(GRID_W * 0.5f * tileSize, 140.0f, 14,
                            {230, 200, 60, 255}, 8.0f, 0.7f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                GRID_W * tileSize / 2 - 5 * tileSize, 120, "NEW BEST!"),
                GRID_W * tileSize / 2 - 5 * tileSize, 120);
        } else {
            sfx.play(uj::Sfx::Lose);
        }
        setMessage("GAME OVER - Best " + std::to_string(bestScore) +
                   " - Press R to restart");
        endGame();
    }

    void winGame() {
        bestScore = std::max(bestScore, score);
        sfx.play(uj::Sfx::Win);
        shake.add(0.5f);
        for (int i = 0; i < 3; ++i) {
            particles.burst(140.0f + (float)i * 210.0f, 260.0f, 20,
                i == 0 ? SDL_Color{255, 220, 60, 255} :
                i == 1 ? SDL_Color{80, 220, 255, 255} :
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

    // ---- Autopilot (greedy chase) ---------------------------------------------
    void autopilot() {
        if (body.empty()) return;
        const int hx = body[0].first, hy = body[0].second;
        const int dx = (foodX > hx) - (foodX < hx);
        const int dy = (foodY > hy) - (foodY < hy);

        auto dirFor = [](int dx, int dy) {
            if (dx > 0) return 1;
            if (dx < 0) return 3;
            if (dy > 0) return 2;
            return 0;
        };
        // Prefer the axis with the larger gap; never reverse.
        if (std::abs(foodX - hx) >= std::abs(foodY - hy)) {
            if (dx != 0 && !isOpposite(dirFor(dx, 0), direction)) {
                (void)setDirection(dirFor(dx, 0));
            } else if (dy != 0 && !isOpposite(dirFor(0, dy), direction)) {
                (void)setDirection(dirFor(0, dy));
            }
        } else {
            if (dy != 0 && !isOpposite(dirFor(0, dy), direction)) {
                (void)setDirection(dirFor(0, dy));
            } else if (dx != 0 && !isOpposite(dirFor(dx, 0), direction)) {
                (void)setDirection(dirFor(dx, 0));
            }
        }
    }

    // ---- Rendering helpers ----------------------------------------------------
    int pixX(int cellX) const { return cellX * tileSize + tileSize / 2; }
    int pixY(int cellY) const { return cellY * tileSize + tileSize / 2; }

    void drawCell(SDL_Renderer* sdl, int x, int y, SDL_Color col,
                  int sx, int sy) const {
        SDL_Rect r = {x * tileSize + sx + 2, y * tileSize + sy + 2,
                      tileSize - 4, tileSize - 4};
        SDL_SetRenderDrawColor(sdl, col.r, col.g, col.b, 255);
        SDL_RenderFillRect(sdl, &r);
    }

    void drawEye(SDL_Renderer* sdl, int hx, int hy, int sx, int sy) const {
        int ex = hx * tileSize + sx, ey = hy * tileSize + sy;
        if (direction == 0) ey += tileSize / 2 - tileSize / 3;
        else if (direction == 2) ey += tileSize / 2 + tileSize / 3;
        else if (direction == 1) ex += tileSize / 2 + tileSize / 3;
        else ex += tileSize / 2 - tileSize / 3;
        SDL_Rect e = {ex + tileSize / 2 - 5, ey + tileSize / 2 - 5, 5, 5};
        SDL_SetRenderDrawColor(sdl, 10, 10, 20, 255);
        SDL_RenderFillRect(sdl, &e);
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
                     "    Length " + std::to_string(body.size()) + "/" +
                     std::to_string(WIN_LENGTH));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the Snake class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static Snake game;
#else
    Snake game;
#endif
    game.run();
    return 0;
}
#endif
