// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Frogger - the arcade classic, game #18 of the 100-game program.
//
// Hop a frog across five lanes of traffic and a river of drifting logs to
// fill five goal slots. Cars and logs are deterministic (fixed-seed LCG),
// so every run reproduces exactly. Reach all five goals to clear a level;
// traffic gets faster each level; beat level 3 to win. A car hit or a
// drowning costs a life (respawn at the bank, goals kept); three deaths
// ends the run with a gold "NEW BEST!" celebration on a session record.
//
// Shipped with the GameJuice kit from day one (Engine/Core/GameJuice.h):
// hop pops, safe-arrival chimes, goal confetti, the car-hit splat with
// heavy shake + hit-stop, the blue drowning splash, the level-clear
// fanfare, and the win confetti - all synthesized in memory, identical
// native / WASM / headless. Pause (P) and a session best included.
//
// One code path serves human input and the LLM: the arrow keys and the
// move_up/down/left/right actions both call the exact hop(). getState()
// exposes the frog position, lives/score/level, the goal slots, and the
// per-lane car and log positions, so an agent can plan crossings like a
// human watching the road.
//
// The smoke autopilot is a genuine planner: a time-bucketed BFS over
// (row, col, t) computes a safe hop sequence to an empty goal, accounting
// for where every car and log will be - the same "wait for a gap, then
// dash" strategy a human uses. It replans whenever drift invalidates the
// current path, so headless smoke genuinely plays the whole game.
//
// Controls: arrow keys | P pause | R restart.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

class Frogger : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int WINDOW_W = 640;
    static constexpr int WINDOW_H = 640;
    static constexpr int TOP_BAND = 36;        // HUD strip
    static constexpr int COLS = 12;
    static constexpr int ROWS = 12;
    static constexpr int CELL = 50;
    static constexpr int LANE_W = COLS * CELL; // 600
    static constexpr int GRID_X = (WINDOW_W - LANE_W) / 2;
    static constexpr int GRID_Y = TOP_BAND;
    static constexpr int START_ROW = ROWS - 1; // 11
    static constexpr int START_COL = 5;
    static constexpr int MAX_LEVELS = 3;       // beat level 3 to win
    static constexpr int GOAL_COUNT = 5;
    static constexpr int GOAL_COLS[GOAL_COUNT] = {1, 3, 5, 7, 9};
    static constexpr float BOT_INTERVAL = 0.5f; // matches BFS time bucket
    static constexpr float BFS_STEP = 0.5f;
    static constexpr int BFS_STEPS = 20;       // 10s planning horizon
    static constexpr int RIVER_TOP = 1;      // river rows 1..3
    static constexpr int RIVER_BOT = 3;
    static constexpr int ROAD_TOP = 5;       // road rows 5..9
    static constexpr int ROAD_BOT = 9;
    static constexpr float FROG_HALF = CELL * 0.38f; // forgiving hitbox

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- Entities ----------------------------------------------------------
    struct Car {
        float x = 0.0f, w = 90.0f, speed = 100.0f;
        int dir = 1;
        SDL_Color color = {200, 200, 200, 255};
    };
    struct Log {
        float x = 0.0f, w = 120.0f, speed = 30.0f;
        int dir = 1;
    };
    struct Lane {
        std::vector<Car> cars;
        std::vector<Log> logs;
        float spawnTimer = 0.0f;
        // Per-lane traffic params (road).
        float speed = 100.0f;
        float interval = 2.0f;
        int maxCars = 3;
        float carW = 90.0f;
        int dir = 1;
        // Per-lane river params.
        float logSpeed = 30.0f;
        float logW = 120.0f;
        int logDir = 1;
        int logCount = 4;
    };

    Lane lanes[ROWS];
    int frogRow = START_ROW;
    float frogX = (float)(GRID_X + START_COL * CELL + CELL / 2);
    bool goalFilled[GOAL_COUNT] = {};
    float hopAnim = 0.0f;       // 0..1 hop bounce

    int score = 0;
    int bestScore = 0;          // session best; survives restarts
    int lives = 3;
    int level = 1;
    int streak = 0;             // goals without a death
    float deathGrace = 0.0f;    // brief invulnerability after respawn

    // Autopilot plan: hop deltas (dr, dc) to execute in order.
    std::vector<std::pair<int, int>> plan;
    int botGoalRow = 0;         // 0 = ascend to a goal; START_ROW = descend home
    float botTimer = 0.0f;

    // Fixed-seed LCG: cars, logs, and level layout are deterministic.
    uint32_t lcgState = 0xDEADBEEFu;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;
    std::string statusText;

