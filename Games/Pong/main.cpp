// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Pong - a real two-player Pong with continuous ball physics.
//
// The ball and paddles move in fractional cell coordinates with a proper
// velocity model (cells/second): reflection off the top/bottom walls,
// deflection angle set by where the ball hits the paddle, and a small
// speed-up per rally so points escalate. The static court (dark
// background + dashed center line) is drawn through the grid; the dynamic
// objects are filled rects at rounded cell positions - the same drawing
// primitive GridEntity::render uses internally.
//
// One code path serves both input and the LLM: keyboard W/S/UP/DOWN poll
// the same movePaddleBy() clamp as the "paddle_left_up" style actions, and
// SPACE / "serve" share doServe(). An LLM can therefore play the exact
// game a human plays.
//
// Controls:  W/S = left paddle, UP/DOWN = right paddle, SPACE = serve,
//            R = restart. First to 7 wins.

#include "Engine/Core/Game2D.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

class Pong : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 100;         // 100x60 cells @ 10px = 1000x600
    static constexpr int GRID_H = 60;
    static constexpr int PADDLE_H = 12;
    static constexpr int PADDLE_X_LEFT = 2;
    static constexpr int PADDLE_X_RIGHT = GRID_W - 3;
    static constexpr float PADDLE_SPEED = 42.0f;    // cells/second
    static constexpr float LLM_PADDLE_STEP = 4.0f;  // cells per LLM action
    static constexpr float BALL_SPEED = 24.0f;      // cells/second on serve
    static constexpr float SPEEDUP = 1.06f;         // per paddle hit
    // Max step = 54/60 = 0.9 cells/frame < the 1-cell paddle width, so the
    // ball can never tunnel through a paddle between frames.
    static constexpr float MAX_SPEED = 54.0f;
    static constexpr float MAX_ANGLE_DEG = 60.0f;   // deflection at paddle edge
    static constexpr float DEG_TO_RAD = 0.0174532925f;
    static constexpr int WIN_SCORE = 7;

    // ---- World ------------------------------------------------------------
    std::shared_ptr<TextDisplay> hud;      // score line
    std::shared_ptr<TextDisplay> message;  // event feedback line

    // ---- State ------------------------------------------------------------
    float paddleY[2] = {0.0f, 0.0f};       // top row of each paddle (0=left)
    float ballX = 0.0f, ballY = 0.0f;      // fractional cell position
    float vx = 0.0f, vy = 0.0f;            // cells/second
    float speed = BALL_SPEED;              // current scalar speed
    int score[2] = {0, 0};
    bool serve = true;                     // true = ball waiting to be served
    int serveSide = 1;                     // +1 left serves, -1 right serves
    std::string statusText;                // LLM-readable message mirror
    float smokeTimer = 0.0f;
    int smokeStep = 0;

