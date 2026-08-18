// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Galaga - the classic fixed-position shooter, game #20 of the 100-game
// program.
//
// A marching formation of bug fighters weaves side to side and drops toward
// your ship while, on a schedule, lone bugs peel out of the formation and
// dive at you on a looping sine path (the signature Galaga move). You sweep
// the bottom and fire up to two shots at a time; clear the swarm and the
// next wave deploys - faster, denser, and with more divers. Beat three waves
// and you win.
//
// It ships to the AAA-feel bar (see GAMES.md) via Engine/Core/GameJuice.h:
// every kill bursts into particles, shake, and hit-stop with a floating
// score pop; divers pay double; a dive-out fires a swoosh; wave clears
// celebrate with a fanfare and confetti. Losing your ship runs the full
// arcade beat - hidden respawn, then an invulnerable blink - through the
// kit's ShipRespawn helper.
//
// One code path serves human input and the LLM: the keyboard (A/D, arrows,
// mouse) polls the same clampPlayer() as the "move_left"/"move_right"
// actions, and SPACE / "fire" share doFire(). An LLM can play the exact game
// a human plays.
//
// Controls: A/D or arrows = move ship (mouse also works), SPACE = fire,
//           P = pause, R = restart. Three lives, clear three waves = win.
//
// All "randomness" (which bug dives, dive sway phase) flows through a
// fixed-seed LCG so a given playthrough is fully deterministic and
// unit-testable.

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

class Galaga : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 80;          // 80x50 cells @ 12px = 960x600
    static constexpr int GRID_H = 50;

    // Player ship.
    static constexpr int PLAYER_W = 3;
    static constexpr int PLAYER_H = 2;
    static constexpr int PLAYER_Y = GRID_H - 5;   // top cell of the ship
    static constexpr float PLAYER_SPEED = 40.0f;  // cells/second
    static constexpr float LLM_PLAYER_STEP = 5.0f;
    static constexpr int MAX_LIVES = 3;
    static constexpr float BULLET_SPEED = 24.0f;  // cells/second upward
    static constexpr int MAX_BULLETS = 2;         // Galaga's classic 2-shot

    // Formation: up to 10 columns x 5 rows of 3x3-cell sprites.
    static constexpr int MAX_COLS = 10;
    static constexpr int MAX_ROWS = 5;
    static constexpr int SPR_W = 3;
    static constexpr int SPR_H = 3;
    static constexpr int COL_STEP = 5;            // horizontal spacing
    static constexpr int ROW_STEP = 4;            // vertical spacing
    static constexpr float FX_START = 20.0f;
    static constexpr float FY_START = 5.0f;
    static constexpr int DROP_STEP = 2;           // cells dropped per edge hit
    static constexpr float MARCH_MIN = 0.15f;     // fastest march

    // Top rows are worth more (Galaga-like scoring).
    static constexpr int ROW_POINTS[MAX_ROWS] = {100, 80, 60, 40, 20};
    static constexpr int DIVE_BONUS = 2;          // divers pay double

    // Diving bugs.
    static constexpr float SWAY_AMP = 6.0f;       // horizontal sine sway (cells)
    static constexpr float SWAY_FREQ = 4.0f;      // rad/s
    static constexpr float RETURN_SPEED = 10.0f;  // cells/s back to the slot
    static constexpr int MAX_DIVERS = 3;          // concurrent divers
    static constexpr float DIVE_INTERVAL_MIN = 0.8f;

    // Wave config: cols, rows, march step interval, dive interval, dive speed.
    struct WaveConfig {
        int cols, rows;
        float march;
        float diveInterval;
        float diveSpeed;
    };
    static constexpr WaveConfig WAVES[3] = {
        {8, 4, 0.9f, 4.0f, 12.0f},
        {9, 4, 0.7f, 3.0f, 14.0f},
        {10, 5, 0.55f, 2.2f, 16.0f},
    };
    static constexpr int WAVES_TO_WIN = 3;

    // ---- World ------------------------------------------------------------
    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- State ------------------------------------------------------------
    int formation[MAX_ROWS][MAX_COLS];  // 1 = alive in formation, 0 = gone
    int waveCols = 0, waveRows = 0;
    int aliveCount = 0;                 // formation alive + divers
    int wave = 1;
    float fx = FX_START, fy = FY_START; // formation origin (cells)
    int marchDir = 1;
    float marchTimer = 0.0f;
    float diveTimer = 0.0f;
    int animFrame = 0;                  // two-frame march animation
    int diveCount = 0;                  // cumulative dives this session

    struct Diver {
        int fromC, fromR;               // formation slot (-1 = test diver)
        float baseX, x, y;              // position (cells)
        float sway;                     // sine phase
        int state;                      // 0 = diving, 1 = returning
    };
    std::vector<Diver> divers;

    float playerX = 0.0f;
    int lives = MAX_LIVES;
    int score = 0;
    int bestScore = 0;                  // session best; survives restarts
    uj::ShipRespawn respawn{1.2f, 2.0f, 20.0f};
    uj::ProjectilePool bullets;         // player shots (tag 0), capped at 2

    // Fixed-seed LCG: every playthrough (and test) is deterministic.
    uint32_t lcgState = 0xC0FFEEu;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;
    float smokeSweep = 0.0f;