public:
    Frogger() : Game2D("Frogger", WINDOW_W, WINDOW_H, 20) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hooks (not used by gameplay).
    void setFrogForTest(int row, int col) {
        frogRow = std::clamp(row, 0, ROWS - 1);
        frogX = (float)(GRID_X + std::clamp(col, 0, COLS - 1) * CELL + CELL / 2);
        plan.clear();
    }

    // Test hook: clear levels back-to-back until the win path fires.
    void forceWinForTest() {
        for (int i = 0; i < MAX_LEVELS + 1 && gameRunning; ++i) {
            for (int g = 0; g < GOAL_COUNT; ++g)
                if (!goalFilled[g]) {
                    goalFilled[g] = true;
                    streak++;
                    score += 100 * level + 25 * streak;
                }
            levelComplete();
        }
    }

    void initGame() override {
        score = 0;
        paused = false;
        lives = 3;
        level = 1;
        streak = 0;
        deathGrace = 0.0f;
        plan.clear();
        botGoalRow = 0;
        botTimer = 0.0f;
        particles.clear();
        floatTexts = uj::FloatingText{};
        for (int g = 0; g < GOAL_COUNT; ++g) goalFilled[g] = false;
        respawnFrog();
        buildLevel();

        hud = createText(10, 8, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, WINDOW_H - 22, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("Hop across the road and the river to the goals!");

        registerAction("move_up", [this]() { return hopAction(-1, 0); });
        registerAction("move_down", [this]() { return hopAction(1, 0); });
        registerAction("move_left", [this]() { return hopAction(0, -1); });
        registerAction("move_right", [this]() { return hopAction(0, 1); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_UP).onPress([this]() { (void)hop(-1, 0); });
        bindKey(KEY_DOWN).onPress([this]() { (void)hop(1, 0); });
        bindKey(KEY_LEFT).onPress([this]() { (void)hop(0, -1); });
        bindKey(KEY_RIGHT).onPress([this]() { (void)hop(0, 1); });
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

        // Advance traffic + logs (deterministic given dt).
        for (int r = 0; r < ROWS; ++r) {
            auto& lane = lanes[r];
            for (auto& car : lane.cars) {
                car.x += car.dir * car.speed * dt;
                car.x = wrap(car.x);
            }
            if (lane.dir != 0) {  // road lane: spawn cars on a timer
                lane.spawnTimer -= dt;
                if (lane.spawnTimer <= 0.0f) {
                    lane.spawnTimer = lane.interval;
                    if (static_cast<int>(lane.cars.size()) < lane.maxCars)
                        spawnCar(r);
                }
            }
            for (auto& log : lane.logs) {
                log.x += log.dir * log.speed * dt;
                log.x = wrap(log.x);
            }
        }

        // River: a frog in the water rides a log or drowns.
        if (frogRow >= RIVER_TOP && frogRow <= RIVER_BOT) {
            const Log* under = logUnder(frogRow, frogX);
            if (under == nullptr) {
                drown();
            } else {
                frogX += under->dir * under->speed * dt;
                frogX = wrap(frogX);
            }
        }

        // Road: a frog on a road lane hit by a car splats.
        if (frogRow >= ROAD_TOP && frogRow <= ROAD_BOT && deathGrace <= 0.0f) {
            if (carHits(frogRow, frogX)) {
                carHit();
                return;
            }
        }
        if (deathGrace > 0.0f) deathGrace -= dt;

        if (hopAnim > 0.0f) hopAnim -= dt * 4.0f;

        // Autopilot: follow the plan, replanning when drift invalidates it.
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

        // The playfield (shakes).
        for (int r = 0; r < ROWS; ++r) {
            const int y = GRID_Y + r * CELL + sy;
            if (r == 0) {
                drawGoalRow(sdl, y);
            } else if (r >= RIVER_TOP && r <= RIVER_BOT) {
                drawRiver(sdl, r, y);
            } else if (r >= ROAD_TOP && r <= ROAD_BOT) {
                drawRoad(sdl, r, y, sx, sy);
            } else {
                drawGrass(sdl, y, r == START_ROW);
            }
        }
        drawLogs(sdl, sx, sy);
        drawFrog(sdl, sx, sy);

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
        state.level = level;
        state.message = statusText;
        state.entities["frog"] = {colOf(frogX), frogRow};
        state.stats["score"] = score;
        state.stats["best"] = std::max(bestScore, score);
        state.stats["lives"] = lives;
        state.stats["level"] = level;
        state.stats["streak"] = streak;
        state.stats["goals_filled"] = goalsFilled();
        state.stats["frog_row"] = frogRow;
        state.stats["frog_col"] = colOf(frogX);
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        for (int g = 0; g < GOAL_COUNT; ++g)
            state.stats["goal_" + std::to_string(g)] = goalFilled[g] ? 1 : 0;
        // Per-lane traffic: positions (in px, wrapped) so an agent can
        // predict where cars and logs will be.
        for (int r = 0; r < ROWS; ++r) {
            const auto& lane = lanes[r];
            if (!lane.cars.empty()) {
                state.stats["car_l" + std::to_string(r) + "_n"] =
                    static_cast<int>(lane.cars.size());
                for (std::size_t i = 0; i < lane.cars.size(); ++i)
                    state.stats["car_l" + std::to_string(r) + "_" +
                               std::to_string(i)] =
                        static_cast<int>(lane.cars[i].x);
            }
            if (!lane.logs.empty()) {
                state.stats["log_l" + std::to_string(r) + "_n"] =
                    static_cast<int>(lane.logs.size());
                for (std::size_t i = 0; i < lane.logs.size(); ++i)
                    state.stats["log_l" + std::to_string(r) + "_" +
                               std::to_string(i)] =
                        static_cast<int>(lane.logs[i].x);
            }
        }
        return state;
    }

private:
    // ---- Geometry helpers ----------------------------------------------------
    static float wrap(float x) {
        float v = std::fmod(x, (float)LANE_W);
        if (v < 0.0f) v += (float)LANE_W;
        return v;
    }

    static int colOf(float x) {
        return std::clamp(static_cast<int>((x - GRID_X) / CELL), 0, COLS - 1);
    }

    static float cellCenter(int col) {
        return (float)(GRID_X + col * CELL + CELL / 2);
    }

    static bool inGoalCol(int col) {
        for (int g = 0; g < GOAL_COUNT; ++g)
            if (GOAL_COLS[g] == col) return true;
        return false;
    }

    // ---- Level construction ---------------------------------------------------
    void buildLevel() {
        for (int r = 0; r < ROWS; ++r) {
            lanes[r].cars.clear();
            lanes[r].logs.clear();
            lanes[r].spawnTimer = 1.0f + (lcgNext() % 100) / 100.0f;
        }

        // Road lanes (rows 5..9): faster and denser as you climb.
        const float boost = 1.0f + 0.25f * (float)(level - 1);
        const float dens = 1.0f + 0.20f * (float)(level - 1);
        const float spec[5][6] = {
            {1.0f, 60.0f, 2.4f, 2, 90.0f, 0},
            {-1.0f, 80.0f, 2.1f, 2, 100.0f, 0},
            {1.0f, 100.0f, 1.9f, 3, 80.0f, 0},
            {-1.0f, 125.0f, 1.7f, 3, 90.0f, 0},
            {1.0f, 150.0f, 1.5f, 3, 70.0f, 0},
        };
        for (int i = 0; i < 5; ++i) {
            const int r = ROAD_TOP + i;
            auto& lane = lanes[r];
            lane.dir = (int)spec[i][0];
            lane.speed = spec[i][1] * boost;
            lane.interval = spec[i][2] / dens;
            lane.maxCars = (int)spec[i][3];
            lane.carW = spec[i][4];
            // A couple of cars already rolling for immediate action.
            for (int k = 0; k < lane.maxCars; ++k)
                spawnCarAt(r, (lcgNext() % LANE_W));
        }

        // River lanes (rows 1..3): deterministic drifting logs.
        const float logSpec[3][4] = {
            {1.0f, 26.0f, 150.0f, 4},   // right, slow, long
            {-1.0f, 33.0f, 120.0f, 4},  // left, mid
            {1.0f, 40.0f, 100.0f, 5},   // right, faster, short
        };
        for (int i = 0; i < 3; ++i) {
            const int r = RIVER_TOP + i;
            auto& lane = lanes[r];
            lane.dir = 0;  // river lanes don't spawn cars
            lane.logDir = (int)logSpec[i][0];
            lane.logSpeed = logSpec[i][1] * (1.0f + 0.15f * (float)(level - 1));
            lane.logW = logSpec[i][2];
            const int count = (int)logSpec[i][3];
            lane.logCount = count;
            for (int k = 0; k < count; ++k) {
                Log log;
                log.dir = lane.logDir;
                log.speed = lane.logSpeed;
                log.w = lane.logW;
                log.x = wrap((float)(k * LANE_W) / (float)count +
                             (float)(lcgNext() % 60));
                lane.logs.push_back(log);
            }
        }
    }

    void spawnCar(int r) {
        auto& lane = lanes[r];
        Car car;
        car.dir = lane.dir;
        car.speed = lane.speed;
        car.w = lane.carW;
        car.x = lane.dir > 0 ? (float)-car.w : (float)LANE_W;
        const SDL_Color palette[4] = {
            {230, 90, 80, 255}, {90, 170, 230, 255},
            {240, 200, 70, 255}, {170, 110, 230, 255},
        };
        car.color = palette[lcgNext() % 4];
        lane.cars.push_back(car);
    }

    void spawnCarAt(int r, float x) {
        auto& lane = lanes[r];
        Car car;
        car.dir = lane.dir;
        car.speed = lane.speed;
        car.w = lane.carW;
        car.x = wrap(x);
        const SDL_Color palette[4] = {
            {230, 90, 80, 255}, {90, 170, 230, 255},
            {240, 200, 70, 255}, {170, 110, 230, 255},
        };
        car.color = palette[lcgNext() % 4];
        lane.cars.push_back(car);
    }

    // ---- Frog movement ---------------------------------------------------------
    bool hop(int dr, int dc) {
        if (!gameRunning || paused) return false;
        const int nr = frogRow + dr;
        const int nc = colOf(frogX) + dc;
        if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) return false;
        // Can't hop down past the bank or up into a filled goal slot.
        if (nr > START_ROW) return false;
        if (nr == 0) {
            if (!inGoalCol(nc) || goalFilled[goalIndex(nc)]) return false;
        }
        frogRow = nr;
        frogX = cellCenter(nc);
        hopAnim = 1.0f;
        sfx.play(uj::Sfx::Thock);
        particles.burst(frogX, (float)(GRID_Y + frogRow * CELL + CELL - 6), 3,
                        {170, 220, 140, 255}, 2.5f, 0.2f, 2.0f);

        // Landing on a goal slot?
        if (nr == 0) {
            const int gi = goalIndex(nc);
            if (!goalFilled[gi]) {
                goalFilled[gi] = true;
                reachGoal(gi);
            }
        }
        return true;
    }

    int goalIndex(int col) const {
        for (int g = 0; g < GOAL_COUNT; ++g)
            if (GOAL_COLS[g] == col) return g;
        return -1;
    }

    int goalsFilled() const {
        int n = 0;
        for (int g = 0; g < GOAL_COUNT; ++g)
            if (goalFilled[g]) ++n;
        return n;
    }

    void reachGoal(int gi) {
        ++streak;
        const int gain = 100 * level + 25 * streak;
        score += gain;

        // ---- Juice: goal chime + confetti -------------------------------------
        sfx.play(uj::Sfx::Clear);
        shake.add(0.15f);
        const float gx = cellCenter(GOAL_COLS[gi]);
        const float gy = (float)(GRID_Y + CELL / 2);
        particles.burst(gx, gy, 12, {255, 220, 90, 255}, 5.0f, 0.5f, 4.0f);
        floatTexts.spawn(std::make_shared<TextDisplay>((int)gx - 24, (int)gy - 20,
                                                       "+" + std::to_string(gain)),
                         (int)gx - 24, (int)gy - 20);
        updateHUD();
        setMessage("Goal " + std::to_string(goalsFilled()) + "/" +
                   std::to_string(GOAL_COUNT) + "!");

        if (goalsFilled() >= GOAL_COUNT) levelComplete();
    }

    void levelComplete() {
        // ---- Juice: level fanfare ----------------------------------------------
        sfx.play(uj::Sfx::Win);
        shake.add(0.4f);
        const int bonus = 250 * level;
        score += bonus;
        floatTexts.spawn(std::make_shared<TextDisplay>(
                             WINDOW_W / 2 - 70, TOP_BAND + 130,
                             "LEVEL " + std::to_string(level) + " CLEAR +" +
                                 std::to_string(bonus)),
                         WINDOW_W / 2 - 70, TOP_BAND + 130);
        for (int i = 0; i < 3; ++i)
            particles.burst((float)(WINDOW_W / 2 + (i - 1) * 100),
                            (float)(GRID_Y + 60), 16,
                            i % 2 == 0 ? SDL_Color{140, 255, 120, 255} :
                                         SDL_Color{80, 220, 255, 255},
                            8.0f, 0.7f, 5.0f);

        if (level >= MAX_LEVELS) {
            winGame();
            return;
        }
        ++level;
        for (int g = 0; g < GOAL_COUNT; ++g) goalFilled[g] = false;
        respawnFrog();
        buildLevel();
        plan.clear();
        updateHUD();
        setMessage("Level " + std::to_string(level) + " - faster traffic!");
    }

    // ---- Deaths ------------------------------------------------------------------
    bool carHits(int row, float fx) const {
        const float f0 = fx - FROG_HALF, f1 = fx + FROG_HALF;
        for (const auto& car : lanes[row].cars) {
            // Car interval [x, x+w], possibly wrapped.
            if (overlapWrapped(f0, f1, car.x, car.x + car.w)) return true;
        }
        return false;
    }

    static bool overlapWrapped(float a0, float a1, float b0, float b1) {
        if (a1 <= b0 || b1 <= a0) {
            // Check the wrapped second half of the car interval.
            if (b1 > LANE_W) {
                const float bw0 = 0.0f, bw1 = b1 - (float)LANE_W;
                if (a0 < bw1 && bw0 < a1) return true;
            }
            if (b0 < 0.0f) {
                const float bw0 = (float)LANE_W + b0, bw1 = (float)LANE_W;
                if (a0 < bw1 && bw0 < a1) return true;
            }
            return false;
        }
        return true;
    }

    const Log* logUnder(int row, float fx) const {
        for (const auto& log : lanes[row].logs) {
            const float l0 = log.x, l1 = log.x + log.w;
            if (l1 <= LANE_W) {
                if (fx >= l0 && fx < l1) return &log;
            } else {
                if (fx >= l0 || fx < l1 - (float)LANE_W) return &log;
            }
        }
        return nullptr;
    }

    void respawnFrog() {
        frogRow = START_ROW;
        frogX = cellCenter(START_COL);
        plan.clear();
        botGoalRow = 0;
        hopAnim = 0.0f;
        deathGrace = 1.0f;  // spawn is on the bank; grace for safety
    }

    void carHit() {
        --lives;
        streak = 0;
        // ---- Juice: splat + shake + hit-stop -----------------------------------
        sfx.play(uj::Sfx::Explode);
        shake.add(0.65f);
        hitStop.trigger(0.16f);
        particles.burst(frogX, (float)(GRID_Y + frogRow * CELL + CELL / 2), 22,
                        {255, 80, 50, 255}, 11.0f, 0.7f, 5.5f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
                             (int)frogX - 30, (int)(GRID_Y + frogRow * CELL) - 20,
                             "SPLAT!"),
                         (int)frogX - 30, (int)(GRID_Y + frogRow * CELL) - 20);
        onDeath();
    }

    void drown() {
        --lives;
        streak = 0;
        // ---- Juice: blue splash --------------------------------------------------
        sfx.play(uj::Sfx::Lose);
        shake.add(0.5f);
        hitStop.trigger(0.12f);
        particles.burst(frogX, (float)(GRID_Y + frogRow * CELL + CELL / 2), 16,
                        {90, 180, 240, 255}, 7.0f, 0.5f, 4.0f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
                             (int)frogX - 32, (int)(GRID_Y + frogRow * CELL) - 20,
                             "SPLASH!"),
                         (int)frogX - 32, (int)(GRID_Y + frogRow * CELL) - 20);
        onDeath();
    }

    void onDeath() {
        if (lives <= 0) {
            loseGame();
            return;
        }
        respawnFrog();
        updateHUD();
        setMessage(std::to_string(lives) + " lives left - goals kept");
    }

    void loseGame() {
        const bool newBest = score > bestScore;
        bestScore = std::max(bestScore, score);
        // ---- Juice: game over burst ----------------------------------------------
        sfx.play(uj::Sfx::Lose);
        shake.add(0.6f);
        hitStop.trigger(0.15f);
        particles.burst((float)(WINDOW_W / 2), (float)(GRID_Y + 200), 20,
                        {255, 60, 50, 255}, 10.0f, 0.6f, 5.0f);
        if (newBest && score > 0) {
            sfx.play(uj::Sfx::Win);
            particles.burst((float)(WINDOW_W / 2), (float)(GRID_Y + 240), 14,
                            {230, 200, 60, 255}, 8.0f, 0.7f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                                 WINDOW_W / 2 - 60, GRID_Y + 160, "NEW BEST!"),
                             WINDOW_W / 2 - 60, GRID_Y + 160);
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
        for (int i = 0; i < 5; ++i) {
            particles.burst((float)(WINDOW_W / 2 + (i - 2) * 80),
                            (float)(GRID_Y + 160), 20,
                            i % 3 == 0 ? SDL_Color{255, 220, 60, 255} :
                            i % 3 == 1 ? SDL_Color{80, 220, 255, 255} :
                                         SDL_Color{140, 255, 120, 255},
                            10.0f, 0.9f, 6.0f);
        }
        floatTexts.spawn(std::make_shared<TextDisplay>(
                             WINDOW_W / 2 - 60, GRID_Y + 100, "YOU WIN!"),
                         WINDOW_W / 2 - 60, GRID_Y + 100);
        setMessage("YOU WIN! Best " + std::to_string(bestScore) +
                   " - Press R to play again");
        updateHUD();
        gameWon = true;
        endGame();
    }

    // ---- Autopilot: time-bucketed BFS --------------------------------------------
    bool botSafeAt(int row, int col, float t) const {
        const float fx = cellCenter(col);
        if (row == 0) {
            if (!inGoalCol(col)) return false;
            const int gi = goalIndex(col);
            return gi >= 0 && !goalFilled[gi];
        }
        if (row == 4 || row == 10 || row == START_ROW) return true;
        if (row >= ROAD_TOP && row <= ROAD_BOT) {
            const float f0 = fx - FROG_HALF, f1 = fx + FROG_HALF;
            for (const auto& car : lanes[row].cars) {
                const float cx = wrap(car.x + car.dir * car.speed * t);
                if (overlapWrapped(f0, f1, cx, cx + car.w)) return false;
            }
            return true;
        }
        if (row >= RIVER_TOP && row <= RIVER_BOT) {
            for (const auto& log : lanes[row].logs) {
                const float lx = wrap(log.x + log.dir * log.speed * t);
                const float l0 = lx, l1 = lx + log.w;
                if (l1 <= LANE_W) {
                    if (fx >= l0 && fx < l1) return true;
                } else {
                    if (fx >= l0 || fx < l1 - (float)LANE_W) return true;
                }
            }
            return false;
        }
        return true;
    }

    // A road cell must be clear for the whole occupancy window (arrival
    // and departure); a river cell just needs a log under the frog at
    // arrival (the log then carries it).
    bool botCrossable(int row, int col, float t) const {
        if (row >= ROAD_TOP && row <= ROAD_BOT) {
            return botSafeAt(row, col, t) &&
                   botSafeAt(row, col, t + BFS_STEP);
        }
        return botSafeAt(row, col, t);
    }

    // BFS over (row, col, step). Returns a hop sequence, or empty if no
    // path exists within the horizon.
    std::vector<std::pair<int, int>> findPlan() const {
        const int startCol = colOf(frogX);
        struct Node { int row, col, step; };
        // visited[row][col][step]
        std::vector<std::vector<std::vector<bool>>> visited(
            (std::size_t)ROWS,
            std::vector<std::vector<bool>>(
                (std::size_t)COLS, std::vector<bool>((std::size_t)BFS_STEPS + 1, false)));
        std::vector<std::vector<std::vector<std::pair<int, int>>>> parent(
            (std::size_t)ROWS,
            std::vector<std::vector<std::pair<int, int>>>(
                (std::size_t)COLS, std::vector<std::pair<int, int>>((std::size_t)BFS_STEPS + 1, {-1, -1})));
        std::queue<Node> q;
        q.push({frogRow, startCol, 0});
        visited[(std::size_t)frogRow][(std::size_t)startCol][0] = true;

        const int dr[4] = {-1, 1, 0, 0};
        const int dc[4] = {0, 0, -1, 1};

        while (!q.empty()) {
            const Node n = q.front();
            q.pop();
            if (n.row == botGoalRow) {  // reached the phase goal
                std::vector<std::pair<int, int>> hops;
                int cr = n.row, cc = n.col, cs = n.step;
                while (!(cr == frogRow && cc == startCol && cs == 0)) {
                    const auto& p = parent[(std::size_t)cr][(std::size_t)cc][(std::size_t)cs];
                    hops.push_back({cr - p.first, cc - p.second});
                    cr = p.first;
                    cc = p.second;
                    --cs;
                    if (cs < 0) break;
                }
                std::reverse(hops.begin(), hops.end());
                return hops;
            }
            if (n.step >= BFS_STEPS) continue;
            const float tNext = (float)(n.step + 1) * BFS_STEP;
            // Hop to each neighbor.
            for (int d = 0; d < 4; ++d) {
                const int nr = n.row + dr[d], nc = n.col + dc[d];
                if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) continue;
                if (nr > START_ROW) continue;
                if (!botCrossable(nr, nc, tNext)) continue;
                if (visited[(std::size_t)nr][(std::size_t)nc][(std::size_t)n.step + 1])
                    continue;
                visited[(std::size_t)nr][(std::size_t)nc][(std::size_t)n.step + 1] = true;
                parent[(std::size_t)nr][(std::size_t)nc][(std::size_t)n.step + 1] = {n.row, n.col};
                q.push({nr, nc, n.step + 1});
            }
            // Wait in place on a safe (grass/bank) row; during the descent
            // a filled goal slot is solid ground too.
            if (n.row == 4 || n.row == 10 || n.row == START_ROW ||
                (botGoalRow == START_ROW && n.row == 0)) {
                if (!visited[(std::size_t)n.row][(std::size_t)n.col][(std::size_t)n.step + 1]) {
                    visited[(std::size_t)n.row][(std::size_t)n.col][(std::size_t)n.step + 1] = true;
                    parent[(std::size_t)n.row][(std::size_t)n.col][(std::size_t)n.step + 1] = {n.row, n.col};
                    q.push({n.row, n.col, n.step + 1});
                }
            }
        }
        return {};
    }

    void botStep() {
        if (!gameRunning) return;

        // Phase transitions: filled a goal -> head back to the bank;
        // back on the bank -> head out again.
        if (botGoalRow == 0 && frogRow == 0) {
            botGoalRow = START_ROW;
        } else if (botGoalRow == START_ROW && frogRow == START_ROW) {
            botGoalRow = 0;
        }

        // Validate the plan's next hop against current traffic before
        // committing (arrival + occupancy window); replan from scratch if
        // drift invalidated it.
        if (!plan.empty()) {
            const auto& next = plan.front();
            const int nr = frogRow + next.first;
            const int nc = colOf(frogX) + next.second;
            const bool ok = nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS &&
                            nr <= START_ROW && botCrossable(nr, nc, 0.0f);
            if (ok) {
                const auto h = plan.front();
                plan.erase(plan.begin());
                (void)hop(h.first, h.second);
                return;
            }
            plan.clear();
        }

        // No valid plan: on a road row we must retreat to grass first.
        if (frogRow >= ROAD_TOP && frogRow <= ROAD_BOT) {
            const int target = frogRow <= 6 ? 4 : 10;
            if (frogRow != target) {
                (void)hop(frogRow < target ? 1 : -1, 0);
                return;
            }
        }

        plan = findPlan();
        // A freshly adopted plan waits one interval before its first hop so
        // execution matches the BFS timeline (hop k lands at (k+1)*BFS_STEP).
    }

    // ---- LLM actions -------------------------------------------------------------
    ActionResult hopAction(int dr, int dc) {
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
        if (!hop(dr, dc)) {
            result.message = "Can't hop there (edge, bank, or filled goal)";
            return result;
        }
        result.success = true;
        result.message = "Frog at row " + std::to_string(frogRow) +
                         ", col " + std::to_string(colOf(frogX));
        result.scoreChange = 0;
        result.gameOver = gameOver;
        result.gameWon = gameWon;
        return result;
    }

    // ---- Rendering -----------------------------------------------------------------
    void drawGoalRow(SDL_Renderer* sdl, int y) const {
        SDL_SetRenderDrawColor(sdl, 20, 48, 90, 255);   // water gaps
        SDL_Rect water = {GRID_X, y, LANE_W, CELL};
        SDL_RenderFillRect(sdl, &water);
        for (int g = 0; g < GOAL_COUNT; ++g) {
            const int x = GRID_X + GOAL_COLS[g] * CELL;
            SDL_SetRenderDrawColor(sdl, goalFilled[g] ? 120 : 70,
                                   goalFilled[g] ? 200 : 140,
                                   goalFilled[g] ? 90 : 60, 255);
            SDL_Rect slot = {x + 4, y + 4, CELL - 8, CELL - 8};
            SDL_RenderFillRect(sdl, &slot);
            if (goalFilled[g]) {
                SDL_SetRenderDrawColor(sdl, 255, 240, 150, 255);
                SDL_Rect star = {x + CELL / 2 - 8, y + CELL / 2 - 8, 16, 16};
                SDL_RenderFillRect(sdl, &star);
            }
        }
    }

    void drawRiver(SDL_Renderer* sdl, int row, int y) const {
        SDL_SetRenderDrawColor(sdl, 24, 62, 110, 255);
        SDL_Rect water = {GRID_X, y, LANE_W, CELL};
        SDL_RenderFillRect(sdl, &water);
        // Ripple dashes.
        SDL_SetRenderDrawColor(sdl, 60, 120, 180, 255);
        for (int x = GRID_X + 12; x < GRID_X + LANE_W - 12; x += 46) {
            SDL_Rect ripple = {x + ((row * 17) % 20), y + 10, 18, 2};
            SDL_RenderFillRect(sdl, &ripple);
            SDL_Rect ripple2 = {x + 24, y + CELL - 16, 14, 2};
            SDL_RenderFillRect(sdl, &ripple2);
        }
        // Logs are drawn after the frog so they pass over it.
    }

    void drawLogs(SDL_Renderer* sdl, int sx, int sy) const {
        for (int r = RIVER_TOP; r <= RIVER_BOT; ++r) {
            for (const auto& log : lanes[r].logs) {
                const int y = GRID_Y + r * CELL + 6 + sy;
                int x0 = (int)log.x + sx;
                // Draw wrapped log in up to two pieces.
                int x1 = x0 + (int)log.w;
                if (x1 > GRID_X + LANE_W) x1 = GRID_X + LANE_W;
                if (x1 > x0) {
                    SDL_SetRenderDrawColor(sdl, 148, 108, 58, 255);
                    SDL_Rect body = {x0, y, x1 - x0, CELL - 12};
                    SDL_RenderFillRect(sdl, &body);
                    SDL_SetRenderDrawColor(sdl, 96, 64, 32, 255);
                    for (int kx = x0 + 12; kx < x1 - 8; kx += 22) {
                        SDL_Rect ring = {kx, y, 5, CELL - 12};
                        SDL_RenderFillRect(sdl, &ring);
                    }
                }
                if (x0 + (int)log.w > GRID_X + LANE_W) {
                    const int xw = x0 + (int)log.w - (GRID_X + LANE_W);
                    if (xw > 0) {
                        const int xr = GRID_X + sx;
                        SDL_SetRenderDrawColor(sdl, 148, 108, 58, 255);
                        SDL_Rect body = {xr, y, xw, CELL - 12};
                        SDL_RenderFillRect(sdl, &body);
                    }
                }
            }
        }
    }

    void drawRoad(SDL_Renderer* sdl, int row, int y, int sx, int sy) const {
        SDL_SetRenderDrawColor(sdl, 34, 36, 40, 255);
        SDL_Rect asphalt = {GRID_X, y, LANE_W, CELL};
        SDL_RenderFillRect(sdl, &asphalt);
        SDL_SetRenderDrawColor(sdl, 60, 64, 70, 255);
        SDL_Rect dash = {GRID_X, y + CELL - 8, LANE_W, 3};
        SDL_RenderFillRect(sdl, &dash);
        for (const auto& car : lanes[row].cars) {
            drawCar(sdl, car, row, sx, sy);
        }
    }

    void drawCar(SDL_Renderer* sdl, const Car& car, int row,
                 int sx, int sy) const {
        const int x0 = (int)car.x + sx;
        const int y = GRID_Y + row * CELL + 6 + sy;
        const int x1 = x0 + (int)car.w;
        auto fill = [&](int a, int b) {
            if (b <= a) return;
            SDL_SetRenderDrawColor(sdl, car.color.r, car.color.g,
                                   car.color.b, 255);
            SDL_Rect body = {a, y, b - a, CELL - 12};
            SDL_RenderFillRect(sdl, &body);
            SDL_SetRenderDrawColor(sdl, 210, 224, 235, 255);
            SDL_Rect win = {a + 8, y + 8, std::max(4, (b - a) / 3), 8};
            SDL_RenderFillRect(sdl, &win);
        };
        fill(x0, std::min(x1, GRID_X + LANE_W));
        if (x1 > GRID_X + LANE_W) fill(GRID_X + sx, x1 - (GRID_X + LANE_W));
    }

    void drawGrass(SDL_Renderer* sdl, int y, bool start) const {
        if (start) {
            SDL_SetRenderDrawColor(sdl, 34, 84, 44, 255);
        } else {
            SDL_SetRenderDrawColor(sdl, 40, 96, 52, 255);
        }
        SDL_Rect grass = {GRID_X, y, LANE_W, CELL};
        SDL_RenderFillRect(sdl, &grass);
        SDL_SetRenderDrawColor(sdl, 30, 70, 40, 255);
        for (int x = GRID_X + 6; x < GRID_X + LANE_W - 6; x += 34) {
            SDL_Rect tuft = {x, y + CELL - 12, 3, 8};
            SDL_RenderFillRect(sdl, &tuft);
        }
    }

    void drawFrog(SDL_Renderer* sdl, int sx, int sy) const {
        const int fx = (int)frogX + sx;
        const int fy = GRID_Y + frogRow * CELL + CELL / 2 + sy;
        const int bounce = hopAnim > 0.0f ? (int)(hopAnim * 14.0f) : 0;
        const int y = fy - bounce;
        const int x = fx;
        SDL_SetRenderDrawColor(sdl, 60, 170, 70, 255);
        SDL_Rect body = {x - 17, y - 12, 34, 24};
        SDL_RenderFillRect(sdl, &body);
        SDL_Rect head = {x - 12, y - 20, 24, 16};
        SDL_RenderFillRect(sdl, &head);
        // Eyes.
        SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
        SDL_Rect eyeL = {x - 10, y - 26, 9, 9};
        SDL_Rect eyeR = {x + 1, y - 26, 9, 9};
        SDL_RenderFillRect(sdl, &eyeL);
        SDL_RenderFillRect(sdl, &eyeR);
        SDL_SetRenderDrawColor(sdl, 20, 20, 20, 255);
        SDL_Rect pupL = {x - 7, y - 23, 4, 4};
        SDL_Rect pupR = {x + 4, y - 23, 4, 4};
        SDL_RenderFillRect(sdl, &pupL);
        SDL_RenderFillRect(sdl, &pupR);
        // Legs.
        SDL_SetRenderDrawColor(sdl, 52, 150, 62, 255);
        SDL_Rect legL = {x - 21, y - 2, 7, 8};
        SDL_Rect legR = {x + 14, y - 2, 7, 8};
        SDL_RenderFillRect(sdl, &legL);
        SDL_RenderFillRect(sdl, &legR);
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
        hud->setText("Level " + std::to_string(level) + "/" +
                     std::to_string(MAX_LEVELS) +
                     "    Goals " + std::to_string(goalsFilled()) + "/" +
                     std::to_string(GOAL_COUNT) +
                     "    Lives " + std::to_string(lives) +
                     "    Score " + std::to_string(score) +
                     "    Best " + std::to_string(std::max(bestScore, score)));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the Frogger class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static Frogger game;
#else
    Frogger game;
#endif
    game.run();
    return 0;
}
#endif
