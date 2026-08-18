// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Breakout - the classic brick-breaker, rebuilt for the 100-game program.
//
// This is the reference implementation of the AAA-feel bar (see GAMES.md):
// the ball uses real reflection physics, and every event is juiced - brick
// shatters throw particle bursts, hits shake the screen and hit-stop, score
// pops up as floating text, and all sound is synthesized procedurally in
// memory (zero asset files, identical native / WASM / headless).
//
// One code path serves human input and the LLM: the keyboard (A/D, arrows,
// mouse) polls the same clampPaddle() as the "move_paddle_left/right"
// actions, and SPACE / "serve" share doServe(). An LLM can play the exact
// game a human plays.
//
// Controls: A/D or arrows = move paddle (mouse also works), SPACE = serve,
//           P = pause, R = restart. Three lives, wall cleared = win.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Breakout : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 80;          // 80x50 cells @ 12px = 960x600
    static constexpr int GRID_H = 50;
    static constexpr int PADDLE_H = 2;
    static constexpr int PADDLE_W = 10;
    static constexpr int PADDLE_Y = GRID_H - 4;
    static constexpr float PADDLE_SPEED = 48.0f;   // cells/second
    static constexpr float LLM_PADDLE_STEP = 6.0f; // cells per LLM action
    static constexpr float BALL_SPEED = 22.0f;     // cells/second on serve
    static constexpr float SPEEDUP = 1.03f;        // per paddle hit
    static constexpr float MAX_SPEED = 40.0f;
    static constexpr float MAX_ANGLE_DEG = 55.0f;
    static constexpr float DEG_TO_RAD = 0.0174532925f;
    static constexpr int MAX_LIVES = 3;
    static constexpr int BRICK_ROWS = 5;
    static constexpr int BRICK_COLS = 12;
    static constexpr int BRICK_TOP = 5;

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
    std::vector<int> bricks;       // 1 = intact, 0 = broken (row-major)
    float paddleX = 0.0f;          // left cell of the paddle
    float ballX = 0.0f, ballY = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float speed = BALL_SPEED;
    int lives = MAX_LIVES;
    int score = 0;
    int bestScore = 0;             // session best; survives restarts
    bool serve = true;
    std::string statusText;
    float smokeTimer = 0.0f;
    int smokeStep = 0;

public:
    Breakout() : Game2D("Breakout", 960, 600, 12) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    void initGame() override {
        score = 0;
        lives = MAX_LIVES;
        speed = BALL_SPEED;
        paddleX = static_cast<float>(GRID_W / 2 - PADDLE_W / 2);
        paused = false;
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

        // Headless smoke mode: auto-serve and nudge the paddle around so a
        // dummy-driver run exercises serve -> rally -> brick -> score.
        if (smokeMode) {
            smokeTimer += dt;
            if (serve && smokeTimer > 0.4f) {
                smokeTimer = 0.0f;
                doServe();
            }
            clampPaddleBy(36.0f * dt * ((smokeStep++ % 2 == 0) ? 1.0f : -1.0f));
        }

        if (serve) {
            updateFx(dt);
            updateHUD();
            return;
        }

        // Integrate the ball.
        ballX += vx * dt;
        ballY += vy * dt;

        // Ceiling: exact mirror reflection.
        if (ballY < 0.0f) {
            ballY = -ballY;
            vy = -vy;
        }
        // Side walls.
        if (ballX < 0.0f) {
            ballX = -ballX;
            vx = -vx;
        } else if (ballX > static_cast<float>(GRID_W - 1)) {
            ballX = 2.0f * static_cast<float>(GRID_W - 1) - ballX;
            vx = -vx;
        }

        // Paddle collision - only while descending and overlapping its span.
        if (vy > 0.0f &&
            ballY >= static_cast<float>(PADDLE_Y) - 0.5f &&
            ballY <= static_cast<float>(PADDLE_Y) + 1.0f &&
            ballX >= paddleX - 0.5f &&
            ballX <= paddleX + static_cast<float>(PADDLE_W) + 0.5f) {
            deflect();
        }

        // Brick collisions: check the cell the ball is entering; flip only
        // the axis of impact so the ball never passes through a brick row.
        resolveBricks();

        // Dropped the ball: lose a life.
        if (ballY > static_cast<float>(GRID_H - 1)) {
            onLoseLife();
        }

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
                const int x = (2 + c * ((GRID_W - 4) / BRICK_COLS)) * tileSize;
                const int y = (BRICK_TOP + r * 2) * tileSize;
                SDL_Rect rect = {x + sx + 1, y + sy + 1,
                                 tileSize * ((GRID_W - 4) / BRICK_COLS) - 2,
                                 tileSize - 2};
                SDL_SetRenderDrawColor(sdl, 255, 90 + r * 30, 90 + r * 20, 255);
                SDL_RenderFillRect(sdl, &rect);
            }
        }

        // Paddle.
        SDL_Rect pad = {
            static_cast<int>(std::lround(paddleX)) * tileSize + sx,
            PADDLE_Y * tileSize + sy,
            PADDLE_W * tileSize,
            PADDLE_H * tileSize
        };
        SDL_SetRenderDrawColor(sdl, 80, 220, 255, 255);
        SDL_RenderFillRect(sdl, &pad);

        // Ball: one bright cell.
        SDL_Rect ballRect = {
            static_cast<int>(std::lround(ballX)) * tileSize + sx,
            static_cast<int>(std::lround(ballY)) * tileSize + sy,
            tileSize, tileSize
        };
        SDL_SetRenderDrawColor(sdl, 240, 240, 240, 255);
        SDL_RenderFillRect(sdl, &ballRect);

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
        state.stats["ball_x"] = static_cast<int>(std::lround(ballX));
        state.stats["ball_y"] = static_cast<int>(std::lround(ballY));
        state.stats["paddle_x"] = static_cast<int>(std::lround(paddleX));
        state.stats["serving"] = serve ? 1 : 0;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.entities["ball"] = {
            static_cast<int>(std::lround(ballX)),
            static_cast<int>(std::lround(ballY))
        };
        state.entities["paddle"] = {
            static_cast<int>(std::lround(paddleX)), PADDLE_Y
        };
        return state;
    }

