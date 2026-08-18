// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Portal Snake - the wrap-around classic, game #17 of the 100-game program.
//
// A green snake slithers across a 25x25 board eating red food, growing one
// segment (and speeding up) per bite. Unlike classic Snake, the edges are
// portals: the head slides off one side and re-enters through the matching
// portal on the opposite side, with a shimmer and a flash at both mouths.
// Bite your own body and it bursts; reach the target length to win. Food
// placement flows through a fixed-seed LCG, so a given run is deterministic
// and reproducible.
//
// The portal twist pays off: eat food within a few seconds of teleporting
// and the bite doubles to +20 with a gold "PORTAL BONUS!" popup - the same
// chase, but the fastest route to the food is often THROUGH the wall.
//
// Feel (GameJuice, Engine/Core/GameJuice.h): every food bite bursts green +
// red sparks with a coin chime, a small shake, and a rising "+10"; every
// portal slide fires a swoosh, a double spark burst (exit + entrance), and
// a white flash ring at the arrival mouth; death detonates the snake with a
// heavy shake + hit-stop + falling tone (and a gold "NEW BEST!" celebration
// on a session record); a win fires a confetti fanfare. All sound is
// synthesized in memory, identical native / WASM / headless. Pause (P) and
// a session best score included.
//
// One code path serves human input and the LLM: arrow keys, WASD, and the
// up/down/left/right actions all call setDirection(). The board is reported
// as a grid (0 empty, 1 body, 2 head, 3 food) plus the head/food positions
// and the portal-bonus window, so an LLM can route through the portals too.
//
// The smoke autopilot greedily steers the head toward the food using
// wrap-aware distances (the shorter way around the torus), so a headless
// run keeps exercising move -> portal -> eat -> grow -> juice (and death ->
// restart) for its whole window.
//
// Controls: arrows / WASD | P pause | R restart.

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

class PortalSnake : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 25;
    static constexpr int GRID_H = 25;
    static constexpr int WIN_LENGTH = 20;          // segments to win
    static constexpr float MOVE_START = 0.14f;     // s per cell at the start
    static constexpr float MOVE_MIN = 0.06f;       // speed cap
    static constexpr float MOVE_RAMP = 0.004f;     // faster per food eaten
    static constexpr float PORTAL_WINDOW = 2.5f;   // bonus window after a wrap

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
    int wrapCount = 0;                       // portals slid through this run
    float moveTimer = 0.0f;
    float moveInterval = MOVE_START;
    float portalBonusTimer = 0.0f;           // >0 = double the next bite
    float timeAccum = 0.0f;                  // portal animation clock
    float arrivalFlash = 0.0f;               // white ring at the arrival mouth
    int arrivalX = 0, arrivalY = 0;

    // Fixed-seed LCG: food placement is deterministic and unit-testable.
    uint32_t lcgState = 0x51E7A0Du;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;