public:
    Pong() : Game2D("Pong", 1000, 600, 10) {
        // Headless smoke mode: base-class auto-restart keeps the loop alive
        // across points/games; the autoplay below drives the physics.
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    void initGame() override {
        // Restart (R key / "restart" action) re-runs initGame - reset the
        // whole world explicitly so a restart is a clean slate.
        score[0] = 0;
        score[1] = 0;
        speed = BALL_SPEED;
        serveSide = 1;
        paddleY[0] = static_cast<float>(GRID_H / 2 - PADDLE_H / 2);
        paddleY[1] = static_cast<float>(GRID_H / 2 - PADDLE_H / 2);

        // Court: dark background + dashed center line, painted once through
        // the grid (renderGrid() draws it every frame).
        createGrid(GRID_W, GRID_H, tileSize);
        grid->fill({12, 14, 20, 255});
        grid->setBorderColor({18, 20, 30, 255});
        for (int y = 0; y < GRID_H; y += 2) {
            grid->setCellColor(GRID_W / 2, y, {26, 30, 44, 255});
        }

        hud = createText(10, 8, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, 30, "");
        message->setColor({255, 220, 120, 255});

        resetServe();
        updateHUD();

        // LLM actions - the SAME code paths as the keyboard bindings.
        registerAction("paddle_left_up", [this]() { return movePaddle(0, -1); });
        registerAction("paddle_left_down", [this]() { return movePaddle(0, 1); });
        registerAction("paddle_right_up", [this]() { return movePaddle(1, -1); });
        registerAction("paddle_right_down", [this]() { return movePaddle(1, 1); });
        registerAction("serve", [this]() { return doServe(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_SPACE).onPress([this]() { doServe(); });
        bindKey(KEY_R).onPress([this]() {
            if (gameOver) startGame();
        });
        // Paddles are driven by held-key polling in updateGame() so movement
        // is scaled by dt (the LLM path moves in fixed steps instead).
    }

    void updateGame(float dt) override {
        // Keyboard: continuous, dt-scaled paddle movement through the same
        // clamp the LLM actions use.
        if (input.isKeyHeld(KEY_W)) movePaddleBy(0, -PADDLE_SPEED * dt);
        if (input.isKeyHeld(KEY_S)) movePaddleBy(0, PADDLE_SPEED * dt);
        if (input.isKeyHeld(KEY_UP)) movePaddleBy(1, -PADDLE_SPEED * dt);
        if (input.isKeyHeld(KEY_DOWN)) movePaddleBy(1, PADDLE_SPEED * dt);

        // Headless smoke mode (PONG_SMOKE=1): auto-serve after a short delay
        // and keep re-serving so a dummy-driver run exercises the real
        // physics paths (serve -> rally -> deflect -> score) instead of
        // parking in the serve-wait state. Used by the CI smoke test.
        if (smokeMode) {
            smokeTimer += dt;
            if (serve && smokeTimer > 0.5f) {
                smokeTimer = 0.0f;
                doServe();
            }
            // Nudge both paddles so movement/clamping also gets exercised.
            movePaddleBy(0, 30.0f * dt * ((smokeStep++ % 2 == 0) ? 1.0f : -1.0f));
            movePaddleBy(1, 30.0f * dt * ((smokeStep % 2 == 0) ? -1.0f : 1.0f));
        }

        if (serve) {
            updateHUD();
            return;
        }

        // Integrate the ball.
        ballX += vx * dt;
        ballY += vy * dt;

        // Top/bottom walls: mirror the overshoot so the reflection is exact.
        if (ballY < 0.0f) {
            ballY = -ballY;
            vy = -vy;
        } else if (ballY > static_cast<float>(GRID_H - 1)) {
            ballY = 2.0f * static_cast<float>(GRID_H - 1) - ballY;
            vy = -vy;
        }

        // Paddle collision - only while moving toward the paddle and only
        // while the ball overlaps the paddle's row span.
        if (vx < 0.0f &&
            ballX >= static_cast<float>(PADDLE_X_LEFT) - 0.5f &&
            ballX <= static_cast<float>(PADDLE_X_LEFT) + 0.5f &&
            ballY >= paddleY[0] - 0.5f &&
            ballY <= paddleY[0] + static_cast<float>(PADDLE_H) + 0.5f) {
            deflect(0);
        } else if (vx > 0.0f &&
                   ballX >= static_cast<float>(PADDLE_X_RIGHT) - 0.5f &&
                   ballX <= static_cast<float>(PADDLE_X_RIGHT) + 0.5f &&
                   ballY >= paddleY[1] - 0.5f &&
                   ballY <= paddleY[1] + static_cast<float>(PADDLE_H) + 0.5f) {
            deflect(1);
        }

        // Scoring: the ball got past a paddle.
        if (ballX < 0.0f) {
            onPoint(1);  // left wall passed -> right scores
        } else if (ballX > static_cast<float>(GRID_W - 1)) {
            onPoint(0);  // right wall passed -> left scores
        }

        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        SDL_Renderer* sdl = getRenderer()->renderer;
        drawPaddle(sdl, 0, {80, 220, 255, 255});
        drawPaddle(sdl, 1, {255, 90, 120, 255});

        // Ball: one bright cell at the rounded fractional position.
        SDL_Rect ballRect = {
            static_cast<int>(std::lround(ballX)) * tileSize,
            static_cast<int>(std::lround(ballY)) * tileSize,
            tileSize,
            tileSize
        };
        SDL_SetRenderDrawColor(sdl, 240, 240, 240, 255);
        SDL_RenderFillRect(sdl, &ballRect);

        for (auto& t : textDisplays) t->render(getRenderer());
    }

    // ---- LLM state ----------------------------------------------------------
    GameState getState() const override {
        GameState state = Game2D::getState();
        state.score = score[0];
        state.message = statusText;
        state.stats["score_left"] = score[0];
        state.stats["score_right"] = score[1];
        state.stats["ball_x"] = static_cast<int>(std::lround(ballX));
        state.stats["ball_y"] = static_cast<int>(std::lround(ballY));
        state.stats["paddle_left_y"] = static_cast<int>(std::lround(paddleY[0]));
        state.stats["paddle_right_y"] = static_cast<int>(std::lround(paddleY[1]));
        state.stats["serving_left"] = serveSide > 0 ? 1 : 0;
        state.entities["ball"] = {
            static_cast<int>(std::lround(ballX)),
            static_cast<int>(std::lround(ballY))
        };
        state.entities["paddle_left"] = {
            PADDLE_X_LEFT, static_cast<int>(std::lround(paddleY[0]))
        };
        state.entities["paddle_right"] = {
            PADDLE_X_RIGHT, static_cast<int>(std::lround(paddleY[1]))
        };
        return state;
    }

private:
    // ---- One movement path, shared by input and the LLM ---------------------
    void movePaddleBy(int side, float delta) {
        paddleY[side] += delta;
        paddleY[side] = std::max(0.0f,
            std::min(paddleY[side], static_cast<float>(GRID_H - PADDLE_H)));
    }

    ActionResult movePaddle(int side, int dir) {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        movePaddleBy(side, static_cast<float>(dir) * LLM_PADDLE_STEP);
        result.success = true;
        result.message = (side == 0 ? "Left" : "Right") +
                         std::string(" paddle at row ") +
                         std::to_string(static_cast<int>(std::lround(paddleY[side])));
        return result;
    }

    void resetServe() {
        serve = true;
        speed = BALL_SPEED;
        vx = 0.0f;
        vy = 0.0f;
        if (serveSide > 0) {  // left serves: ball rests on the left paddle
            ballX = static_cast<float>(PADDLE_X_LEFT) + 0.5f;
            ballY = paddleY[0] + static_cast<float>(PADDLE_H) / 2.0f;
        } else {              // right serves
            ballX = static_cast<float>(PADDLE_X_RIGHT) - 0.5f;
            ballY = paddleY[1] + static_cast<float>(PADDLE_H) / 2.0f;
        }
        setMessage("SPACE or 'serve' to launch");
    }

    ActionResult doServe() {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        if (!serve) {
            result.message = "Ball already in play";
            return result;
        }
        serve = false;
        // Deterministic serve: the launch angle alternates each rally so
        // games vary without any randomness an LLM can't predict.
        const float angle = ((score[0] + score[1]) % 2 == 0) ? 15.0f : -15.0f;
        vx = static_cast<float>(serveSide) * BALL_SPEED * std::cos(angle * DEG_TO_RAD);
        vy = BALL_SPEED * std::sin(angle * DEG_TO_RAD);
        setMessage("Serve!");
        updateHUD();
        result.success = true;
        result.message = "Ball in play";
        return result;
    }

    void deflect(int side) {
        // Where the ball hit (0 = top edge, 1 = bottom edge) maps to a
        // launch angle of -MAX_ANGLE_DEG..+MAX_ANGLE_DEG.
        const float hit = (ballY - paddleY[side]) / static_cast<float>(PADDLE_H);
        const float angle = (hit - 0.5f) * 2.0f * MAX_ANGLE_DEG;
        speed = std::min(speed * SPEEDUP, MAX_SPEED);
        const float dir = (side == 0) ? 1.0f : -1.0f;
        vx = dir * speed * std::cos(angle * DEG_TO_RAD);
        vy = speed * std::sin(angle * DEG_TO_RAD);
        // Nudge the ball off the paddle so it cannot re-collide next frame.
        ballX = (side == 0) ? static_cast<float>(PADDLE_X_LEFT) + 1.0f
                            : static_cast<float>(PADDLE_X_RIGHT) - 1.0f;
        setMessage("Ping!");
        updateHUD();
    }

    void onPoint(int scorer) {
        score[scorer]++;
        setMessage(scorer == 0 ? "Left scores!" : "Right scores!");
        updateHUD();
        if (score[scorer] >= WIN_SCORE) {
            setMessage(scorer == 0 ? "LEFT WINS! Press R to play again"
                                   : "RIGHT WINS! Press R to play again");
            gameWon = true;
            endGame();
            return;
        }
        // The conceding side serves the next rally.
        serveSide = (scorer == 0) ? -1 : 1;
        resetServe();
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Left " + std::to_string(score[0]) + "    Right " +
                     std::to_string(score[1]) + "    (first to " +
                     std::to_string(WIN_SCORE) + ")");
    }

    void drawPaddle(SDL_Renderer* sdl, int side, SDL_Color color) {
        const int x = (side == 0 ? PADDLE_X_LEFT : PADDLE_X_RIGHT) * tileSize;
        const int y = static_cast<int>(std::lround(paddleY[side])) * tileSize;
        SDL_Rect rect = {x, y, tileSize, PADDLE_H * tileSize};
        SDL_SetRenderDrawColor(sdl, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(sdl, &rect);
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the Pong class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static Pong game;
#else
    Pong game;
#endif
    game.run();
    return 0;
}
#endif