public:
    Galaga() : Game2D("Galaga", 960, 600, 12) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hooks: force a diver onto the ship, or clear the current wave.
    void spawnDiverForTest(float x, float y) {
        Diver d;
        d.fromC = -1;
        d.fromR = -1;
        d.baseX = x;
        d.x = x;
        d.y = y;
        d.sway = 0.0f;
        d.state = 0;
        divers.push_back(d);
    }

    void forceWaveClearForTest() {
        for (int r = 0; r < MAX_ROWS; ++r)
            for (int c = 0; c < MAX_COLS; ++c)
                formation[r][c] = 0;
        divers.clear();
        aliveCount = 0;
        // Drive the clear/win path directly: routing through tick() would
        // be swallowed by the hit-stop the previous clear just triggered.
        onWaveCleared();
    }

    void initGame() override {
        score = 0;
        lives = MAX_LIVES;
        paused = false;
        diveCount = 0;
        smokeSweep = 0.0f;
        particles.clear();
        floatTexts = uj::FloatingText{};
        bullets.setCap(MAX_BULLETS);
        lcgState = 0xC0FFEEu;   // deterministic run each reset
        startWave(1);

        // Court: dark background painted once through the grid.
        createGrid(GRID_W, GRID_H, tileSize);
        grid->fill({5, 6, 16, 255});
        grid->setBorderColor({12, 14, 28, 255});

        hud = createText(10, 6, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, GRID_H * tileSize - 26, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("SPACE or 'fire' to shoot");

        registerAction("move_left", [this]() { return movePlayer(-1); });
        registerAction("move_right", [this]() { return movePlayer(1); });
        registerAction("fire", [this]() { return doFire(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_SPACE).onPress([this]() { doFire(); });
        bindKey(KEY_P).onPress([this]() { paused = !paused; });
        bindKey(KEY_R).onPress([this]() {
            if (gameOver) startGame();
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

        if (input.isKeyHeld(KEY_A) || input.isKeyHeld(KEY_LEFT)) clampPlayerBy(-PLAYER_SPEED * dt);
        if (input.isKeyHeld(KEY_D) || input.isKeyHeld(KEY_RIGHT)) clampPlayerBy(PLAYER_SPEED * dt);

        // Headless smoke mode: sweep the ship and keep a two-shot stream in
        // flight so a dummy-driver run exercises march -> dive -> collision ->
        // score -> respawn. (The engine auto-restarts on game over.)
        if (smokeMode) {
            smokeSweep += dt;
            const float period = 3.0f;
            const int dir = (static_cast<int>(smokeSweep / period) % 2 == 0) ? 1 : -1;
            clampPlayerBy(static_cast<float>(dir) * 36.0f * dt);
            if (!respawn.waiting()) (void)doFire();
        }

        if (respawn.update(dt)) setMessage("Respawned");

        stepFormation(dt);
        stepDivers(dt);
        stepBullets(dt);

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // The marching formation: 3x3-cell pixel-art bugs, two-frame
        // animation, one color per row (top = most points).
        for (int r = 0; r < waveRows; ++r) {
            const SDL_Color col = enemyColor(r);
            for (int c = 0; c < waveCols; ++c) {
                if (!formation[r][c]) continue;
                const int ox = static_cast<int>(std::lround(fx)) + c * COL_STEP;
                const int oy = static_cast<int>(std::lround(fy)) + r * ROW_STEP;
                drawBug(sdl, ox * tileSize + sx, oy * tileSize + sy, col, animFrame);
            }
        }

        // Diving bugs: the same shape, tinted hot so they read as a threat.
        for (const Diver& d : divers) {
            const SDL_Color col = (d.fromR >= 0)
                ? enemyColor(d.fromR)
                : SDL_Color{255, 120, 200, 255};
            drawBug(sdl, static_cast<int>(std::lround(d.x)) * tileSize + sx,
                    static_cast<int>(std::lround(d.y)) * tileSize + sy, col,
                    animFrame);
        }

        // Player bullets.
        for (const auto& p : bullets.all()) {
            SDL_Rect b = {
                static_cast<int>(std::lround(p.x)) * tileSize + sx,
                static_cast<int>(std::lround(p.y)) * tileSize + sy,
                tileSize, tileSize * 2
            };
            SDL_SetRenderDrawColor(sdl, 240, 240, 240, 255);
            SDL_RenderFillRect(sdl, &b);
        }

        // Player ship (hidden during respawn, blinking while invulnerable).
        if (respawn.visible()) {
            const int px = static_cast<int>(std::lround(playerX));
            SDL_Rect body = {px * tileSize + sx, (PLAYER_Y + 1) * tileSize + sy,
                             PLAYER_W * tileSize, tileSize};
            SDL_Rect nose = {(px + 1) * tileSize + sx, PLAYER_Y * tileSize + sy,
                             tileSize, tileSize};
            SDL_SetRenderDrawColor(sdl, 120, 235, 255, 255);
            SDL_RenderFillRect(sdl, &body);
            SDL_RenderFillRect(sdl, &nose);
        }

        particles.render(sdl, sx, sy);
        floatTexts.render(getRenderer());

        if (paused) {
            SDL_Rect veil = {0, 0, GRID_W * tileSize, GRID_H * tileSize};
            SDL_SetRenderDrawColor(sdl, 0, 0, 0, 140);
            SDL_RenderFillRect(sdl, &veil);
        }

        for (auto& t : textDisplays) t->render(getRenderer());
    }

    // ---- LLM state ----------------------------------------------------------
    GameState getState() const override {
        GameState state = Game2D::getState();
        state.score = score;
        state.level = wave;
        state.message = statusText;
        state.stats["score"] = score;
        state.stats["best"] = std::max(bestScore, score);
        state.stats["lives"] = lives;
        state.stats["wave"] = wave;
        state.stats["enemies_left"] = aliveCount;
        state.stats["player_x"] = static_cast<int>(std::lround(playerX));
        state.stats["player_y"] = PLAYER_Y;
        state.stats["formation_x"] = static_cast<int>(std::lround(fx));
        state.stats["formation_y"] = static_cast<int>(std::lround(fy));
        state.stats["divers"] = static_cast<int>(divers.size());
        state.stats["dive_count"] = diveCount;
        state.stats["bullets"] = static_cast<int>(bullets.size());
        state.stats["hittable"] = respawn.hittable() ? 1 : 0;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["shake"] = static_cast<int>(shake.level() * 100.0f);
        state.stats["particles"] = particles.count();
        state.entities["player"] = {
            static_cast<int>(std::lround(playerX)), PLAYER_Y
        };
        return state;
    }

private:
    // ---- Wave / formation ----------------------------------------------------
    const WaveConfig& currentWave() const { return WAVES[wave - 1]; }

    void startWave(int w) {
        wave = w;
        const WaveConfig& cfg = currentWave();
        waveCols = cfg.cols;
        waveRows = cfg.rows;
        for (int r = 0; r < MAX_ROWS; ++r)
            for (int c = 0; c < MAX_COLS; ++c)
                formation[r][c] = 0;
        aliveCount = 0;
        for (int r = 0; r < waveRows; ++r)
            for (int c = 0; c < waveCols; ++c) {
                formation[r][c] = 1;
                ++aliveCount;
            }
        divers.clear();
        fx = FX_START;
        fy = FY_START;
        marchDir = 1;
        marchTimer = cfg.march;
        diveTimer = cfg.diveInterval;
        animFrame = 0;
        playerX = static_cast<float>(GRID_W / 2 - PLAYER_W / 2);
        bullets.clear();
        respawn.reset();
        setMessage("WAVE " + std::to_string(wave) + " - clear the swarm!");
    }

    float marchInterval() const {
        const float ratio = static_cast<float>(aliveCount) /
                            static_cast<float>(waveCols * waveRows);
        return std::max(MARCH_MIN, currentWave().march * ratio);
    }

    float diveInterval() const {
        const float ratio = static_cast<float>(aliveCount) /
                            static_cast<float>(waveCols * waveRows);
        return std::max(DIVE_INTERVAL_MIN, currentWave().diveInterval * ratio);
    }

    float diveSpeed() const { return currentWave().diveSpeed; }

    void stepFormation(float dt) {
        if (aliveCount == 0) {
            onWaveCleared();
            return;
        }

        marchTimer -= dt;
        if (marchTimer <= 0.0f) {
            marchTimer += marchInterval();
            fx += static_cast<float>(marchDir);

            // Edge detection uses only live columns, so a thinned formation
            // marches right up to the wall before dropping.
            int minC = waveCols - 1, maxC = 0;
            for (int c = 0; c < waveCols; ++c) {
                for (int r = 0; r < waveRows; ++r) {
                    if (formation[r][c]) {
                        minC = std::min(minC, c);
                        maxC = std::max(maxC, c);
                    }
                }
            }
            const float rightEdge = fx + maxC * COL_STEP + (SPR_W - 1);
            const float leftEdge = fx + minC * COL_STEP;
            if (rightEdge >= GRID_W - 1) {
                fx = static_cast<float>(GRID_W - 1) - (maxC * COL_STEP + (SPR_W - 1));
                marchDir = -1;
                fy += DROP_STEP;
            } else if (leftEdge <= 0.0f) {
                fx = static_cast<float>(-minC * COL_STEP);
                marchDir = 1;
                fy += DROP_STEP;
            }
            animFrame = 1 - animFrame;

            // The swarm reaches the ship's row: game over.
            const float bottom = fy + (waveRows - 1) * ROW_STEP + (SPR_H - 1);
            if (bottom >= PLAYER_Y) {
                bestScore = std::max(bestScore, score);
                sfx.play(uj::Sfx::Lose);
                shake.add(0.5f);
                hitStop.trigger(0.12f);
                setMessage("THE SWARM REACHED YOU - Press R to restart");
                endGame();
                return;
            }
        }

        // Dive scheduling: ramps up as the formation thins.
        diveTimer -= dt;
        if (diveTimer <= 0.0f) {
            diveTimer += diveInterval();
            spawnDive();
        }
    }

    void spawnDive() {
        if (static_cast<int>(divers.size()) >= MAX_DIVERS) return;
        std::vector<std::pair<int, int>> live;
        live.reserve(static_cast<size_t>(waveCols * waveRows));
        for (int r = 0; r < waveRows; ++r)
            for (int c = 0; c < waveCols; ++c)
                if (formation[r][c]) live.push_back({c, r});
        if (live.empty()) return;

        const auto [c, r] = live[lcgNext() % live.size()];
        formation[r][c] = 0;              // leaves the formation
        Diver d;
        d.fromC = c;
        d.fromR = r;
        d.baseX = fx + c * COL_STEP + SPR_W / 2.0f;
        d.x = d.baseX;
        d.y = fy + r * ROW_STEP + SPR_H / 2.0f;
        d.sway = static_cast<float>(lcgNext() % 628) / 100.0f;  // 0..6.28 rad
        d.state = 0;
        divers.push_back(d);
        ++diveCount;
        sfx.play(uj::Sfx::Serve);
        particles.burst(d.x * tileSize, d.y * tileSize, 6, enemyColor(r),
                        5.0f, 0.4f, 4.0f);
    }

    // ---- Divers -----------------------------------------------------------------
    void stepDivers(float dt) {
        if (divers.empty()) return;
        // Iterate a copy: onPlayerHit() clears `divers` mid-loop.
        const std::vector<Diver> snapshot = divers;
        std::vector<Diver> keep;
        keep.reserve(snapshot.size());

        for (Diver d : snapshot) {
            if (d.state == 0) {
                // Dive: ease toward the ship, sway side to side, fall fast.
                d.sway += SWAY_FREQ * dt;
                const float targetX = playerX + PLAYER_W / 2.0f;
                d.baseX += (targetX - d.baseX) * std::min(1.0f, 3.0f * dt);
                d.x = std::max(0.0f, std::min(static_cast<float>(GRID_W - SPR_W),
                               d.baseX + std::sin(d.sway) * SWAY_AMP));
                d.y += diveSpeed() * dt;

                if (respawn.hittable() &&
                    d.x < playerX + PLAYER_W && d.x + SPR_W > playerX &&
                    d.y < PLAYER_Y + PLAYER_H && d.y + SPR_H > PLAYER_Y) {
                    onPlayerHit();  // clears divers and returns
                    return;
                }
                if (d.y > GRID_H) {
                    if (d.fromR >= 0) d.state = 1;  // loop back to the slot
                    else continue;                  // test diver: gone
                }
                keep.push_back(d);
            } else {
                // Return to the formation slot.
                const float tx = fx + d.fromC * COL_STEP + SPR_W / 2.0f;
                const float ty = fy + d.fromR * ROW_STEP + SPR_H / 2.0f;
                const float dx = tx - d.x, dy = ty - d.y;
                const float dist = std::sqrt(dx * dx + dy * dy);
                const float step = RETURN_SPEED * dt;
                if (dist <= step || dist < 0.4f) {
                    if (d.fromR >= 0 && d.fromC >= 0)
                        formation[d.fromR][d.fromC] = 1;
                    continue;  // rejoined
                }
                d.x += dx / dist * step;
                d.y += dy / dist * step;
                keep.push_back(d);
            }
        }
        divers = std::move(keep);
    }

    void onPlayerHit() {
        --lives;
        bullets.clear();
        // Diving bugs fly off; any that had slots rejoin the formation.
        for (const Diver& d : divers) {
            if (d.fromR >= 0 && d.fromC >= 0)
                formation[d.fromR][d.fromC] = 1;
        }
        divers.clear();

        particles.burst((float)(playerX * tileSize + PLAYER_W * tileSize / 2),
                        (float)((PLAYER_Y + 1) * tileSize), 26, {120, 235, 255, 255},
                        10.0f, 0.7f, 6.0f);
        if (lives <= 0) {
            bestScore = std::max(bestScore, score);
            sfx.play(uj::Sfx::Lose);
            shake.add(0.6f);
            hitStop.trigger(0.12f);
            setMessage("GAME OVER - Best " + std::to_string(bestScore) +
                       " - Press R to restart");
            endGame();
            return;
        }
        sfx.play(uj::Sfx::Explode);
        shake.add(0.5f);
        hitStop.trigger(0.1f);
        playerX = static_cast<float>(GRID_W / 2 - PLAYER_W / 2);
        respawn.start();
        setMessage("Ship destroyed - " + std::to_string(lives) + " lives left");
        updateHUD();
    }

    // ---- Bullets / collisions -----------------------------------------------------
    void stepBullets(float dt) {
        if (bullets.empty()) return;
        bullets.update(dt);  // free flight, culls by life

        for (size_t i = 0; i < bullets.size(); ) {
            const auto& p = bullets.all()[i];
            if (p.y < -2.0f || hitEnemy(p.x, p.y)) {
                bullets.kill(i);
            } else {
                ++i;
            }
        }
    }

    // A bullet spans (x..x+1, y..y+1). Returns true when it hits something.
    bool hitEnemy(float x, float y) {
        // Formation bugs.
        for (int r = 0; r < waveRows; ++r) {
            for (int c = 0; c < waveCols; ++c) {
                if (!formation[r][c]) continue;
                const float ix = fx + c * COL_STEP;
                const float iy = fy + r * ROW_STEP;
                if (x <= ix + SPR_W - 1 && x + 1.0f >= ix &&
                    y <= iy + SPR_H - 1 && y + 1.0f >= iy) {
                    formation[r][c] = 0;
                    --aliveCount;
                    score += ROW_POINTS[r];
                    killJuice(ix + SPR_W * 0.5f, iy + SPR_H * 0.5f,
                              enemyColor(r), ROW_POINTS[r]);
                    updateHUD();
                    setMessage("Bug down! +" + std::to_string(ROW_POINTS[r]));
                    return true;
                }
            }
        }
        // Diving bugs: worth double.
        for (size_t i = 0; i < divers.size(); ++i) {
            const Diver& d = divers[i];
            if (d.x <= x + 1.0f && d.x + SPR_W >= x &&
                d.y <= y + 1.0f && d.y + SPR_H >= y) {
                const int pts = DIVE_BONUS * ROW_POINTS[std::max(0, d.fromR)];
                score += pts;
                --aliveCount;
                const SDL_Color col = (d.fromR >= 0)
                    ? enemyColor(d.fromR)
                    : SDL_Color{255, 120, 200, 255};
                killJuice(d.x + SPR_W * 0.5f, d.y + SPR_H * 0.5f, col, pts);
                divers.erase(divers.begin() + static_cast<long>(i));
                updateHUD();
                setMessage("Diver down! +" + std::to_string(pts));
                return true;
            }
        }
        return false;
    }

    void killJuice(float cx, float cy, SDL_Color col, int pts) {
        const int px = static_cast<int>(cx * tileSize);
        const int py = static_cast<int>(cy * tileSize);
        particles.burst(static_cast<float>(px), static_cast<float>(py), 16, col,
                        8.0f, 0.5f, 5.0f);
        shake.add(0.18f);
        hitStop.trigger(0.04f);
        sfx.play(uj::Sfx::Explode);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            px, py - 12, "+" + std::to_string(pts)), px, py - 12);
    }

    void onWaveCleared() {
        bestScore = std::max(bestScore, score);
        if (wave >= WAVES_TO_WIN) {
            score += 500;
            bestScore = std::max(bestScore, score);
            shake.add(0.6f);
            hitStop.trigger(0.12f);
            sfx.play(uj::Sfx::Win);
            for (int i = 0; i < 3; ++i) {
                particles.burst(static_cast<float>((i + 1) * 240), 300.0f, 20,
                    (i == 0) ? SDL_Color{255, 95, 95, 255} :
                    (i == 1) ? SDL_Color{95, 220, 110, 255} :
                               SDL_Color{110, 130, 255, 255},
                    9.0f, 0.8f, 6.0f);
            }
            setMessage("YOU WIN! Best " + std::to_string(bestScore) +
                       " - Press R to play again");
            gameWon = true;
            endGame();
        } else {
            score += 100 * wave;
            sfx.play(uj::Sfx::Clear);
            shake.add(0.4f);
            hitStop.trigger(0.1f);
            setMessage("WAVE " + std::to_string(wave) + " CLEARED! +" +
                       std::to_string(100 * wave));
            startWave(wave + 1);
        }
        updateHUD();
    }

    // ---- Player -------------------------------------------------------------------
    void clampPlayerBy(float delta) {
        playerX += delta;
        playerX = std::max(0.0f, std::min(playerX,
            static_cast<float>(GRID_W - PLAYER_W)));
    }

    ActionResult movePlayer(int dir) {
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
        clampPlayerBy(static_cast<float>(dir) * LLM_PLAYER_STEP);
        result.success = true;
        result.message = "Ship at column " +
                         std::to_string(static_cast<int>(std::lround(playerX)));
        return result;
    }

    ActionResult doFire() {
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
        if (respawn.waiting()) {
            result.message = "Ship is respawning";
            return result;
        }
        if (bullets.size() >= static_cast<size_t>(MAX_BULLETS)) {
            result.message = "Two shots already in flight";
            return result;
        }
        const float bx = playerX + PLAYER_W / 2.0f - 0.5f;
        const float by = PLAYER_Y - 1.0f;
        if (!bullets.fire(bx, by, 0.0f, -BULLET_SPEED, 4.0f, 0)) {
            result.message = "Shot cap reached";
            return result;
        }
        sfx.play(uj::Sfx::Shoot);
        shake.add(0.05f);  // subtle recoil
        setMessage("Fire!");
        result.success = true;
        result.message = "Shot fired";
        return result;
    }

    // ---- Feel / rendering helpers --------------------------------------------------
    static SDL_Color enemyColor(int row) {
        if (row == 0) return {255, 95, 95, 255};    // boss row, red
        if (row == 1) return {255, 170, 60, 255};   // orange
        if (row == 2) return {95, 220, 110, 255};   // green
        if (row == 3) return {110, 130, 255, 255};  // blue
        return {170, 120, 255, 255};                // purple
    }

    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
    }

    void drawBug(SDL_Renderer* sdl, int px, int py, SDL_Color col, int frame) {
        // Classic two-frame bee silhouette (3x3); the wings shuffle between
        // frames to sell the march.
        const char* rowsA[SPR_H] = {"X.X", "XXX", ".X."};
        const char* rowsB[SPR_H] = {".X.", "XXX", "X.X"};
        const char* const* rows = (frame == 0) ? rowsA : rowsB;
        SDL_SetRenderDrawColor(sdl, col.r, col.g, col.b, col.a);
        for (int r = 0; r < SPR_H; ++r) {
            for (int c = 0; c < SPR_W; ++c) {
                if (rows[r][c] != 'X') continue;
                SDL_Rect cell = {px + c * tileSize, py + r * tileSize,
                                 tileSize, tileSize};
                SDL_RenderFillRect(sdl, &cell);
            }
        }
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Score " + std::to_string(score) + "    Wave " +
                     std::to_string(wave) + "    Lives " +
                     std::to_string(lives) + "    Best " +
                     std::to_string(std::max(bestScore, score)));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the Galaga class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static Galaga game;
#else
    Galaga game;
#endif
    game.run();
    return 0;
}
#endif
