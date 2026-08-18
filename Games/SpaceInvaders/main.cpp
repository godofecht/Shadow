// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Space Invaders - the classic marching-invaders shooter, game #8 of the
// 100-game program.
//
// An 11x5 formation of invaders marches side to side, dropping a row each
// time it hits an edge and speeding up as it thins out. The player cannon
// sweeps the bottom, fires single shots straight up, and hides behind three
// eroding shields (bullets and bombs both chip shield cells). A bonus saucer
// periodically flies across the top for extra points. The invasion reaches
// your row - or you run out of lives - and it's game over; clear the sky and
// you win.
//
// It ships to the AAA-feel bar (see GAMES.md) via Engine/Core/GameJuice.h:
// invader kills and saucer payoffs burst into particles, kills shake the
// screen and hit-stop, score floats up in text, and every shot/explosion is
// a procedural sound effect synthesized in memory.
//
// One code path serves human input and the LLM: the keyboard (A/D, arrows,
// mouse) polls the same clampPlayer() as the "move_left"/"move_right"
// actions, and SPACE / "fire" share doFire(). An LLM can play the exact game
// a human plays.
//
// Controls: A/D or arrows = move cannon (mouse also works), SPACE = fire,
//           P = pause, R = restart. Three lives, clear all 55 invaders = win.
//
// All "randomness" (bomb targets, saucer points) flows through a fixed-seed
// LCG so a given playthrough is fully deterministic and unit-testable.

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

class SpaceInvaders : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 80;          // 80x50 cells @ 12px = 960x600
    static constexpr int GRID_H = 50;

    // Player cannon.
    static constexpr int PLAYER_W = 3;
    static constexpr int PLAYER_H = 2;
    static constexpr int PLAYER_Y = GRID_H - 6;   // top cell of the cannon
    static constexpr float PLAYER_SPEED = 40.0f;  // cells/second
    static constexpr float LLM_PLAYER_STEP = 5.0f;
    static constexpr int MAX_LIVES = 3;
    static constexpr float RESPAWN_TIME = 1.5f;   // invulnerable after a hit
    static constexpr float BULLET_SPEED = 22.0f;  // cells/second upward
    static constexpr float BOMB_SPEED = 6.0f;     // cells/second downward

    // Invader formation: 11 columns x 5 rows of 3x4-cell sprites.
    static constexpr int INVADER_COLS = 11;
    static constexpr int INVADER_ROWS = 5;
    static constexpr int SPR_W = 3;   // invader sprite footprint
    static constexpr int SPR_H = 4;   // (SPRITE_W/H collide with an engine guard)
    static constexpr int COL_STEP = 4;            // horizontal spacing
    static constexpr int ROW_STEP = 5;            // vertical spacing
    static constexpr float FX_START = 18.0f;      // leftmost live cell ~= col 0
    static constexpr float FY_START = 5.0f;
    static constexpr int DROP_STEP = 2;           // cells dropped per edge hit
    static constexpr float MARCH_START = 0.8f;    // seconds per step, fresh
    static constexpr float MARCH_MIN = 0.15f;     // fastest march
    static constexpr float FIRE_START = 1.6f;     // seconds between bombs, fresh
    static constexpr float FIRE_MIN = 0.45f;      // fastest enemy fire
    static constexpr int ROW_POINTS[INVADER_ROWS] = {30, 20, 20, 10, 10};

    // Shields: three 13x5 cell blocks that erode under fire.
    static constexpr int SHIELD_COUNT = 3;
    static constexpr int SHIELD_W = 13;
    static constexpr int SHIELD_H = 5;
    static constexpr int SHIELD_Y = GRID_H - 11;  // 39..43, cannon at 44..45
    static constexpr int SHIELD_X[SHIELD_COUNT] = {10, 34, 58};

    // Bonus saucer.
    static constexpr int SAUCER_Y = 2;
    static constexpr int SAUCER_W = 5;
    static constexpr float SAUCER_SPEED = 9.0f;
    static constexpr float SAUCER_INTERVAL = 22.0f;

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
    int invaders[INVADER_ROWS][INVADER_COLS]; // 1 = alive, 0 = dead
    int shieldCells[SHIELD_COUNT][SHIELD_H][SHIELD_W]; // 1 = intact, 0 = gone
    int alive = INVADER_ROWS * INVADER_COLS;
    float fx = FX_START, fy = FY_START;       // formation origin (cells)
    int marchDir = 1;
    float marchTimer = MARCH_START;
    float fireTimer = FIRE_START;
    int animFrame = 0;                        // two-frame march animation

    float playerX = 0.0f;
    int lives = MAX_LIVES;
    int score = 0;
    int bestScore = 0;                         // session best; survives restarts
    float respawnTimer = 0.0f;
    bool bulletActive = false;
    float bx = 0.0f, by = 0.0f;               // the player's single bullet
    struct Bomb { float x, y, vx; };
    std::vector<Bomb> bombs;

    // Bonus saucer.
    bool saucerActive = false;
    float saucerX = 0.0f;
    int saucerDir = 1;
    int saucerPoints = 100;
    float saucerTimer = SAUCER_INTERVAL * 0.5f;

    // Fixed-seed LCG: every playthrough (and test) is deterministic.
    uint32_t lcgState = 0xC0FFEEu;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;
    float smokeSweep = 0.0f;

