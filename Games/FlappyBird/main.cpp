// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Flappy Bird - the one-button gravity classic, game #10 of the 100-game
// program.
//
// The bird has a fixed x and a y velocity: gravity accelerates it downward
// every frame, and a flap snaps the velocity upward. Pipes scroll in from the
// right with a procedurally placed gap (fixed-seed LCG, so a given playthrough
// is deterministic); passing one scores a point, touching a pipe, the ground,
// or the ceiling ends the run.
//
// One code path serves human input and the LLM: SPACE/W/up and the "flap"
// action share the exact same flap(). An LLM can play the exact game a human
// plays - the state exposes the bird's y/velocity and the next pipe's gap.
//
// Controls: SPACE / W / up = flap, R = restart.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

class FlappyBird : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 80;          // 80x50 cells @ 12px = 960x600
    static constexpr int GRID_H = 50;
    static constexpr int BIRD_X = 20;          // fixed column of the bird
    static constexpr int BIRD_W = 2;           // 2x2-cell bird
    static constexpr int BIRD_H = 2;
    static constexpr int BIRD_SPAWN_Y = 20;
    static constexpr float GRAVITY = 30.0f;    // cells/s^2 downward
    static constexpr float FLAP_VY = -9.5f;    // cells/s impulse on flap
    static constexpr float MAX_FALL = 14.0f;   // terminal fall speed
    static constexpr float PIPE_SPEED = 12.0f; // cells/s scroll
    static constexpr int PIPE_W = 4;
    static constexpr int PIPE_GAP = 13;        // gap height in cells
    static constexpr float PIPE_INTERVAL = 2.2f; // seconds between pipes
    static constexpr float FIRST_PIPE = 0.6f;  // first pipe arrives fast
    static constexpr int GROUND_H = 3;         // solid ground rows at the bottom

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
    struct Pipe {
        float x;
        int gapTop;
        int gapBottom;
        bool scored = false;
    };
    std::vector<Pipe> pipes;
    float birdY = static_cast<float>(BIRD_SPAWN_Y);
    float birdVy = 0.0f;
    int score = 0;
    int bestScore = 0;                        // session best; survives restarts
    float pipeTimer = FIRST_PIPE;

    // Fixed-seed LCG: pipe gaps are deterministic and unit-testable.
    uint32_t lcgState = 0xB17D5EEDu;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;
    float smokeCooldown = 0.0f;