private:
    int countBricks() const {
        int n = 0;
        for (int b : bricks) n += b;
        return n;
    }

    void clampPaddleBy(float delta) {
        paddleX += delta;
        paddleX = std::max(0.0f, std::min(paddleX,
            static_cast<float>(GRID_W - PADDLE_W)));
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
        speed = BALL_SPEED;
        vx = 0.0f;
        vy = 0.0f;
        ballX = paddleX + static_cast<float>(PADDLE_W) / 2.0f;
        ballY = static_cast<float>(PADDLE_Y) - 1.0f;
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
        vx = speed * std::sin(angle * DEG_TO_RAD);
        vy = -speed * std::cos(angle * DEG_TO_RAD);
        sfx.play(uj::Sfx::Serve);
        setMessage("Serve!");
        updateHUD();
        result.success = true;
        result.message = "Ball in play";
        return result;
    }

    void deflect() {
        // Where the ball hit (0 = left edge, 1 = right edge) maps to a
        // launch angle of -MAX_ANGLE..+MAX_ANGLE from straight up.
        const float hit = (ballX - paddleX) / static_cast<float>(PADDLE_W);
        const float angle = (hit - 0.5f) * 2.0f * MAX_ANGLE_DEG;
        speed = std::min(speed * SPEEDUP, MAX_SPEED);
        vx = speed * std::sin(angle * DEG_TO_RAD);
        vy = -speed * std::cos(angle * DEG_TO_RAD);
        // Nudge off the paddle so it cannot re-collide next frame.
        ballY = static_cast<float>(PADDLE_Y) - 1.0f;
        sfx.play(uj::Sfx::Ping);
        shake.add(0.12f);
        setMessage("Ping!");
        updateHUD();
    }

    void resolveBricks() {
        // The ball spans (ballX, ballY) -> (ballX+1, ballY+1); a brick's
        // cell is hit when the ball overlaps it. Flip only the axis of
        // impact so a corner never lets the ball skip past.
        const int col = static_cast<int>(ballX);
        const int row = static_cast<int>(ballY);
        if (row < BRICK_TOP || row >= BRICK_TOP + BRICK_ROWS * 2) return;
        if (col < 2 || col >= GRID_W - 2) return;

        int brickRow = -1;
        for (int r = 0; r < BRICK_ROWS; ++r) {
            if (row >= BRICK_TOP + r * 2 && row < BRICK_TOP + r * 2 + 2) {
                brickRow = r;
                break;
            }
        }
        int brickCol = (col - 2) / ((GRID_W - 4) / BRICK_COLS);
        if (brickCol < 0 || brickCol >= BRICK_COLS) return;
        if (bricks[(size_t)brickRow * BRICK_COLS + brickCol] == 0) return;

        bricks[(size_t)brickRow * BRICK_COLS + brickCol] = 0;
        score += 10;
        // Flip the dominant axis of travel so the bounce matches the hit.
        if (std::abs(vx) > std::abs(vy)) {
            vx = -vx;
        } else {
            vy = -vy;
        }

        // ---- Juice: shatter, shake, hit-stop, pop-up, sound ----------------
        const int bx = (2 + brickCol * ((GRID_W - 4) / BRICK_COLS)) * tileSize;
        const int by = (BRICK_TOP + brickRow * 2) * tileSize;
        const SDL_Color color = {255, (uint8_t)(90 + brickRow * 30),
                                 (uint8_t)(90 + brickRow * 20), 255};
        particles.burst(bx + tileSize, by + tileSize / 2, 14, color,
                        8.0f, 0.5f, 5.0f);
        shake.add(0.22f);
        hitStop.trigger(0.05f);
        sfx.play(uj::Sfx::Thock);
        floatTexts.spawn(std::make_shared<TextDisplay>(bx, by - 12, "+10"),
                         bx, by - 12);

        setMessage("Brick! (" + std::to_string(countBricks()) + " left)");
        updateHUD();

        if (countBricks() == 0) {
            // ---- Win: celebration -------------------------------------------
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
        particles.burst(ballX * tileSize, ballY * tileSize, 18,
                        {240, 240, 240, 255}, 9.0f, 0.6f, 5.0f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            (int)(ballX * tileSize), (int)(ballY * tileSize) - 14,
            "-1 LIFE"), (int)(ballX * tileSize), (int)(ballY * tileSize) - 14);
        setMessage("Ball lost - " + std::to_string(lives) + " lives left");
        resetServe();
    }

    // ---- Feel update ----------------------------------------------------------
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
        hud->setText("Score " + std::to_string(score) + "    Lives " +
                     std::to_string(lives) + "    Bricks " +
                     std::to_string(countBricks()) + "    Best " +
                     std::to_string(std::max(bestScore, score)));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the Breakout class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static Breakout game;
#else
    Breakout game;
#endif
    game.run();
    return 0;
}
#endif
