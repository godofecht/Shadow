// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Roguelike - the turn-based dungeon crawl, game #4 of the 100-game program,
// retrofitted to the AAA-feel bar with the GameJuice kit.
//
// A procedurally carved dungeon (rooms + L-corridors) hides enemies, gold,
// and a staircase. Step into an enemy to fight it; step onto gold to collect
// it; reach the stairs to descend. Reach level 5's stairs to escape and win;
// die and it's game over. All generation and combat rolls flow through a
// fixed-seed LCG, so a given run is deterministic and reproducible.
//
// Feel (GameJuice, Engine/Core/GameJuice.h): a hit flashes the enemy white
// with a thwack and hit-stop; a kill bursts the enemy red with a "KILL!"
// popup; gold chips +10 with a coin chime; a descend fires the stairs and a
// "LEVEL N!" popup; death detonates the player with a heavy shake + falling
// tone (and a gold "NEW BEST!" celebration on a session record); an escape
// fires a confetti fanfare. All sound is synthesized in memory, identical
// native / WASM / headless. Pause (P) and a session best score included.
//
// One code path serves human input and the LLM: arrow keys, WASD, and the
// up/down/left/right actions all call movePlayer(), so an LLM plays the
// exact game a human plays. The dungeon is reported as a grid (0 floor, 1
// wall, 2 gold, 3 stairs, 4 enemy, 5 player) plus entity positions.
//
// The smoke autopilot BFS-routes to the stairs, preferring paths that avoid
// enemies (and fighting only when the path is blocked), so a headless run
// keeps exercising move -> collect -> fight -> descend -> win/lose ->
// restart for its whole window.
//
// Controls: arrows / WASD | P pause | R restart.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"
#include "RoguelikeState.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Roguelike : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 35;
    static constexpr int GRID_H = 35;
    static constexpr int FINAL_LEVEL = 5;
    static constexpr int DIRS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- World ------------------------------------------------------------
    std::shared_ptr<TextDisplay> messageText;
    std::shared_ptr<GameStats> stats;
    RoguelikeState state;
    int bestScore = 0;             // session best gold; survives restarts
    int kills = 0;                 // enemies defeated this run
    int stairsX = -1, stairsY = -1;

    // Hit / hurt flashes (short-lived white overlays).
    int flashEnemy = -1;
    float flashTimer = 0.0f;
    float hurtFlashTimer = 0.0f;
    float autopilotTimer = 0.0f;

    // The autopilot commits to a full BFS path and follows it, so enemy
    // movement between turns can't bounce the player back and forth.
    std::vector<std::pair<int, int>> plan;

    // Fixed-seed LCG: rooms, placement, and combat rolls are deterministic.
    uint32_t lcgState = 0x50A7C0DEu;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

