// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Brick Breaker+ - Breakout with a power-up system (catalog #19).
//
// The classic brick-breaker loop from Games/Breakout, plus a drop/catch
// power-up economy: some bricks shed a capsule as they shatter, and catching
// it on the paddle activates one of six effects - widen the paddle, split
// the ball(s) into a multi-ball barrage, slow the ball(s) down, gain an
// extra life, make the paddle sticky (catch + re-aim the ball), or fire
// laser bolts that burn whole brick columns. Every effect has a distinct
// color and a floating label, so the board reads at a glance even during a
// fast rally.
//
// One code path serves human input and the LLM: A/D/arrows poll the same
// clampPaddle() as the "move_paddle_left/right" actions, and SPACE/"serve"
// share doServe(). Catch is automatic - any capsule that lands on the paddle
// activates. An LLM can play the exact game a human plays.
//
// Controls: A/D or arrows = move paddle (mouse also works), SPACE = serve,
//           P = pause, R = restart. Three lives, wall cleared = win.

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

class BrickBreakerPlus : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 80;          // 80x50 cells @ 12px = 960x600
    static constexpr int GRID_H = 50;
    static constexpr int PADDLE_H = 2;
    static constexpr int PADDLE_W = 10;        // base width
    static constexpr int PADDLE_W_WIDE = 16;   // EXPAND power-up width
    static constexpr int PADDLE_Y = GRID_H - 4;
    static constexpr float PADDLE_SPEED = 48.0f;    // cells/second
    static constexpr float LLM_PADDLE_STEP = 6.0f;  // cells per LLM action
    static constexpr float BALL_SPEED = 22.0f;      // cells/second on serve
    static constexpr float SPEEDUP = 1.03f;         // per paddle hit
    static constexpr float MAX_SPEED = 40.0f;
    static constexpr float SLOW_FACTOR = 0.65f;     // SLOW power-up multiplier
    static constexpr float MAX_ANGLE_DEG = 55.0f;
    static constexpr float DEG_TO_RAD = 0.0174532925f;
    static constexpr int MAX_LIVES = 3;
    static constexpr int MAX_LIVES_CAP = 6;
    static constexpr int BRICK_ROWS = 5;
    static constexpr int BRICK_COLS = 12;
    static constexpr int BRICK_W = (GRID_W - 4) / BRICK_COLS;   // 6 cells
    static constexpr int BRICK_TOP = 5;
    static constexpr float PUP_SPEED = 9.0f;         // capsule fall speed
    static constexpr int DROP_CHANCE_PCT = 28;       // brick -> capsule odds
    static constexpr float EXPAND_DURATION = 12.0f;
    static constexpr float SLOW_DURATION = 10.0f;
    static constexpr float STICKY_DURATION = 12.0f;
    static constexpr float LASER_SPEED = 60.0f;   // cells/second (upward)
    static constexpr int MAX_BALLS = 8;

    // Power-up kinds (also the capsule color index).
    enum Pup { EXPAND = 0, MULTI = 1, SLOW = 2, LIFE = 3, STICKY = 4, LASER = 5, NUM_PUPS = 6 };

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
    struct Ball { float x, y, vx, vy; };
    struct PowerUp { int type; float x, y; };
    struct LaserBolt { float x, y; bool spent = false; };

    std::vector<int> bricks;       // 1 = intact, 0 = broken (row-major)
    std::vector<Ball> balls;       // >= 1; the resting ball is balls[0]
    std::vector<PowerUp> powerups; // falling capsules
    std::vector<LaserBolt> lasers; // LASER bolts flying up their columns
    float paddleX = 0.0f;          // left cell of the paddle
    float baseSpeed = BALL_SPEED;  // difficulty ramp (grows on paddle hits)
    float expandTimer = 0.0f;      // EXPAND remaining
    float slowTimer = 0.0f;        // SLOW remaining
    float stickyTimer = 0.0f;      // STICKY remaining
    int lives = MAX_LIVES;
    int score = 0;
    int bestScore = 0;             // session best; survives restarts
    bool serve = true;
    std::string statusText;
    float smokeTimer = 0.0f;
    uint32_t rng = 0x1234ABCDu;    // deterministic drop LCG