public:
    SpaceInvaders() : Game2D("Space Invaders", 960, 600, 12) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hook: drop a bomb dead-center on the cannon so the very next
    // stepBombs call deterministically hits it, exercising onPlayerHit() and
    // the post-loop bombs.clear() (which must NOT run mid-iteration, or it
    // invalidates stepBombs' range-for - the container-overflow the sanitizer
    // found). Placed at the cannon's row so it is below every shield and
    // cannot erode a cell on the way in.
    void dropBombOnPlayerForTest() {
        Bomb bmb;
        bmb.x = playerX + PLAYER_W / 2.0f - 0.5f;
        bmb.y = static_cast<float>(PLAYER_Y);
        bmb.vx = 0.0f;
        bombs.push_back(bmb);
    }

    void initGame() override {
        score = 0;
        lives = MAX_LIVES;
        playerX = static_cast<float>(GRID_W / 2 - PLAYER_W / 2);
        fx = FX_START;
        fy = FY_START;
        marchDir = 1;
        marchTimer = MARCH_START;
        fireTimer = FIRE_START;
        animFrame = 0;
        respawnTimer = 0.0f;
        bulletActive = false;
        bombs.clear();
        saucerActive = false;
        saucerTimer = SAUCER_INTERVAL * 0.5f;
        smokeSweep = 0.0f;
        paused = false;
        particles.clear();
        floatTexts = uj::FloatingText{};

        alive = 0;
        for (int r = 0; r < INVADER_ROWS; ++r) {
            for (int c = 0; c < INVADER_COLS; ++c) {
                invaders[r][c] = 1;
                ++alive;
            }
        }
        for (int s = 0; s < SHIELD_COUNT; ++s) {
            for (int r = 0; r < SHIELD_H; ++r) {
                for (int c = 0; c < SHIELD_W; ++c) {
                    shieldCells[s][r][c] = 1;
                }
            }
        }

        // Court: dark background painted once through the grid.
        createGrid(GRID_W, GRID_H, tileSize);
        grid->fill({6, 7, 12, 255});
        grid->setBorderColor({12, 14, 22, 255});

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

        // Pause freezes everything (world + fx) but keeps rendering.
        if (paused) {
            if (message) message->setText("PAUSED - press P to resume");
            return;
        }

        // Hit-stop: the world stands still for a beat on a kill.
        if (hitStop.frozen()) {
            hitStop.update(dt);
            return;
        }

        // Keyboard: continuous, dt-scaled movement.
        if (input.isKeyHeld(KEY_A) || input.isKeyHeld(KEY_LEFT)) clampPlayerBy(-PLAYER_SPEED * dt);
        if (input.isKeyHeld(KEY_D) || input.isKeyHeld(KEY_RIGHT)) clampPlayerBy(PLAYER_SPEED * dt);

        // Headless smoke mode: sweep the cannon and fire whenever a shot is
        // available so a dummy-driver run exercises march -> bombs -> shields
        // -> collisions -> score. (The engine auto-restarts on game over.)
        if (smokeMode) {
            smokeSweep += dt;
            const float period = 3.0f;
            const int dir = (static_cast<int>(smokeSweep / period) % 2 == 0) ? 1 : -1;
            clampPlayerBy(static_cast<float>(dir) * 36.0f * dt);
            if (respawnTimer <= 0.0f) (void)doFire();
        }

        if (respawnTimer > 0.0f) {
            respawnTimer -= dt;
            if (respawnTimer <= 0.0f) setMessage("Respawned");
        }

        stepFormation(dt);
        stepBombs(dt);
        stepSaucer(dt);

        // The player's single shot.
        if (bulletActive) {
            by -= BULLET_SPEED * dt;
            if (by < -2.0f) {
                bulletActive = false;
            } else if (hitInvader(bx, by)) {
                bulletActive = false;
            } else if (erodeShield(bx, by, 1.0f, 2.0f)) {
                bulletActive = false;
            } else if (hitSaucer(bx, by)) {
                bulletActive = false;
            }
        }

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        // World space shakes; the HUD and floating text do not.
        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // The marching formation: 3x4-cell pixel-art sprites, two-frame
        // animation, one color per row (top = 30 pts, middle = 20, bottom = 10).
        for (int r = 0; r < INVADER_ROWS; ++r) {
            const SDL_Color col = invaderColor(r);
            for (int c = 0; c < INVADER_COLS; ++c) {
                if (!invaders[r][c]) continue;
                const int ox = static_cast<int>(std::lround(fx)) + c * COL_STEP;
                const int oy = static_cast<int>(std::lround(fy)) + r * ROW_STEP;
                drawInvader(sdl, ox * tileSize + sx, oy * tileSize + sy, col, animFrame);
            }
        }

        // Shields: intact cells only.
        for (int s = 0; s < SHIELD_COUNT; ++s) {
            for (int r = 0; r < SHIELD_H; ++r) {
                for (int c = 0; c < SHIELD_W; ++c) {
                    if (!shieldCells[s][r][c]) continue;
                    SDL_Rect cell = {
                        (SHIELD_X[s] + c) * tileSize + sx,
                        (SHIELD_Y + r) * tileSize + sy,
                        tileSize, tileSize
                    };
                    SDL_SetRenderDrawColor(sdl, 40, 200, 130, 255);
                    SDL_RenderFillRect(sdl, &cell);
                }
            }
        }

        // Bonus saucer.
        if (saucerActive) {
            SDL_Rect saucer = {
                static_cast<int>(std::lround(saucerX)) * tileSize + sx,
                SAUCER_Y * tileSize + sy,
                SAUCER_W * tileSize, tileSize
            };
            SDL_SetRenderDrawColor(sdl, 230, 120, 255, 255);
            SDL_RenderFillRect(sdl, &saucer);
        }

        // Player bullets and enemy bombs.
        if (bulletActive) {
            SDL_Rect b = {
                static_cast<int>(std::lround(bx)) * tileSize + sx,
                static_cast<int>(std::lround(by)) * tileSize + sy,
                tileSize, tileSize * 2
            };
            SDL_SetRenderDrawColor(sdl, 240, 240, 240, 255);
            SDL_RenderFillRect(sdl, &b);
        }
        for (const Bomb& bmb : bombs) {
            SDL_Rect b = {
                static_cast<int>(std::lround(bmb.x)) * tileSize + sx,
                static_cast<int>(std::lround(bmb.y)) * tileSize + sy,
                tileSize, tileSize * 2
            };
            SDL_SetRenderDrawColor(sdl, 255, 160, 60, 255);
            SDL_RenderFillRect(sdl, &b);
        }

        // Player cannon: barrel + body.
        const int px = static_cast<int>(std::lround(playerX));
        SDL_Rect body = {px * tileSize + sx, (PLAYER_Y + 1) * tileSize + sy,
                         PLAYER_W * tileSize, tileSize};
        SDL_Rect barrel = {(px + 1) * tileSize + sx, PLAYER_Y * tileSize + sy,
                           tileSize, tileSize};
        SDL_SetRenderDrawColor(sdl, 80, 220, 255, 255);
        SDL_RenderFillRect(sdl, &barrel);
        SDL_RenderFillRect(sdl, &body);

        // Particles live in world space, so they shake with it.
        particles.render(sdl, sx, sy);

        // Floating score labels (screen space).
        floatTexts.render(getRenderer());

        // Pause veil.
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
        state.message = statusText;
        state.stats["score"] = score;
        state.stats["best"] = std::max(bestScore, score);
        state.stats["lives"] = lives;
        state.stats["invaders_left"] = alive;
        state.stats["player_x"] = static_cast<int>(std::lround(playerX));
        state.stats["player_y"] = PLAYER_Y;
        state.stats["formation_x"] = static_cast<int>(std::lround(fx));
        state.stats["formation_y"] = static_cast<int>(std::lround(fy));
        state.stats["shields_left"] = countShieldCells();
        state.stats["bullets"] = bulletActive ? 1 : 0;
        state.stats["bombs"] = static_cast<int>(bombs.size());
        state.stats["saucer_active"] = saucerActive ? 1 : 0;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.entities["player"] = {
            static_cast<int>(std::lround(playerX)), PLAYER_Y
        };
        return state;
    }