public:
    FlappyBird() : Game2D("Flappy Bird", 960, 600, 12) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    void initGame() override {
        score = 0;
        birdY = static_cast<float>(BIRD_SPAWN_Y);
        birdVy = 0.0f;
        pipeTimer = FIRST_PIPE;
        pipes.clear();
        smokeCooldown = 0.0f;
        paused = false;
        particles.clear();
        floatTexts = uj::FloatingText{};

        createGrid(GRID_W, GRID_H, tileSize);
        grid->fill({10, 14, 20, 255});
        grid->setBorderColor({16, 18, 26, 255});

        hud = createText(10, 6, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, GRID_H * tileSize - 26, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("SPACE or 'flap' to fly");

        registerAction("flap", [this]() { return doFlap(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_SPACE).onPress([this]() { doFlap(); });
        bindKey(KEY_W).onPress([this]() { doFlap(); });
        bindKey(KEY_UP).onPress([this]() { doFlap(); });
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

        // Hit-stop: the world stands still for a beat on a death.
        if (hitStop.frozen()) {
            hitStop.update(dt);
            return;
        }

        // Headless smoke mode: a bang-bang controller keeps the bird level
        // with the next gap - flap when below its center, coast when above.
        // With the ~1s of slack a 13-cell gap gives at 12 cells/s, this bot
        // survives comfortably and racks up score for the dummy-driver run.
        if (smokeMode) {
            smokeCooldown -= dt;
            if (smokeCooldown <= 0.0f) {
                // Aim at the next gap's center; before any pipe arrives,
                // just hold altitude so the bird doesn't free-fall and die.
                const Pipe* next = nextPipeAhead();
                const float target = next
                    ? (next->gapTop + next->gapBottom) * 0.5f - 0.5f
                    : static_cast<float>(BIRD_SPAWN_Y + 2);
                if (birdY + BIRD_H * 0.5f > target) {
                    flap();
                    smokeCooldown = 0.09f;  // avoids 60 Hz oscillation
                }
            }
        }

        // Physics: gravity pulls down, flap pushes up.
        birdVy += GRAVITY * dt;
        birdVy = std::min(birdVy, MAX_FALL);
        birdY += birdVy * dt;

        // Spawn pipes and scroll them left.
        pipeTimer -= dt;
        if (pipeTimer <= 0.0f) {
            pipeTimer += PIPE_INTERVAL;
            spawnPipe();
        }
        for (Pipe& p : pipes) p.x -= PIPE_SPEED * dt;
        pipes.erase(std::remove_if(pipes.begin(), pipes.end(),
            [](const Pipe& p) { return p.x + PIPE_W < 0.0f; }), pipes.end());

        // Score pipes the bird has fully passed.
        for (Pipe& p : pipes) {
            if (!p.scored && p.x + PIPE_W < BIRD_X) {
                p.scored = true;
                ++score;
                setMessage("Score " + std::to_string(score));
                // ---- Juice: score pop -------------------------------------
                const int px = static_cast<int>(std::lround(p.x + PIPE_W)) * tileSize;
                const int py = (p.gapTop + p.gapBottom) / 2 * tileSize;
                particles.burst(BIRD_X * tileSize + BIRD_W * tileSize / 2,
                                (int)std::lround(birdY) * tileSize +
                                    BIRD_H * tileSize / 2,
                                6, {245, 230, 120, 255}, 3.5f, 0.35f, 4.0f);
                floatTexts.spawn(std::make_shared<TextDisplay>(px, py, "+1"),
                                 px, py);
                sfx.play(uj::Sfx::Coin);
                updateHUD();
            }
        }

        // Collision: pipes, ground, ceiling.
        if (hitPipe() || birdY + BIRD_H >= GRID_H - GROUND_H || birdY < 0.0f) {
            onDeath();
        }

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        // World space shakes; the HUD and floating text do not.
        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // Pipes: green columns with a darker cap.
        for (const Pipe& p : pipes) {
            const int px = static_cast<int>(std::lround(p.x));
            drawPipe(sdl, px, 0, p.gapTop, sx, sy);
            drawPipe(sdl, px, p.gapBottom, GRID_H - GROUND_H - p.gapBottom,
                     sx, sy);
        }

        // Ground strip.
        SDL_Rect ground = {sx, (GRID_H - GROUND_H) * tileSize + sy,
                           GRID_W * tileSize, GROUND_H * tileSize};
        SDL_SetRenderDrawColor(sdl, 120, 90, 40, 255);
        SDL_RenderFillRect(sdl, &ground);

        // The bird: a 2x2 bright cell with a darker outline.
        SDL_Rect bird = {BIRD_X * tileSize + sx,
                         static_cast<int>(std::lround(birdY)) * tileSize + sy,
                         BIRD_W * tileSize, BIRD_H * tileSize};
        SDL_SetRenderDrawColor(sdl, 240, 220, 60, 255);
        SDL_RenderFillRect(sdl, &bird);
        SDL_SetRenderDrawColor(sdl, 160, 120, 20, 255);
        SDL_RenderDrawRect(sdl, &bird);

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
        state.stats["bird_x"] = BIRD_X;
        state.stats["bird_y"] = static_cast<int>(std::lround(birdY));
        state.stats["bird_vy"] = static_cast<int>(std::lround(birdVy));
        const Pipe* next = nextPipeAhead();
        state.stats["next_pipe_x"] = next ? static_cast<int>(std::lround(next->x)) : -1;
        state.stats["next_gap_top"] = next ? next->gapTop : -1;
        state.stats["next_gap_bottom"] = next ? next->gapBottom : -1;
        state.stats["pipes_ahead"] = static_cast<int>(pipes.size());
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.entities["bird"] = {BIRD_X, static_cast<int>(std::lround(birdY))};
        return state;
    }

private:
    // ---- Pipes -----------------------------------------------------------------
    void spawnPipe() {
        // Gap center stays clear of the ceiling and the ground strip.
        const int range = (GRID_H - GROUND_H) - PIPE_GAP - 4;
        const int center = 2 + static_cast<int>(lcgNext() % static_cast<uint32_t>(range)) + PIPE_GAP / 2;
        Pipe p;
        p.x = static_cast<float>(GRID_W);
        p.gapTop = center - PIPE_GAP / 2;
        p.gapBottom = center + PIPE_GAP / 2;
        pipes.push_back(p);
    }

    const Pipe* nextPipeAhead() const {
        for (const Pipe& p : pipes) {
            if (p.x + PIPE_W > BIRD_X) return &p;
        }
        return nullptr;
    }

    bool hitPipe() const {
        for (const Pipe& p : pipes) {
            if (p.x >= BIRD_X + BIRD_W) continue;         // pipe not here yet
            if (p.x + PIPE_W <= BIRD_X) continue;         // pipe fully passed
            // Overlaps the top or bottom pipe body?
            if (birdY < p.gapTop || birdY + BIRD_H > p.gapBottom) return true;
        }
        return false;
    }

    // ---- Bird -------------------------------------------------------------------
    void flap() {
        birdVy = FLAP_VY;
        // ---- Juice: hop puff ----------------------------------------------
        particles.burst(BIRD_X * tileSize + BIRD_W * tileSize / 2,
                        (int)std::lround(birdY + BIRD_H) * tileSize,
                        6, {255, 255, 255, 255}, 3.5f, 0.3f, 3.5f);
        shake.add(0.05f);
        sfx.play(uj::Sfx::Jump);
    }

    ActionResult doFlap() {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        flap();
        setMessage("Flap!");
        result.success = true;
        result.message = "Flapped";
        return result;
    }

    // ---- Death / fx ---------------------------------------------------------
    void onDeath() {
        bestScore = std::max(bestScore, score);
        // ---- Juice: death burst -------------------------------------------
        particles.burst(BIRD_X * tileSize + BIRD_W * tileSize / 2,
                        (int)std::lround(birdY) * tileSize +
                            BIRD_H * tileSize / 2,
                        22, {240, 220, 60, 255}, 10.0f, 0.7f, 5.0f);
        particles.burst(BIRD_X * tileSize + BIRD_W * tileSize / 2,
                        (int)std::lround(birdY) * tileSize +
                            BIRD_H * tileSize / 2,
                        8, {255, 255, 255, 255}, 7.0f, 0.5f, 4.0f);
        shake.add(0.55f);
        hitStop.trigger(0.12f);
        sfx.play(uj::Sfx::Lose);
        setMessage("GAME OVER - Score " + std::to_string(score) +
                   " - Best " + std::to_string(bestScore) +
                   " - Press R to restart");
        updateHUD();
        endGame();
    }

    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
    }

    // ---- Rendering -----------------------------------------------------------------
    void drawPipe(SDL_Renderer* sdl, int px, int py, int height,
                  int sx, int sy) {
        if (height <= 0) return;
        SDL_Rect body = {px * tileSize + sx, py * tileSize + sy,
                         PIPE_W * tileSize, height * tileSize};
        SDL_SetRenderDrawColor(sdl, 60, 190, 90, 255);
        SDL_RenderFillRect(sdl, &body);
        SDL_SetRenderDrawColor(sdl, 30, 110, 50, 255);
        SDL_RenderDrawRect(sdl, &body);
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Score " + std::to_string(score) + "  Best " +
                     std::to_string(std::max(bestScore, score)));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the FlappyBird class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static FlappyBird game;
#else
    FlappyBird game;
#endif
    game.run();
    return 0;
}
#endif