public:
    Roguelike() : Game2D("Roguelike", 700, 700, 20) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hook: BFS-step the player one turn toward a target (same path the
    // autopilot uses). Returns false when no move was possible.
    bool stepTowardForTest(int tx, int ty) {
        int dx = 0, dy = 0;
        if (!bfsStepToward(tx, ty, dx, dy)) return false;
        return movePlayer(dx, dy);
    }

    // Test hook: commit to a full BFS route toward (tx, ty) and advance along
    // it one turn (fighting a blocker when the route is cut). Returns false
    // only when no path exists at all.
    bool advanceTowardForTest(int tx, int ty) { return advanceToward(tx, ty); }

    // Test hook: fight the nearest enemy repeatedly until it is gone or the
    // player dies. Returns the number of turns taken.
    int huntEnemiesForTest(int maxTurns) {
        int turns = 0;
        while (turns < maxTurns && state.getPhase() == RoguelikePhase::Playing) {
            const auto& p = state.getPlayer();
            int tx = -1, ty = -1, best = 1 << 30;
            for (const auto& e : state.getEnemies()) {
                if (!e.alive) continue;
                const int d = std::abs(e.x - p.x) + std::abs(e.y - p.y);
                if (d < best) {
                    best = d;
                    tx = e.x;
                    ty = e.y;
                }
            }
            if (tx < 0) break;   // nothing left to fight
            int dx = 0, dy = 0;
            if (!bfsStepToward(tx, ty, dx, dy)) break;
            (void)movePlayer(dx, dy);
            ++turns;
        }
        return turns;
    }

    // Test hook: restart at the final level with a big HP buffer and walk to
    // the stairs, driving the win path deterministically. Not used by gameplay.
    void forceWinForTest() {
        state.startRun(FINAL_LEVEL, FINAL_LEVEL, 1000, 10);
        generateLevel();
        for (int i = 0; i < 2000 && state.getPhase() == RoguelikePhase::Playing; ++i) {
            if (!advanceToward(stairsX, stairsY)) break;
        }
    }

    void initGame() override {
        textDisplays.clear();
        buttons.clear();
        statsDisplays.clear();
        entities.clear();
        paused = false;
        kills = 0;
        flashEnemy = -1;
        flashTimer = 0.0f;
        hurtFlashTimer = 0.0f;
        autopilotTimer = 0.0f;
        plan.clear();
        particles.clear();
        floatTexts = uj::FloatingText{};

        // UI must exist before generateLevel(), since it updates text/stats.
        messageText = createText(10, 10, "");
        messageText->setColor({255, 255, 255, 255});
        stats = createStats(10, 35);

        state.startRun(1, FINAL_LEVEL, 100, 10);
        generateLevel();

        registerAction("up", [this]() { return moveAction(0, -1); });
        registerAction("down", [this]() { return moveAction(0, 1); });
        registerAction("left", [this]() { return moveAction(-1, 0); });
        registerAction("right", [this]() { return moveAction(1, 0); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_UP).onPress([this]() { (void)movePlayer(0, -1); });
        bindKey(KEY_DOWN).onPress([this]() { (void)movePlayer(0, 1); });
        bindKey(KEY_LEFT).onPress([this]() { (void)movePlayer(-1, 0); });
        bindKey(KEY_RIGHT).onPress([this]() { (void)movePlayer(1, 0); });
        bindKey(KEY_W).onPress([this]() { (void)movePlayer(0, -1); });
        bindKey(KEY_S).onPress([this]() { (void)movePlayer(0, 1); });
        bindKey(KEY_A).onPress([this]() { (void)movePlayer(-1, 0); });
        bindKey(KEY_D).onPress([this]() { (void)movePlayer(1, 0); });
        bindKey(KEY_P).onPress([this]() { paused = !paused; });
        bindKey(KEY_R).onPress([this]() {
            if (gameOver || gameWon) startGame();
        });
    }

    void updateGame(float dt) override {
        if (!gameRunning) return;

        if (paused) {
            if (messageText) messageText->setText("PAUSED - press P to resume");
            return;
        }

        if (hitStop.frozen()) {
            hitStop.update(dt);
            return;
        }

        // Smoke autopilot: BFS toward the stairs every beat (one turn each).
        if (smokeMode && state.getPhase() == RoguelikePhase::Playing) {
            autopilotTimer -= dt;
            if (autopilotTimer <= 0.0f) {
                autopilotTimer = 0.15f;
                autopilot();
            }
        }

        updateFx(dt);
        updateStats();
    }

    void renderGame() override {
        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // The dungeon (walls + floor) shakes with the world.
        for (int y = 0; y < GRID_H; ++y) {
            for (int x = 0; x < GRID_W; ++x) {
                SDL_Rect r = {x * tileSize + sx, y * tileSize + sy,
                              tileSize, tileSize};
                if (state.isSolid(x, y)) {
                    SDL_SetRenderDrawColor(sdl, 15, 15, 22, 255);
                    SDL_RenderFillRect(sdl, &r);
                    drawGlyph(sdl, x, y, "#", {90, 90, 110, 255}, 0.7f, sx, sy);
                } else {
                    const bool check = ((x + y) % 2) == 0;
                    SDL_SetRenderDrawColor(sdl, check ? 42 : 36,
                                           check ? 42 : 36,
                                           check ? 64 : 58, 255);
                    SDL_RenderFillRect(sdl, &r);
                }
            }
        }

        // Gold: a yellow coin with a '$'.
        for (const auto& [gx, gy] : state.goldPositions()) {
            SDL_Rect r = {gx * tileSize + sx + 5, gy * tileSize + sy + 5,
                          tileSize - 10, tileSize - 10};
            SDL_SetRenderDrawColor(sdl, 255, 215, 0, 255);
            SDL_RenderFillRect(sdl, &r);
            drawGlyph(sdl, gx, gy, "$", {40, 25, 0, 255}, 0.8f, sx, sy);
        }

        // Stairs: '>'.
        drawGlyph(sdl, stairsX, stairsY, ">", {255, 255, 80, 255}, 0.8f, sx, sy);

        // Enemies (a hit flash turns the cell white).
        const auto& modelEnemies = state.getEnemies();
        for (size_t i = 0; i < modelEnemies.size(); ++i) {
            const auto& e = modelEnemies[i];
            if (!e.alive) continue;
            const bool flashing = static_cast<int>(i) == flashEnemy &&
                                  flashTimer > 0.0f;
            SDL_Rect r = {e.x * tileSize + sx + 2, e.y * tileSize + sy + 2,
                          tileSize - 4, tileSize - 4};
            SDL_SetRenderDrawColor(sdl, flashing ? 255 : 220,
                                   flashing ? 255 : 40,
                                   flashing ? 255 : 40, 255);
            SDL_RenderFillRect(sdl, &r);
            drawGlyph(sdl, e.x, e.y, "e", {255, 230, 230, 255}, 0.8f, sx, sy);
        }

        // The player (a hurt flash turns the cell white too).
        const auto& p = state.getPlayer();
        const bool hurtFlash = hurtFlashTimer > 0.0f;
        SDL_Rect pr = {p.x * tileSize + sx + 2, p.y * tileSize + sy + 2,
                       tileSize - 4, tileSize - 4};
        SDL_SetRenderDrawColor(sdl, hurtFlash ? 255 : 0,
                               hurtFlash ? 255 : 230,
                               hurtFlash ? 255 : 90, 255);
        SDL_RenderFillRect(sdl, &pr);
        drawGlyph(sdl, p.x, p.y, "@", {20, 30, 20, 255}, 0.8f, sx, sy);

        // Particles live in world space; floating text stays screen-stable.
        particles.render(sdl, sx, sy);
        floatTexts.render(getRenderer());

        if (paused) {
            SDL_Rect veil = {0, 0, 700, 700};
            SDL_SetRenderDrawColor(sdl, 0, 0, 0, 140);
            SDL_RenderFillRect(sdl, &veil);
        }

        messageText->render(getRenderer());
        for (auto& s : statsDisplays) s->render(getRenderer());
    }

    // ---- LLM state ----------------------------------------------------------
    GameState getState() const override {
        GameState s = Game2D::getState();
        s.level = state.getLevel();
        s.score = state.getGoldCount();
        s.message = state.getMessage();
        const auto& p = state.getPlayer();
        s.stats["hp"] = p.health;
        s.stats["atk"] = p.attack;
        s.stats["gold"] = state.getGoldCount();
        s.stats["best"] = std::max(bestScore, state.getGoldCount());
        s.stats["kills"] = kills;
        s.stats["turn"] = state.getTurns();
        s.stats["level"] = state.getLevel();
        s.stats["paused"] = paused ? 1 : 0;
        s.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        s.stats["particles"] = particles.count();
        s.entities["player"] = {p.x, p.y};
        const auto& modelEnemies = state.getEnemies();
        for (size_t i = 0; i < modelEnemies.size(); ++i) {
            if (!modelEnemies[i].alive) continue;
            s.entities["enemy_" + std::to_string(i)] = {
                modelEnemies[i].x, modelEnemies[i].y};
        }
        int gi = 0;
        for (const auto& [gx, gy] : state.goldPositions()) {
            s.entities["gold_" + std::to_string(gi++)] = {gx, gy};
        }
        s.entities["stairs"] = {stairsX, stairsY};
        // Board map: 0 floor, 1 wall, 2 gold, 3 stairs, 4 enemy, 5 player.
        s.gridWidth = GRID_W;
        s.gridHeight = GRID_H;
        s.grid.assign(static_cast<std::size_t>(GRID_H),
                      std::vector<int>(static_cast<std::size_t>(GRID_W), 0));
        for (int y = 0; y < GRID_H; ++y) {
            for (int x = 0; x < GRID_W; ++x) {
                int v = state.isSolid(x, y) ? 1 : 0;
                if (v == 0 && state.goldAt(x, y)) v = 2;
                if (v == 0 && x == stairsX && y == stairsY) v = 3;
                s.grid[static_cast<std::size_t>(y)]
                      [static_cast<std::size_t>(x)] = v;
            }
        }
        for (const auto& e : modelEnemies) {
            if (!e.alive) continue;
            s.grid[static_cast<std::size_t>(e.y)]
                  [static_cast<std::size_t>(e.x)] = 4;
        }
        s.grid[static_cast<std::size_t>(p.y)]
              [static_cast<std::size_t>(p.x)] = 5;
        return s;
    }

private:
    // ---- Level generation -----------------------------------------------------
    void generateLevel() {
        state.startLevelMap(GRID_W, GRID_H);

        // Carve rooms out of the solid wall, then connect them with
        // L-shaped corridors (deterministic via the fixed-seed LCG).
        const int rooms = 5 + static_cast<int>(lcgNext() % 5);
        std::vector<std::pair<int, int>> centers;
        for (int i = 0; i < rooms; ++i) {
            const int rw = 4 + static_cast<int>(lcgNext() % 5);
            const int rh = 4 + static_cast<int>(lcgNext() % 5);
            const int rx = 1 + static_cast<int>(lcgNext() % (uint32_t)(GRID_W - rw - 2));
            const int ry = 1 + static_cast<int>(lcgNext() % (uint32_t)(GRID_H - rh - 2));
            for (int y = ry; y < ry + rh && y < GRID_H - 1; ++y) {
                for (int x = rx; x < rx + rw && x < GRID_W - 1; ++x) {
                    state.setSolid(x, y, false);
                }
            }
            centers.emplace_back(rx + rw / 2, ry + rh / 2);
        }
        for (size_t i = 1; i < centers.size(); ++i) {
            const int x1 = centers[i - 1].first, y1 = centers[i - 1].second;
            const int x2 = centers[i].first, y2 = centers[i].second;
            for (int x = std::min(x1, x2); x <= std::max(x1, x2); ++x) {
                state.setSolid(x, y1, false);
            }
            for (int y = std::min(y1, y2); y <= std::max(y1, y2); ++y) {
                state.setSolid(x2, y, false);
            }
        }

        // Player in the first room, stairs in the last.
        state.setPlayerPos(centers[0].first, centers[0].second);
        stairsX = centers.back().first;
        stairsY = centers.back().second;
        state.setStairs(stairsX, stairsY);

        // Enemies scale with the level.
        const int enemyCount = 3 + state.getLevel();
        for (int i = 0; i < enemyCount; ++i) {
            int ex, ey;
            findEmptySpot(ex, ey);
            state.addEnemy(ex, ey, 20 + state.getLevel() * 5,
                           5 + state.getLevel() * 2);
        }

        // Gold.
        for (int i = 0; i < 5; ++i) {
            int gx, gy;
            findEmptySpot(gx, gy);
            state.addGold(gx, gy);
        }

        flashEnemy = -1;
        flashTimer = 0.0f;
        plan.clear();
        if (messageText) messageText->setText(state.getMessage());
        updateStats();
    }

    void findEmptySpot(int& x, int& y) {
        const auto& p = state.getPlayer();
        for (int tries = 0; tries < 512; ++tries) {
            x = static_cast<int>(lcgNext() % GRID_W);
            y = static_cast<int>(lcgNext() % GRID_H);
            if (!state.isSolid(x, y) && !(x == p.x && y == p.y)) return;
        }
        bool found = false;
        for (y = 0; y < GRID_H && !found; ++y) {
            for (x = 0; x < GRID_W && !found; ++x) {
                if (!state.isSolid(x, y) && !(x == p.x && y == p.y)) found = true;
            }
        }
        if (!found) {
            x = p.x;
            y = p.y;
        }
    }

    // ---- Turn / combat ----------------------------------------------------------
    bool movePlayer(int dx, int dy) {
        if (gameOver || paused || state.getPhase() != RoguelikePhase::Playing) {
            return false;
        }

        const auto result = state.playerStep(dx, dy, lcgNext());
        if (!result.blocked && !result.descended && !result.died && !result.won) {
            state.enemyStep();
        }

        const auto& p = state.getPlayer();

        // ---- Juice ----------------------------------------------------------
        if (result.fought) {
            const auto& e = state.getEnemies()[
                static_cast<std::size_t>(result.enemyHitIndex)];
            const int ex = pixX(e.x), ey = pixY(e.y);
            flashEnemy = result.enemyHitIndex;
            flashTimer = 0.15f;
            shake.add(0.12f);
            hitStop.trigger(0.03f);
            if (!e.alive) {
                // Kill: red burst + "KILL!" popup.
                ++kills;
                sfx.play(uj::Sfx::Kill);
                shake.add(0.3f);
                hitStop.trigger(0.07f);
                particles.burst((float)ex, (float)ey, 16,
                                {255, 70, 60, 255}, 9.0f, 0.55f, 5.0f);
                particles.burst((float)ex, (float)ey, 6,
                                {255, 255, 255, 255}, 6.0f, 0.4f, 4.0f);
                floatTexts.spawn(std::make_shared<TextDisplay>(
                    ex - 14, ey - 20, "KILL!"), ex - 14, ey - 20);
            } else {
                sfx.play(uj::Sfx::Hit);
                floatTexts.spawn(std::make_shared<TextDisplay>(
                    ex - 14, ey - 18, "-" + std::to_string(result.damageToEnemy)),
                    ex - 14, ey - 18);
            }
            if (result.damageToPlayer > 0) {
                hurtFlashTimer = 0.15f;
                if (!result.died) sfx.play(uj::Sfx::Hurt);
            }
        }
        if (result.collectedGold) {
            const int gx = pixX(p.x), gy = pixY(p.y);
            sfx.play(uj::Sfx::Coin);
            shake.add(0.06f);
            particles.burst((float)gx, (float)gy, 10,
                            {255, 215, 0, 255}, 7.0f, 0.5f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                gx - 16, gy - 18, "+10 GOLD"), gx - 16, gy - 18);
        }
        if (result.descended) {
            sfx.play(uj::Sfx::Descend);
            shake.add(0.25f);
            particles.burst((float)pixX(stairsX), (float)pixY(stairsY), 14,
                            {200, 200, 80, 255}, 8.0f, 0.6f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                GRID_W * tileSize / 2 - 40, 160,
                "LEVEL " + std::to_string(state.getLevel()) + "!"),
                GRID_W * tileSize / 2 - 40, 160);
            if (messageText) messageText->setText(state.getMessage());
            generateLevel();
            return true;
        }
        if (result.died) {
            loseGame();
            return true;
        }
        if (result.won) {
            winGame();
            return true;
        }

        if (messageText) messageText->setText(state.getMessage());
        updateStats();
        return !result.blocked;
    }

    // ---- Win / lose -----------------------------------------------------------
    void loseGame() {
        const bool newBest = state.getGoldCount() > bestScore;
        bestScore = std::max(bestScore, state.getGoldCount());
        const auto& p = state.getPlayer();
        // ---- Juice: the player detonates --------------------------------------
        particles.burst((float)pixX(p.x), (float)pixY(p.y), 26,
                        {0, 230, 90, 255}, 11.0f, 0.7f, 6.0f);
        particles.burst((float)pixX(p.x), (float)pixY(p.y), 10,
                        {255, 255, 255, 255}, 7.0f, 0.5f, 5.0f);
        shake.add(0.6f);
        hitStop.trigger(0.15f);
        if (newBest && state.getGoldCount() > 0) {
            sfx.play(uj::Sfx::Win);
            particles.burst(GRID_W * 0.5f * tileSize, 140.0f, 14,
                            {230, 200, 60, 255}, 8.0f, 0.7f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                GRID_W * tileSize / 2 - 5 * tileSize, 120, "NEW BEST!"),
                GRID_W * tileSize / 2 - 5 * tileSize, 120);
        } else {
            sfx.play(uj::Sfx::Lose);
        }
        if (messageText) {
            messageText->setText("YOU DIED - Best " +
                                 std::to_string(bestScore) +
                                 " - Press R to restart");
        }
        updateStats();
        gameWon = false;
        endGame();
    }

    void winGame() {
        const bool newBest = state.getGoldCount() > bestScore;
        bestScore = std::max(bestScore, state.getGoldCount());
        // ---- Juice: fanfare + confetti -----------------------------------------
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
            GRID_W * tileSize / 2 - 60, 120,
            newBest ? "YOU ESCAPED! NEW BEST!" : "YOU ESCAPED!"),
            GRID_W * tileSize / 2 - 60, 120);
        if (messageText) {
            messageText->setText("YOU ESCAPED! Best " +
                                 std::to_string(bestScore) +
                                 " - Press R to play again");
        }
        updateStats();
        gameWon = true;
        endGame();
    }

    // ---- Autopilot (committed BFS route to the stairs) -------------------------
    bool bfsFullPath(int tx, int ty, bool avoidEnemies,
                     std::vector<std::pair<int, int>>& path) {
        const auto& p = state.getPlayer();
        const int start = p.y * GRID_W + p.x;
        const int target = ty * GRID_W + tx;
        std::vector<int> cameFrom(GRID_W * GRID_H, -2);
        std::vector<int> queue;
        cameFrom[start] = -1;
        queue.push_back(start);
        std::size_t head = 0;
        while (head < queue.size()) {
            const int cur = queue[head++];
            if (cur == target) break;
            const int cx = cur % GRID_W, cy = cur / GRID_W;
            for (int d = 0; d < 4; ++d) {
                const int nx = cx + DIRS[d][0], ny = cy + DIRS[d][1];
                if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) continue;
                const int nid = ny * GRID_W + nx;
                if (cameFrom[nid] != -2) continue;
                if (state.isSolid(nx, ny)) continue;
                if (avoidEnemies && state.hasEnemyAt(nx, ny)) continue;
                cameFrom[nid] = cur;
                queue.push_back(nid);
            }
        }
        if (cameFrom[target] == -2) return false;
        std::vector<int> cells;
        int cur = target;
        while (cur != -1) {
            cells.push_back(cur);
            if (cur == start) break;
            cur = cameFrom[static_cast<std::size_t>(cur)];
        }
        path.clear();
        for (std::size_t i = cells.size(); i-- > 0;) {
            path.emplace_back(cells[i] % GRID_W, cells[i] / GRID_W);
        }
        return true;
    }

    bool bfsStepToward(int tx, int ty, int& dx, int& dy) {
        std::vector<std::pair<int, int>> path;
        if (bfsFullPath(tx, ty, true, path) ||
            bfsFullPath(tx, ty, false, path)) {
            if (path.size() < 2) return false;
            dx = path[1].first - path[0].first;
            dy = path[1].second - path[0].second;
            return true;
        }
        return false;
    }

    // Follow the committed plan one turn; when it is exhausted (or a wall
    // appeared), replan a full route and take its first step. A blocker (an
    // enemy that cut the route) is stepped on, which starts a fight that
    // clears the way - so the bot can never bounce back and forth forever.
    bool advanceToward(int tx, int ty) {
        for (int replans = 0; replans < 64; ++replans) {
            while (!plan.empty()) {
                const auto& p = state.getPlayer();
                const auto cell = plan.back();
                if (cell.first == p.x && cell.second == p.y) {
                    plan.pop_back();
                    continue;
                }
                if (state.isSolid(cell.first, cell.second)) {
                    plan.clear();
                    break;
                }
                const bool acted = movePlayer(cell.first - p.x,
                                              cell.second - p.y);
                // A descend/win/die inside movePlayer clears the plan.
                if (!plan.empty()) plan.pop_back();
                if (!acted) plan.clear();   // blocked: replan (may fight)
                return true;
            }
            std::vector<std::pair<int, int>> path;
            if (!(bfsFullPath(tx, ty, true, path) ||
                  bfsFullPath(tx, ty, false, path))) {
                return false;
            }
            if (path.size() < 2) return false;
            for (std::size_t i = path.size(); i-- > 1;) {
                plan.push_back(path[i]);   // plan.back() is the first step
            }
            // Loop back and take the first step now.
        }
        return false;
    }

    void autopilot() {
        if (state.getPhase() != RoguelikePhase::Playing) return;
        (void)advanceToward(stairsX, stairsY);
    }

    // ---- LLM actions ---------------------------------------------------------
    ActionResult moveAction(int dx, int dy) {
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
        const bool acted = movePlayer(dx, dy);
        result.success = acted;
        result.message = state.getMessage();
        result.gameOver = gameOver;
        result.gameWon = gameWon;
        return result;
    }

    // ---- Helpers / rendering ---------------------------------------------------
    int pixX(int cellX) const { return cellX * tileSize + tileSize / 2; }
    int pixY(int cellY) const { return cellY * tileSize + tileSize / 2; }

    void drawGlyph(SDL_Renderer* sdl, int x, int y, const std::string& ch,
                   SDL_Color col, float scale, int sx, int sy) const {
        drawSimpleText(sdl, x * tileSize + 6 + sx, y * tileSize + 2 + sy,
                       ch, col, scale);
    }

    // ---- Juice / HUD ----------------------------------------------------------
    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
        if (flashTimer > 0.0f) flashTimer -= dt;
        if (hurtFlashTimer > 0.0f) hurtFlashTimer -= dt;
    }

    void updateStats() {
        if (!stats) return;
        const auto& p = state.getPlayer();
        stats->setStat("HP", p.health);
        stats->setStat("ATK", p.attack);
        stats->setStat("Gold", state.getGoldCount());
        stats->setStat("Best", std::max(bestScore, state.getGoldCount()));
        stats->setStat("Level", state.getLevel());
        stats->setStat("Turn", state.getTurns());
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the Roguelike class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    try {
#ifdef __EMSCRIPTEN__
        static Roguelike game;
#else
        Roguelike game;
#endif
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
#endif