private:
    // ---- Formation ----------------------------------------------------------
    float marchInterval() const {
        const float ratio = static_cast<float>(alive) /
                            static_cast<float>(INVADER_ROWS * INVADER_COLS);
        return std::max(MARCH_MIN, MARCH_START * ratio);
    }

    float fireInterval() const {
        const float ratio = static_cast<float>(alive) /
                            static_cast<float>(INVADER_ROWS * INVADER_COLS);
        return std::max(FIRE_MIN, FIRE_START * ratio);
    }

    void stepFormation(float dt) {
        if (alive == 0) return;

        marchTimer -= dt;
        if (marchTimer <= 0.0f) {
            marchTimer += marchInterval();
            fx += static_cast<float>(marchDir);

            // Edge detection uses only live columns, so a thinned formation
            // marches right up to the wall before dropping.
            int minC = INVADER_COLS - 1, maxC = 0;
            for (int c = 0; c < INVADER_COLS; ++c) {
                for (int r = 0; r < INVADER_ROWS; ++r) {
                    if (invaders[r][c]) {
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

            // The invasion reaches the cannon row: game over.
            const float bottom = fy + (INVADER_ROWS - 1) * ROW_STEP + (SPR_H - 1);
            if (bottom >= PLAYER_Y) {
                bestScore = std::max(bestScore, score);
                sfx.play(uj::Sfx::Lose);
                shake.add(0.5f);
                hitStop.trigger(0.12f);
                setMessage("INVADERS REACHED EARTH - Press R to restart");
                endGame();
                return;
            }
        }

        // Enemy fire: a random live invader drops a bomb; fire rate ramps up
        // as the formation thins.
        fireTimer -= dt;
        if (fireTimer <= 0.0f) {
            fireTimer += fireInterval();
            fireFromFormation();
        }
    }

    void fireFromFormation() {
        std::vector<std::pair<int, int>> live;
        live.reserve(static_cast<size_t>(INVADER_ROWS * INVADER_COLS));
        for (int r = 0; r < INVADER_ROWS; ++r) {
            for (int c = 0; c < INVADER_COLS; ++c) {
                if (invaders[r][c]) live.push_back({c, r});
            }
        }
        if (live.empty()) return;
        const auto [c, r] = live[lcgNext() % live.size()];
        // Bomb drops from the invader's bottom edge, center of its 3 cells.
        Bomb bmb;
        bmb.x = fx + c * COL_STEP + SPR_W / 2.0f;
        bmb.y = fy + r * ROW_STEP + SPR_H;
        bmb.vx = (static_cast<float>(lcgNext() % 100) / 100.0f) * 3.0f - 1.5f;
        bombs.push_back(bmb);
    }

    // ---- Bombs ---------------------------------------------------------------
    void stepBombs(float dt) {
        if (bombs.empty()) return;
        std::vector<Bomb> keep;
        keep.reserve(bombs.size());
        bool playerHit = false;
        for (Bomb bmb : bombs) {
            bmb.x += bmb.vx * dt;
            bmb.y += BOMB_SPEED * dt;
            bool dead = false;
            if (bmb.y > GRID_H) {
                dead = true;                     // fell off the bottom
            } else if (erodeShield(bmb.x, bmb.y, 1.0f, 2.0f)) {
                dead = true;                     // ate a shield cell
            } else if (respawnTimer <= 0.0f &&
                       bmb.y + 2.0f >= PLAYER_Y &&
                       bmb.y <= PLAYER_Y + PLAYER_H &&
                       bmb.x + 1.0f >= playerX &&
                       bmb.x <= playerX + PLAYER_W) {
                onPlayerHit();                   // hit the cannon
                dead = true;
                playerHit = true;
            }
            if (!dead) keep.push_back(bmb);
        }
        // A hit clears ALL enemy bombs (the respawn beat resets the field).
        // Clear here rather than inside onPlayerHit(), which runs mid-loop and
        // would invalidate this range-for's iterators (container-overflow).
        if (playerHit) bombs.clear();
        else bombs = std::move(keep);
    }

    void onPlayerHit() {
        --lives;
        bulletActive = false;
        // ---- Juice: the cannon bursts --------------------------------------
        particles.burst(playerX * tileSize + PLAYER_W * tileSize / 2,
                        (PLAYER_Y + 1) * tileSize, 22, {80, 220, 255, 255},
                        10.0f, 0.7f, 6.0f);
        if (lives <= 0) {
            bestScore = std::max(bestScore, score);
            sfx.play(uj::Sfx::Lose);
            shake.add(0.55f);
            hitStop.trigger(0.12f);
            setMessage("GAME OVER - Best " + std::to_string(bestScore) +
                       " - Press R to restart");
            endGame();
            return;
        }
        sfx.play(uj::Sfx::Explode);
        shake.add(0.45f);
        hitStop.trigger(0.10f);
        playerX = static_cast<float>(GRID_W / 2 - PLAYER_W / 2);
        respawnTimer = RESPAWN_TIME;
        setMessage("Cannon hit - " + std::to_string(lives) + " lives left");
        updateHUD();
    }

    // ---- Bullets / collisions --------------------------------------------------
    bool hitInvader(float x, float y) {
        // Bullet spans (x..x+1, y..y+2); an invader spans its sprite cells.
        for (int r = 0; r < INVADER_ROWS; ++r) {
            for (int c = 0; c < INVADER_COLS; ++c) {
                if (!invaders[r][c]) continue;
                const float ix = fx + c * COL_STEP;
                const float iy = fy + r * ROW_STEP;
                if (x <= ix + SPR_W - 1 && x + 1.0f >= ix &&
                    y <= iy + SPR_H - 1 && y + 2.0f >= iy) {
                    invaders[r][c] = 0;
                    --alive;
                    score += ROW_POINTS[r];
                    // ---- Juice: shatter, shake, hit-stop, score pop --------
                    const int cx = static_cast<int>((ix + SPR_W * 0.5f) * tileSize);
                    const int cy = static_cast<int>((iy + SPR_H * 0.5f) * tileSize);
                    particles.burst((float)cx, (float)cy, 16, invaderColor(r),
                                    8.0f, 0.5f, 5.0f);
                    shake.add(0.18f);
                    hitStop.trigger(0.04f);
                    sfx.play(uj::Sfx::Explode);
                    floatTexts.spawn(std::make_shared<TextDisplay>(
                        cx, cy - 12, "+" + std::to_string(ROW_POINTS[r])),
                        cx, cy - 12);
                    updateHUD();
                    if (alive == 0) {
                        // ---- Win: celebration ---------------------------------
                        shake.add(0.6f);
                        hitStop.trigger(0.12f);
                        sfx.play(uj::Sfx::Win);
                        for (int i = 0; i < 3; ++i) {
                            particles.burst((float)((i + 1) * 240), 300.0f, 20,
                                (i == 0) ? SDL_Color{255, 95, 95, 255} :
                                (i == 1) ? SDL_Color{95, 220, 110, 255} :
                                           SDL_Color{110, 130, 255, 255},
                                9.0f, 0.8f, 6.0f);
                        }
                        bestScore = std::max(bestScore, score);
                        setMessage("SKY CLEARED! Best " + std::to_string(bestScore) +
                                   " - Press R to play again");
                        gameWon = true;
                        endGame();
                    } else {
                        setMessage("Invader down! (" + std::to_string(alive) +
                                   " left, +" + std::to_string(ROW_POINTS[r]) + ")");
                    }
                    return true;
                }
            }
        }
        return false;
    }

    bool hitSaucer(float x, float y) {
        if (!saucerActive) return false;
        const float sx = saucerX;
        if (x <= sx + SAUCER_W - 1 && x + 1.0f >= sx && y <= SAUCER_Y + 1) {
            saucerActive = false;
            score += saucerPoints;
            // ---- Juice: the saucer pays out --------------------------------
            const int cx = static_cast<int>((sx + SAUCER_W * 0.5f) * tileSize);
            const int cy = static_cast<int>((SAUCER_Y + 0.5f) * tileSize);
            particles.burst((float)cx, (float)cy, 18, {230, 120, 255, 255},
                            9.0f, 0.6f, 6.0f);
            shake.add(0.25f);
            hitStop.trigger(0.06f);
            sfx.play(uj::Sfx::Coin);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                cx, cy - 12, "+" + std::to_string(saucerPoints)),
                cx, cy - 12);
            setMessage("Saucer! +" + std::to_string(saucerPoints));
            updateHUD();
            return true;
        }
        return false;
    }

    // Erode any shield cells overlapped by the rect (x,y,w,h); returns true
    // if a cell was destroyed (which also consumes the bullet/bomb).
    bool erodeShield(float x, float y, float w, float h) {
        for (int s = 0; s < SHIELD_COUNT; ++s) {
            const int sx = SHIELD_X[s];
            if (x + w <= sx || x >= sx + SHIELD_W) continue;
            if (y + h <= SHIELD_Y || y >= SHIELD_Y + SHIELD_H) continue;
            const int c0 = std::max(0, static_cast<int>(x) - sx);
            const int c1 = std::min(SHIELD_W - 1, static_cast<int>(x + w - 1) - sx);
            const int r0 = std::max(0, static_cast<int>(y) - SHIELD_Y);
            const int r1 = std::min(SHIELD_H - 1, static_cast<int>(y + h - 1) - SHIELD_Y);
            bool hit = false;
            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    if (shieldCells[s][r][c]) {
                        shieldCells[s][r][c] = 0;
                        hit = true;
                    }
                }
            }
            if (hit) {
                // ---- Juice: shield chips spark green ------------------------
                particles.burst((x + w * 0.5f) * tileSize,
                                (y + h * 0.5f) * tileSize, 6,
                                {40, 200, 130, 255}, 5.0f, 0.35f, 4.0f);
                return true;
            }
        }
        return false;
    }

    // ---- Saucer ----------------------------------------------------------------
    void stepSaucer(float dt) {
        if (!saucerActive) {
            saucerTimer -= dt;
            if (saucerTimer <= 0.0f) {
                saucerActive = true;
                const bool fromLeft = (lcgNext() % 2 == 0);
                saucerX = fromLeft ? -SAUCER_W : static_cast<float>(GRID_W);
                saucerDir = fromLeft ? 1 : -1;
                const int pts[4] = {50, 100, 150, 300};
                saucerPoints = pts[lcgNext() % 4];
            }
            return;
        }
        saucerX += saucerDir * SAUCER_SPEED * dt;
        if (saucerX < -SAUCER_W - 1 || saucerX > GRID_W + 1) {
            saucerActive = false;
            saucerTimer = SAUCER_INTERVAL;
        }
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
        result.message = "Cannon at column " +
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
        if (bulletActive) {
            result.message = "Shot already in flight";
            return result;
        }
        if (respawnTimer > 0.0f) {
            result.message = "Cannon is respawning";
            return result;
        }
        bx = playerX + 1.0f;          // barrel center
        by = PLAYER_Y - 1.0f;
        bulletActive = true;
        sfx.play(uj::Sfx::Shoot);
        shake.add(0.05f);             // subtle recoil
        setMessage("Fire!");
        result.success = true;
        result.message = "Shot fired";
        return result;
    }

    // ---- Feel / rendering helpers -------------------------------------------------
    static SDL_Color invaderColor(int row) {
        if (row == 0) return {255, 95, 95, 255};    // crab, red
        if (row < 3)  return {95, 220, 110, 255};   // squid, green
        return {110, 130, 255, 255};                // jelly, blue
    }

    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
    }

    void drawInvader(SDL_Renderer* sdl, int px, int py, SDL_Color col, int frame) {
        // Classic two-frame sprite (3 wide x 4 tall); the bottom row of
        // "legs" shuffles between frames to sell the march.
        const char* rowsA[SPR_H] = {".X.", "XXX", "XXX", "X.X"};
        const char* rowsB[SPR_H] = {".X.", "XXX", "XXX", "XX."};
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

    int countShieldCells() const {
        int n = 0;
        for (int s = 0; s < SHIELD_COUNT; ++s) {
            for (int r = 0; r < SHIELD_H; ++r) {
                for (int c = 0; c < SHIELD_W; ++c) {
                    n += shieldCells[s][r][c];
                }
            }
        }
        return n;
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Score " + std::to_string(score) + "    Lives " +
                     std::to_string(lives) + "    Invaders " +
                     std::to_string(alive) + "    Best " +
                     std::to_string(std::max(bestScore, score)));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the SpaceInvaders class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static SpaceInvaders game;
#else
    SpaceInvaders game;
#endif
    game.run();
    return 0;
}
#endif