public:
    BrickBreakerPlus() : Game2D("Brick Breaker+", 960, 600, 12) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hooks for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Place a capsule directly (test hook): type is a Pup value.
    void dropPowerupForTest(int type, float x, float y) {
        PowerUp p;
        p.type = type;
        p.x = x;
        p.y = y;
        powerups.push_back(p);
    }

    void setPaddleXForTest(float x) {
        paddleX = std::max(0.0f, std::min(x, static_cast<float>(GRID_W - paddleW())));
    }

    // Launch the (single) ball directly (test hook): clears the serve state
    // and puts one ball in flight at (x, y) with velocity (vx, vy), so tests
    // can drive a deterministic paddle hit / sticky catch.
    void setBallForTest(float x, float y, float vx, float vy) {
        balls.clear();
        Ball b;
        b.x = x;
        b.y = y;
        b.vx = vx;
        b.vy = vy;
        balls.push_back(b);
        serve = false;
    }

    void forceWinForTest() {
        bricks.assign(bricks.size(), 0);
        bestScore = std::max(bestScore, score);
        gameWon = true;
        endGame();
    }

    void initGame() override {
        score = 0;
        lives = MAX_LIVES;
        baseSpeed = BALL_SPEED;
        expandTimer = 0.0f;
        slowTimer = 0.0f;
        stickyTimer = 0.0f;
        paddleX = static_cast<float>(GRID_W / 2 - PADDLE_W / 2);
        paused = false;
        balls.clear();
        powerups.clear();
        lasers.clear();
        particles.clear();
        floatTexts = uj::FloatingText{};

        // Court: dark background painted once through the grid.
        createGrid(GRID_W, GRID_H, tileSize);
        grid->fill({10, 12, 18, 255});
        grid->setBorderColor({16, 18, 26, 255});

        hud = createText(10, 6, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, GRID_H * tileSize - 26, "");
        message->setColor({255, 220, 120, 255});

        // Brick wall: 5 rows of 12, strongest row on top.
        bricks.assign(static_cast<size_t>(BRICK_ROWS) * BRICK_COLS, 1);

        resetServe();
        updateHUD();

        registerAction("move_paddle_left", [this]() { return movePaddle(-1); });
        registerAction("move_paddle_right", [this]() { return movePaddle(1); });
        registerAction("serve", [this]() { return doServe(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_SPACE).onPress([this]() { doServe(); });
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

        // Hit-stop: the world stands still for a beat on impact.
        if (hitStop.frozen()) {
            hitStop.update(dt);
            return;
        }

        // Keyboard: continuous, dt-scaled paddle movement.
        if (input.isKeyHeld(KEY_A) || input.isKeyHeld(KEY_LEFT)) clampPaddleBy(-PADDLE_SPEED * dt);
        if (input.isKeyHeld(KEY_D) || input.isKeyHeld(KEY_RIGHT)) clampPaddleBy(PADDLE_SPEED * dt);

        // Effect timers. When SLOW expires, restore the balls to base speed.
        if (expandTimer > 0.0f) expandTimer -= dt;
        const bool wasSlow = slowTimer > 0.0f;
        if (slowTimer > 0.0f) slowTimer -= dt;
        if (wasSlow && slowTimer <= 0.0f) rescaleBalls(baseSpeed);
        if (stickyTimer > 0.0f) stickyTimer -= dt;

        // A shrinking paddle (EXPAND expiry) must re-clamp into the court.
        paddleX = std::min(paddleX, static_cast<float>(GRID_W - paddleW()));

        // Headless smoke mode: chase the ball (or a falling capsule) and
        // auto-serve, so a dummy-driver run exercises serve -> rally ->
        // brick -> drop -> catch -> effect -> win/lose.
        if (smokeMode) autopilot(dt);

        // Capsules fall; catching is automatic on the paddle.
        updatePowerups(dt);
        updateLasers(dt);

        if (serve) {
            // The resting ball rides the paddle until served.
            balls[0].x = paddleX + static_cast<float>(paddleW()) / 2.0f;
            balls[0].y = static_cast<float>(PADDLE_Y) - 1.0f;
            updateFx(dt);
            updateHUD();
            return;
        }

        // Integrate every ball.
        bool caughtSticky = false;
        for (Ball& b : balls) {
            b.x += b.vx * dt;
            b.y += b.vy * dt;

            // Ceiling: exact mirror reflection.
            if (b.y < 0.0f) {
                b.y = -b.y;
                b.vy = -b.vy;
            }
            // Side walls.
            if (b.x < 0.0f) {
                b.x = -b.x;
                b.vx = -b.vx;
            } else if (b.x > static_cast<float>(GRID_W - 1)) {
                b.x = 2.0f * static_cast<float>(GRID_W - 1) - b.x;
                b.vx = -b.vx;
            }

            // Paddle collision - only while descending and overlapping.
            if (b.vy > 0.0f &&
                b.y >= static_cast<float>(PADDLE_Y) - 0.5f &&
                b.y <= static_cast<float>(PADDLE_Y) + 1.0f &&
                b.x >= paddleX - 0.5f &&
                b.x <= paddleX + static_cast<float>(paddleW()) + 0.5f) {
                // STICKY catches the ball instead of deflecting it; the catch
                // mutates `balls`, so it runs after this loop.
                if (stickyTimer > 0.0f) {
                    caughtSticky = true;
                    break;
                }
                deflect(b);
            }

            resolveBricks(b);
            if (!gameRunning) break;  // win can end the game mid-frame
        }
        if (caughtSticky) stickyCatch();

        // Balls that fell off the bottom are lost.
        balls.erase(std::remove_if(balls.begin(), balls.end(),
            [](const Ball& b) { return b.y > static_cast<float>(GRID_H - 1); }),
            balls.end());
        if (balls.empty()) onLoseLife();

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        // World space shakes; the HUD and floating text do not.
        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // Bricks: intact cells only, one color per row.
        for (int r = 0; r < BRICK_ROWS; ++r) {
            for (int c = 0; c < BRICK_COLS; ++c) {
                if (bricks[(size_t)r * BRICK_COLS + c] == 0) continue;
                const int x = (2 + c * BRICK_W) * tileSize;
                const int y = (BRICK_TOP + r * 2) * tileSize;
                SDL_Rect rect = {x + sx + 1, y + sy + 1,
                                 tileSize * BRICK_W - 2, tileSize - 2};
                SDL_SetRenderDrawColor(sdl, 255, (Uint8)(90 + r * 30), (Uint8)(90 + r * 20), 255);
                SDL_RenderFillRect(sdl, &rect);
            }
        }

        // Paddle (wider + pink while EXPAND is active).
        const bool wide = expandTimer > 0.0f;
        SDL_Rect pad = {
            static_cast<int>(std::lround(paddleX)) * tileSize + sx,
            PADDLE_Y * tileSize + sy,
            paddleW() * tileSize,
            PADDLE_H * tileSize
        };
        SDL_SetRenderDrawColor(sdl, wide ? 255 : 80, wide ? 60 : 220,
                               wide ? 200 : 255, 255);
        SDL_RenderFillRect(sdl, &pad);

        // Balls: one bright cell each.
        for (const Ball& b : balls) {
            SDL_Rect ballRect = {
                static_cast<int>(std::lround(b.x)) * tileSize + sx,
                static_cast<int>(std::lround(b.y)) * tileSize + sy,
                tileSize, tileSize
            };
            SDL_SetRenderDrawColor(sdl, 240, 240, 240, 255);
            SDL_RenderFillRect(sdl, &ballRect);
        }

        // Falling power-up capsules: colored square with a white border.
        for (const PowerUp& p : powerups) {
            const int px = static_cast<int>(std::lround(p.x * tileSize)) + sx;
            const int py = static_cast<int>(std::lround(p.y * tileSize)) + sy;
            const SDL_Color c = powerupColor(p.type);
            SDL_Rect r = {px - tileSize / 2, py - tileSize / 2, tileSize, tileSize};
            SDL_SetRenderDrawColor(sdl, c.r, c.g, c.b, 255);
            SDL_RenderFillRect(sdl, &r);
            SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
            SDL_RenderDrawRect(sdl, &r);
        }

        // Laser bolts: bright rising beams, a red glow around a hot core.
        for (const LaserBolt& l : lasers) {
            if (l.spent) continue;
            const int lx = static_cast<int>(std::lround(l.x * tileSize)) + sx;
            const int ly = static_cast<int>(std::lround(l.y * tileSize)) + sy;
            SDL_SetRenderDrawColor(sdl, 255, 60, 60, 255);
            SDL_Rect glow = {lx - 2, ly, 5, tileSize * 4};
            SDL_RenderFillRect(sdl, &glow);
            SDL_SetRenderDrawColor(sdl, 255, 210, 210, 255);
            SDL_Rect core = {lx, ly, 1, tileSize * 4};
            SDL_RenderFillRect(sdl, &core);
        }

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
        state.stats["bricks_left"] = countBricks();
        state.stats["paddle_x"] = static_cast<int>(std::lround(paddleX));
        state.stats["paddle_w"] = paddleW();
        state.stats["balls"] = static_cast<int>(balls.size());
        state.stats["active_powerups"] = static_cast<int>(powerups.size());
        state.stats["expand_timer"] = static_cast<int>(std::ceil(expandTimer));
        state.stats["slow_timer"] = static_cast<int>(std::ceil(slowTimer));
        state.stats["sticky_timer"] = static_cast<int>(std::ceil(stickyTimer));
        state.stats["laser_bolts"] = static_cast<int>(lasers.size());
        state.stats["serving"] = serve ? 1 : 0;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        if (!balls.empty()) {
            state.stats["ball_x"] = static_cast<int>(std::lround(balls[0].x));
            state.stats["ball_y"] = static_cast<int>(std::lround(balls[0].y));
            state.entities["ball"] = {
                static_cast<int>(std::lround(balls[0].x)),
                static_cast<int>(std::lround(balls[0].y))
            };
        } else {
            state.stats["ball_x"] = 0;
            state.stats["ball_y"] = 0;
        }
        state.entities["paddle"] = {
            static_cast<int>(std::lround(paddleX)), PADDLE_Y
        };
        return state;
    }

private:
    uint32_t nextRand() {
        rng = rng * 1664525u + 1013904223u;
        return rng;
    }

    int countBricks() const {
        int n = 0;
        for (int b : bricks) n += b;
        return n;
    }

    int paddleW() const {
        return expandTimer > 0.0f ? PADDLE_W_WIDE : PADDLE_W;
    }

    float speedNow() const {
        return slowTimer > 0.0f ? baseSpeed * SLOW_FACTOR : baseSpeed;
    }

    void rescaleBalls(float newSpeed) {
        for (Ball& b : balls) {
            const float mag = std::sqrt(b.vx * b.vx + b.vy * b.vy);
            if (mag < 0.001f) continue;
            const float k = newSpeed / mag;
            b.vx *= k;
            b.vy *= k;
        }
    }

    void clampPaddleBy(float delta) {
        paddleX += delta;
        paddleX = std::max(0.0f, std::min(paddleX,
            static_cast<float>(GRID_W - paddleW())));
    }

    ActionResult movePaddle(int dir) {
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
        clampPaddleBy(static_cast<float>(dir) * LLM_PADDLE_STEP);
        result.success = true;
        result.message = "Paddle at column " +
                         std::to_string(static_cast<int>(std::lround(paddleX)));
        return result;
    }

    void resetServe() {
        serve = true;
        Ball b;
        b.x = paddleX + static_cast<float>(PADDLE_W) / 2.0f;
        b.y = static_cast<float>(PADDLE_Y) - 1.0f;
        b.vx = 0.0f;
        b.vy = 0.0f;
        balls.clear();
        balls.push_back(b);
        setMessage("SPACE or 'serve' to launch");
    }

    ActionResult doServe() {
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
        if (!serve) {
            result.message = "Ball already in play";
            return result;
        }
        serve = false;
        // Deterministic serve: slight angle alternates so games vary without
        // any randomness an LLM can't predict.
        const float angle = (score % 2 == 0) ? 12.0f : -12.0f;
        balls[0].vx = speedNow() * std::sin(angle * DEG_TO_RAD);
        balls[0].vy = -speedNow() * std::cos(angle * DEG_TO_RAD);
        sfx.play(uj::Sfx::Serve);
        setMessage("Serve!");
        updateHUD();
        result.success = true;
        result.message = "Ball in play";
        return result;
    }

    void deflect(Ball& b) {
        // Where the ball hit (0 = left edge, 1 = right edge) maps to a
        // launch angle of -MAX_ANGLE..+MAX_ANGLE from straight up.
        const float hit = (b.x - paddleX) / static_cast<float>(paddleW());
        const float angle = (hit - 0.5f) * 2.0f * MAX_ANGLE_DEG;
        baseSpeed = std::min(baseSpeed * SPEEDUP, MAX_SPEED);
        const float sp = speedNow();
        b.vx = sp * std::sin(angle * DEG_TO_RAD);
        b.vy = -sp * std::cos(angle * DEG_TO_RAD);
        // Nudge off the paddle so it cannot re-collide next frame.
        b.y = static_cast<float>(PADDLE_Y) - 1.0f;
        sfx.play(uj::Sfx::Ping);
        shake.add(0.12f);
        setMessage("Ping!");
        updateHUD();
    }

    void resolveBricks(Ball& b) {
        const int col = static_cast<int>(b.x);
        const int row = static_cast<int>(b.y);
        if (row < BRICK_TOP || row >= BRICK_TOP + BRICK_ROWS * 2) return;
        if (col < 2 || col >= GRID_W - 2) return;

        int brickRow = -1;
        for (int r = 0; r < BRICK_ROWS; ++r) {
            if (row >= BRICK_TOP + r * 2 && row < BRICK_TOP + r * 2 + 2) {
                brickRow = r;
                break;
            }
        }
        int brickCol = (col - 2) / BRICK_W;
        if (brickCol < 0 || brickCol >= BRICK_COLS) return;
        if (bricks[(size_t)brickRow * BRICK_COLS + brickCol] == 0) return;

        // Flip the dominant axis of travel so the bounce matches the hit.
        if (std::abs(b.vx) > std::abs(b.vy)) {
            b.vx = -b.vx;
        } else {
            b.vy = -b.vy;
        }

        destroyBrick(brickRow, brickCol);
        setMessage("Brick! (" + std::to_string(countBricks()) + " left)");
        updateHUD();

        if (countBricks() == 0) winCelebration();
    }

    // Shared brick-destruction path (ball hits AND laser bolts): score, the
    // shatter burst, shake, hit-stop, pop-up, sound, and the drop roll.
    void destroyBrick(int brickRow, int brickCol) {
        if (bricks[(size_t)brickRow * BRICK_COLS + brickCol] == 0) return;
        bricks[(size_t)brickRow * BRICK_COLS + brickCol] = 0;
        score += 10;

        const int bx = (2 + brickCol * BRICK_W) * tileSize;
        const int by = (BRICK_TOP + brickRow * 2) * tileSize;
        const SDL_Color color = {255, (uint8_t)(90 + brickRow * 30),
                                 (uint8_t)(90 + brickRow * 20), 255};
        particles.burst((float)(bx + tileSize * BRICK_W / 2), (float)(by + tileSize / 2), 14,
                          color, 8.0f, 0.5f, 5.0f);
        shake.add(0.22f);
        hitStop.trigger(0.05f);
        sfx.play(uj::Sfx::Thock);
        floatTexts.spawn(std::make_shared<TextDisplay>(bx, by - 12, "+10"),
                         bx, by - 12);

        // ---- Power-up drop: deterministic LCG decides whether/what ---------
        if (nextRand() % 100 < DROP_CHANCE_PCT) {
            const int type = static_cast<int>(nextRand() % NUM_PUPS);
            dropPowerup(type, 2 + brickCol * BRICK_W + BRICK_W / 2.0f,
                        BRICK_TOP + brickRow * 2 + 1.0f);
        }
    }

    void winCelebration() {
        shake.add(0.5f);
        hitStop.trigger(0.12f);
        sfx.play(uj::Sfx::Win);
        for (int i = 0; i < 3; ++i) {
            particles.burst((float)((i + 1) * 240), 300.0f, 20,
                            (i == 0) ? SDL_Color{255, 220, 60, 255} :
                            (i == 1) ? SDL_Color{80, 220, 255, 255} :
                                       SDL_Color{140, 255, 120, 255},
                            9.0f, 0.8f, 6.0f);
        }
        bestScore = std::max(bestScore, score);
        setMessage("WALL CLEARED! Best " + std::to_string(bestScore) +
                   " - Press R to play again");
        gameWon = true;
        endGame();
    }

    void dropPowerup(int type, float x, float y) {
        PowerUp p;
        p.type = type;
        p.x = x;
        p.y = y;
        powerups.push_back(p);
        sfx.play(uj::Sfx::Pickup);  // soft "capsule released" blip
    }

    void updatePowerups(float dt) {
        for (PowerUp& p : powerups) p.y += PUP_SPEED * dt;

        std::vector<PowerUp> surviving;
        surviving.reserve(powerups.size());
        for (PowerUp& p : powerups) {
            if (p.y > static_cast<float>(GRID_H - 1)) continue;  // fell off
            if (p.y >= static_cast<float>(PADDLE_Y) - 0.5f &&
                p.y <= static_cast<float>(PADDLE_Y) + 1.0f &&
                p.x >= paddleX - 0.5f &&
                p.x <= paddleX + static_cast<float>(paddleW()) + 0.5f) {
                activate(p.type);  // caught
                continue;
            }
            surviving.push_back(p);
        }
        powerups.swap(surviving);
    }

    void activate(int type) {
        const int px = static_cast<int>(std::lround(paddleX)) * tileSize;
        const int py = PADDLE_Y * tileSize;
        const SDL_Color c = powerupColor(type);
        particles.burst((float)px + paddleW() * tileSize / 2.0f,
                        (float)py + tileSize / 2.0f, 18, c, 9.0f, 0.6f, 5.0f);
        shake.add(0.25f);
        hitStop.trigger(0.06f);
        sfx.play(uj::Sfx::Coin);

        const char* label = "";
        switch (type) {
            case EXPAND:
                expandTimer = EXPAND_DURATION;
                label = "WIDE!";
                break;
            case MULTI:
                applyMulti();
                label = "MULTI BALL!";
                break;
            case SLOW:
                slowTimer = SLOW_DURATION;
                rescaleBalls(speedNow());
                label = "SLOW!";
                break;
            case LIFE:
                if (lives < MAX_LIVES_CAP) ++lives;
                label = "+1 LIFE!";
                break;
            case STICKY:
                stickyTimer = STICKY_DURATION;
                label = "STICKY!";
                break;
            case LASER:
                fireLasers();
                label = "LASER!";
                break;
            default:
                label = "POWER UP!";
                break;
        }
        floatTexts.spawn(std::make_shared<TextDisplay>(
            px, py - 20, label), px, py - 20);
        setMessage(std::string(label) + " (Best " +
                   std::to_string(std::max(bestScore, score)) + ")");
        updateHUD();
    }

    void applyMulti() {
        // A resting ball must be launched before it can split.
        if (serve) doServe();
        const size_t n = balls.size();
        for (size_t i = 0; i < n && balls.size() < MAX_BALLS; ++i) {
            // Copy by value: push_back below may reallocate `balls`, which
            // would invalidate a reference/iterator into it.
            const Ball b = balls[i];
            for (const float off : {20.0f, -20.0f}) {
                if (balls.size() >= MAX_BALLS) break;
                const float ang = std::atan2(b.vy, b.vx) + off * DEG_TO_RAD;
                Ball c;
                c.x = b.x;
                c.y = b.y;
                c.vx = speedNow() * std::cos(ang);
                c.vy = speedNow() * std::sin(ang);
                balls.push_back(c);
            }
        }
    }

    // ---- STICKY + LASER -----------------------------------------------------
    // STICKY: the ball sticks to the paddle instead of deflecting; it re-serves
    // when the player launches again (the classic re-aim catch).
    void stickyCatch() {
        serve = true;
        Ball b;
        b.x = paddleX + static_cast<float>(paddleW()) / 2.0f;
        b.y = static_cast<float>(PADDLE_Y) - 1.0f;
        b.vx = 0.0f;
        b.vy = 0.0f;
        balls.clear();
        balls.push_back(b);

        const SDL_Color c = powerupColor(STICKY);
        particles.burst((paddleX + paddleW() / 2.0f) * tileSize,
                        (PADDLE_Y - 1.0f) * tileSize, 16, c, 8.0f, 0.5f, 5.0f);
        shake.add(0.18f);
        hitStop.trigger(0.04f);
        sfx.play(uj::Sfx::Pickup);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            (int)(paddleX * tileSize), (PADDLE_Y - 2) * tileSize, "STICKY CATCH!"),
            (int)(paddleX * tileSize), (PADDLE_Y - 2) * tileSize);
        setMessage("Sticky catch - re-aim and serve");
        updateHUD();
    }

    // LASER: two bolts rise from the paddle; each burns the whole column it
    // lands on (column-burning), then the bolt expires.
    void fireLasers() {
        const float cx = paddleX + paddleW() / 2.0f;
        for (const float off : {paddleW() * -0.25f, paddleW() * 0.25f}) {
            LaserBolt b;
            b.x = cx + off;
            b.y = static_cast<float>(PADDLE_Y) - 2.0f;
            lasers.push_back(b);
        }
        sfx.play(uj::Sfx::Shoot);
    }

    void updateLasers(float dt) {
        for (LaserBolt& l : lasers) {
            if (l.spent) continue;
            l.y -= LASER_SPEED * dt;
            if (l.y < static_cast<float>(BRICK_TOP + BRICK_ROWS * 2)) {
                burnColumn(l.x);
                l.spent = true;
                if (!gameRunning) break;  // winning mid-volley ends the world
            }
        }
        lasers.erase(std::remove_if(lasers.begin(), lasers.end(),
            [](const LaserBolt& l) { return l.spent || l.y < 0.0f; }),
            lasers.end());
    }

    void burnColumn(float x) {
        const int brickCol = (static_cast<int>(x) - 2) / BRICK_W;
        if (brickCol < 0 || brickCol >= BRICK_COLS) return;
        int burned = 0;
        for (int r = BRICK_ROWS - 1; r >= 0; --r) {
            if (bricks[(size_t)r * BRICK_COLS + brickCol] == 0) continue;
            destroyBrick(r, brickCol);
            ++burned;
        }
        if (burned == 0) return;  // empty column: the bolt just flies through
        shake.add(0.3f);
        hitStop.trigger(0.08f);
        sfx.play(uj::Sfx::Clear);
        const int lx = (2 + brickCol * BRICK_W + BRICK_W / 2) * tileSize;
        floatTexts.spawn(std::make_shared<TextDisplay>(lx, BRICK_TOP * tileSize,
            "COLUMN CLEAR!"), lx, BRICK_TOP * tileSize);
        setMessage("Laser column clear!");
        updateHUD();
        if (countBricks() == 0) winCelebration();
    }

    void onLoseLife() {
        --lives;
        bestScore = std::max(bestScore, score);
        if (lives <= 0) {
            sfx.play(uj::Sfx::Lose);
            shake.add(0.45f);
            hitStop.trigger(0.10f);
            setMessage("GAME OVER - Best " + std::to_string(bestScore) +
                       " - Press R to restart");
            endGame();
            return;
        }
        sfx.play(uj::Sfx::Lose);
        shake.add(0.4f);
        hitStop.trigger(0.10f);
        particles.burst((float)(paddleX * tileSize), (float)(PADDLE_Y * tileSize), 18,
                        {240, 240, 240, 255}, 9.0f, 0.6f, 5.0f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            (int)(paddleX * tileSize), PADDLE_Y * tileSize - 14, "-1 LIFE"),
            (int)(paddleX * tileSize), PADDLE_Y * tileSize - 14);
        setMessage("Ball lost - " + std::to_string(lives) + " lives left");
        resetServe();
    }

    void autopilot(float dt) {
        smokeTimer += dt;
        if (serve) {
            if (smokeTimer > 0.4f) {
                smokeTimer = 0.0f;
                doServe();
            }
            return;
        }

        // Prefer the lowest falling capsule in the lower two-thirds of the
        // court; otherwise track the lowest ball (closest to the paddle).
        float targetX = paddleX + static_cast<float>(paddleW()) / 2.0f;
        const PowerUp* bestPup = nullptr;
        for (const PowerUp& p : powerups) {
            if (p.y > GRID_H * 0.35f && (!bestPup || p.y > bestPup->y)) {
                bestPup = &p;
            }
        }
        if (bestPup) {
            targetX = bestPup->x;
        } else if (!balls.empty()) {
            const Ball* lowest = &balls[0];
            for (const Ball& b : balls) if (b.y > lowest->y) lowest = &b;
            targetX = lowest->x;
        }

        const float dx = targetX - (paddleX + static_cast<float>(paddleW()) / 2.0f);
        if (std::abs(dx) > 1.0f) {
            clampPaddleBy((dx > 0.0f ? 1.0f : -1.0f) * PADDLE_SPEED * dt);
        }
    }

    // ---- Feel update ----------------------------------------------------------
    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
    }

    SDL_Color powerupColor(int type) const {
        switch (type) {
            case EXPAND: return {255, 60, 200, 255};   // magenta
            case MULTI:  return {60, 220, 255, 255};   // cyan
            case SLOW:   return {120, 255, 120, 255};  // green
            case LIFE:   return {255, 210, 60, 255};   // gold
            case STICKY: return {255, 150, 40, 255};   // orange
            case LASER:  return {255, 60, 60, 255};    // red
            default:     return {220, 220, 220, 255};
        }
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    void updateHUD() {
        if (!hud) return;
        std::string text = "Score " + std::to_string(score) + "    Lives " +
                           std::to_string(lives) + "    Bricks " +
                           std::to_string(countBricks()) + "    Best " +
                           std::to_string(std::max(bestScore, score));
        if (expandTimer > 0.0f) text += "    [WIDE " + fmt1(expandTimer) + "s]";
        if (slowTimer > 0.0f) text += "    [SLOW " + fmt1(slowTimer) + "s]";
        if (stickyTimer > 0.0f) text += "    [STICKY " + fmt1(stickyTimer) + "s]";
        hud->setText(text);
    }

    static std::string fmt1(float v) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1f", v);
        return std::string(buf);
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the BrickBreakerPlus class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static BrickBreakerPlus game;
#else
    BrickBreakerPlus game;
#endif
    game.run();
    return 0;
}
#endif