public:
    PortalSnake() : Game2D("Portal Snake", 700, 700, 28) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hooks (not used by gameplay).
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

    void setBodyForTest(const std::vector<std::pair<int, int>>& b, int dir) {
        body = b;
        direction = dir;
        nextDirection = dir;
    }

    void setFoodForTest(int x, int y) {
        foodX = x;
        foodY = y;
    }

    void initGame() override {
        createGrid(GRID_W, GRID_H, tileSize);
        grid->fill({16, 20, 32, 255});
        grid->setBorderColor({12, 14, 22, 255});

        score = 0;
        paused = false;
        moveTimer = 0.0f;
        moveInterval = MOVE_START;
        wrapCount = 0;
        portalBonusTimer = 0.0f;
        arrivalFlash = 0.0f;
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
        setMessage("The edges are portals - slide through for a bonus!");

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
        if (portalBonusTimer > 0.0f) portalBonusTimer -= dt;
        if (arrivalFlash > 0.0f) arrivalFlash -= dt;
        timeAccum += dt;

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        renderGrid();   // the board background does not shake

        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // The four portal mouths (drawn under the snake).
        drawPortals(sdl, sx, sy);

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

        // Arrival flash ring at the mouth the head just slid through.
        if (arrivalFlash > 0.0f) {
            const float a = arrivalFlash / 0.4f;
            const int r = 4 + (int)(22.0f * (1.0f - a));
            SDL_SetRenderDrawColor(sdl, 255, 255, 255,
                                   (Uint8)(200.0f * a));
            SDL_Rect ring = {pixX(arrivalX) - r + sx, pixY(arrivalY) - r + sy,
                             r * 2, r * 2};
            SDL_RenderDrawRect(sdl, &ring);
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
        state.stats["wraps"] = wrapCount;
        state.stats["portal_bonus"] = portalBonusTimer > 0.0f ? 1 : 0;
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

        int nx = body[0].first + dx;
        int ny = body[0].second + dy;

        // Portal wrap: slide off one edge, re-enter the opposite portal.
        if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
            const int exitX = body[0].first, exitY = body[0].second;
            nx = (nx + GRID_W) % GRID_W;
            ny = (ny + GRID_H) % GRID_H;
            ++wrapCount;
            portalBonusTimer = PORTAL_WINDOW;
            arrivalX = nx;
            arrivalY = ny;
            arrivalFlash = 0.4f;
            // ---- Juice: slide through the portal -------------------------
            sfx.play(uj::Sfx::Swing);
            shake.add(0.08f);
            particles.burst((float)pixX(exitX), (float)pixY(exitY), 8,
                            {150, 220, 255, 255}, 5.0f, 0.35f, 3.5f);
            particles.burst((float)pixX(nx), (float)pixY(ny), 10,
                            {150, 220, 255, 255}, 6.0f, 0.45f, 4.0f);
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
            const bool portalBite = portalBonusTimer > 0.0f;
            const int gain = portalBite ? 20 : 10;
            score += gain;
            moveInterval = std::max(MOVE_MIN, moveInterval - MOVE_RAMP);
            // ---- Juice: food bite -----------------------------------------
            sfx.play(uj::Sfx::Coin);
            shake.add(portalBite ? 0.18f : 0.12f);
            hitStop.trigger(0.03f);
            particles.burst((float)pixX(nx), (float)pixY(ny), 10,
                            {255, 60, 50, 255}, 8.0f, 0.45f, 5.0f);
            particles.burst((float)pixX(nx), (float)pixY(ny), 6,
                            {120, 240, 120, 255}, 6.0f, 0.4f, 4.0f);
            if (portalBite) {
                particles.burst((float)pixX(nx), (float)pixY(ny), 8,
                                {230, 200, 60, 255}, 7.0f, 0.5f, 4.5f);
                floatTexts.spawn(std::make_shared<TextDisplay>(
                    pixX(nx) - 44, pixY(ny) - 24, "PORTAL BONUS +20!"),
                    pixX(nx) - 44, pixY(ny) - 24);
            } else {
                floatTexts.spawn(std::make_shared<TextDisplay>(
                    pixX(nx) - 14, pixY(ny) - 20, "+10"),
                    pixX(nx) - 14, pixY(ny) - 20);
            }
            portalBonusTimer = 0.0f;
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

    // ---- Autopilot (greedy chase, wrap-aware) -----------------------------------
    // Distance on the torus: the shorter way around each axis.
    static int wrapDist(int a, int b, int size) {
        const int d = std::abs(a - b);
        return std::min(d, size - d);
    }

    void autopilot() {
        if (body.empty()) return;
        const int hx = body[0].first, hy = body[0].second;
        // Choose the axis with the larger wrapped gap, then move toward the
        // food the shorter way around; never reverse.
        if (wrapDist(hx, foodX, GRID_W) >= wrapDist(hy, foodY, GRID_H)) {
            const int dx = wrapDist(hx, foodX, GRID_W);
            if (dx > 0) {
                const bool goRight =
                    ((hx + 1) % GRID_W == foodX) ||
                    wrapDist((hx + 1) % GRID_W, foodX, GRID_W) <
                        wrapDist((hx - 1 + GRID_W) % GRID_W, foodX, GRID_W);
                (void)setDirection(goRight ? 1 : 3);
            } else {
                (void)setDirection(hy < foodY ? 2 : 0);
            }
        } else {
            const int dy = wrapDist(hy, foodY, GRID_H);
            if (dy > 0) {
                const bool goDown =
                    ((hy + 1) % GRID_H == foodY) ||
                    wrapDist((hy + 1) % GRID_H, foodY, GRID_H) <
                        wrapDist((hy - 1 + GRID_H) % GRID_H, foodY, GRID_H);
                (void)setDirection(goDown ? 2 : 0);
            } else {
                (void)setDirection(hx < foodX ? 1 : 3);
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

    // The four portal mouths at the edge midpoints: two concentric rings
    // with a slowly rotating arc, so the portal mouths read at a glance.
    void drawPortals(SDL_Renderer* sdl, int sx, int sy) const {
        const int half = GRID_W * tileSize / 2;
        const int cx = half + sx, cy = half + sy;
        const int rOuter = tileSize * 2, rInner = tileSize;
        const float phase = timeAccum * 2.4f;
        for (int m = 0; m < 4; ++m) {
            int px = cx, py = cy;
            if (m == 0) { py = tileSize / 2 + sy; }
            else if (m == 1) { px = GRID_W * tileSize - tileSize / 2 + sx; }
            else if (m == 2) { py = GRID_H * tileSize - tileSize / 2 + sy; }
            else { px = tileSize / 2 + sx; }
            SDL_SetRenderDrawColor(sdl, 90, 160, 230, 60);
            SDL_Rect outer = {px - rOuter, py - rOuter, rOuter * 2, rOuter * 2};
            SDL_RenderDrawRect(sdl, &outer);
            SDL_SetRenderDrawColor(sdl, 150, 210, 255, 90);
            SDL_Rect inner = {px - rInner, py - rInner, rInner * 2, rInner * 2};
            SDL_RenderDrawRect(sdl, &inner);
            // Rotating arc: three small dashes orbiting the mouth.
            SDL_SetRenderDrawColor(sdl, 200, 240, 255, 140);
            for (int k = 0; k < 3; ++k) {
                const float a = phase + (float)k * 2.094f;
                const int ax = px + (int)(std::cos(a) * rOuter * 0.7f);
                const int ay = py + (int)(std::sin(a) * rOuter * 0.7f);
                SDL_Rect dot = {ax - 2, ay - 2, 5, 5};
                SDL_RenderFillRect(sdl, &dot);
            }
        }
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
                     std::to_string(WIN_LENGTH) +
                     "    Portals " + std::to_string(wrapCount));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the PortalSnake class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static PortalSnake game;
#else
    PortalSnake game;
#endif
    game.run();
    return 0;
}
#endif
