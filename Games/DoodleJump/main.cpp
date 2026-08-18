// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Doodle Jump - the bouncy climber, game #11 of the 100-game program.
//
// The doodle rises by bouncing off platforms that scroll procedurally
// upward, and only falls if you miss one. Altitude is the score. Four
// platform types: normal (bounce), moving (drifts side to side), breakable
// (one bounce, then gone), and spring (launches you much higher). Edges
// wrap around. Falling past the bottom ends the run.
//
// Shipped with the GameJuice kit from day one (see Engine/Core/GameJuice.h):
// every bounce throws dust, springs throw a yellow burst with a bigger shake
// and a hit-stop beat, milestones chime, and all sound is synthesized in
// memory - identical native / WASM / headless. The doodle squashes on
// landing, stretches while rising, and points its nose where it steers;
// falling past the bottom bursts it and celebrates a new best.
//
// One code path serves human input and the LLM: A/D/arrows poll the same
// steer() as "move_left"/"move_right", so an LLM plays the exact game a
// human plays.
//
// Controls: A/D or arrows = steer (mouse also works), P = pause, R = restart.

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

class DoodleJump : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 80;          // 80x50 cells @ 12px = 960x600
    static constexpr int GRID_H = 50;
    static constexpr int PLATFORM_W = 4;
    static constexpr int PLAYER_W = 2;
    static constexpr int PLAYER_H = 3;

    static constexpr float GRAVITY = 55.0f;    // cells/s^2
    static constexpr float BOUNCE_VY = 28.0f;  // cells/s on a normal bounce
    static constexpr float SPRING_VY = 42.0f;  // spring launch
    static constexpr float MAX_FALL = 24.0f;
    static constexpr float MOVE_SPEED = 40.0f; // cells/s horizontal
    static constexpr float LLM_STEP = 6.0f;    // cells per LLM action

    static constexpr float BASE_X = 38.0f;     // the starting platform
    static constexpr float SPAWN_X = 39.0f;    // player left cell at spawn

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- Platform kinds ----------------------------------------------------
    enum Kind : int { Normal = 0, Moving, Breakable, Spring };

    struct Platform {
        float x, y;            // x = left cell; y = surface altitude (cells)
        Kind kind = Normal;
        float vx = 0.0f;       // horizontal drift for Moving
        bool dead = false;     // Breakable: broken after one bounce
    };

    // ---- State ------------------------------------------------------------
    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;
    std::vector<Platform> platforms;
    float px = SPAWN_X;        // player left cell
    float feet = 0.0f;         // player feet altitude (cells; up = higher)
    float vy = 0.0f;           // vertical velocity (+ = up)
    float camera = 0.0f;       // altitude of the bottom of the screen
    int score = 0;
    int bestScore = 0;         // session best; survives restarts
    int lastMilestone = 0;
    float nextPlatformHeight = 4.0f;
    float squash = 0.0f;       // landing squash 0..1 (eases back to rest)
    float stretch = 0.0f;      // launch stretch 0..1 (velocity-driven)
    int faceDir = 0;           // -1 left, 1 right: nose points this way

    // Fixed-seed LCG: platform layout is deterministic and unit-testable.
    uint32_t lcgState = 0xD00D1E5u;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }
    float lcgF() { return (float)(lcgNext() & 0xFFFFu) / 65535.0f; }

    std::string statusText;
    int smokeDir = 0;

public:
    DoodleJump() : Game2D("Doodle Jump", 960, 600, 12) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    void initGame() override {
        px = SPAWN_X;
        feet = 0.0f;
        vy = 0.0f;
        camera = 0.0f;
        score = 0;
        lastMilestone = 0;
        nextPlatformHeight = 4.0f;
        paused = false;
        smokeDir = 0;
        squash = 0.0f;
        stretch = 0.0f;
        faceDir = 0;
        platforms.clear();
        particles.clear();
        floatTexts = uj::FloatingText{};

        // The ground platform under the spawn point.
        platforms.push_back({BASE_X, 0.0f, Normal, 0.0f, false});
        generatePlatforms();

        // ---- Juice: spawn dust ---------------------------------------------
        particles.burst((float)((BASE_X + PLATFORM_W * 0.5f) * tileSize),
                        (float)((GRID_H - 1) * tileSize), 8,
                        {120, 220, 90, 255}, 5.0f, 0.4f, 4.0f);

        createGrid(GRID_W, GRID_H, tileSize);
        grid->fill({16, 20, 32, 255});
        grid->setBorderColor({12, 14, 22, 255});

        hud = createText(10, 6, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, GRID_H * tileSize - 26, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("A/D or arrows to steer");

        registerAction("move_left", [this]() { return steerBy(-LLM_STEP); });
        registerAction("move_right", [this]() { return steerBy(LLM_STEP); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

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

        // ---- Steer: keyboard + mouse + smoke autopilot ---------------------
        int steerDir = 0;
        if (input.isKeyHeld(KEY_A) || input.isKeyHeld(KEY_LEFT)) steerDir = -1;
        if (input.isKeyHeld(KEY_D) || input.isKeyHeld(KEY_RIGHT)) steerDir = 1;

        if (smokeMode) {
            // Aim at the highest reachable platform; bounce physics does the
            // climbing, this just keeps the doodle over its landing spot.
            smokeDir = autopilotDir();
            steerDir = smokeDir;
        }
        if (steerDir != 0) faceDir = steerDir;   // nose points where it steers

        px += steerDir * MOVE_SPEED * dt;
        // Wrap-around edges (classic Doodle Jump).
        if (px + PLAYER_W <= 0.0f) px = static_cast<float>(GRID_W - 1);
        if (px >= GRID_W) px = 0.0f;

        // ---- Physics --------------------------------------------------------
        const float prevFeet = feet;
        vy -= GRAVITY * dt;
        vy = std::max(vy, -MAX_FALL);
        feet += vy * dt;

        // Rising: stretch the doodle with its climb speed.
        if (vy > 0.0f) {
            stretch = std::max(stretch,
                std::min(1.0f, vy / (BOUNCE_VY * 1.3f)));
        }

        // Land on platforms while falling.
        if (vy < 0.0f) {
            for (Platform& p : platforms) {
                if (p.dead) continue;
                const bool overlapped =
                    prevFeet >= p.y && feet <= p.y &&
                    px + PLAYER_W > p.x && px < p.x + PLATFORM_W;
                if (overlapped) landOn(p);
            }
        }

        // Move drifting platforms and wrap their drift at the walls.
        for (Platform& p : platforms) {
            if (p.kind != Moving || p.dead) continue;
            p.x += p.vx * dt;
            if (p.x <= 0.0f) {
                p.x = 0.0f;
                p.vx = std::abs(p.vx);
                puffAtWall(p);
            } else if (p.x + PLATFORM_W >= GRID_W) {
                p.x = static_cast<float>(GRID_W - PLATFORM_W);
                p.vx = -std::abs(p.vx);
                puffAtWall(p);
            }
        }
        platforms.erase(std::remove_if(platforms.begin(), platforms.end(),
            [](const Platform& p) { return p.dead; }), platforms.end());

        // ---- Camera follows the climb (never slides back down) ---------------
        camera = std::max(camera, feet - GRID_H * 0.45f);

        // Cull platforms that scrolled off the bottom; generate new ones above.
        platforms.erase(std::remove_if(platforms.begin(), platforms.end(),
            [this](const Platform& p) { return p.y < camera - 4.0f; }),
            platforms.end());
        generatePlatforms();

        // ---- Score / milestones ------------------------------------------------
        score = std::max(score, static_cast<int>(feet));
        if (score - lastMilestone >= 50) {
            lastMilestone = score - (score % 50);
            sfx.play(uj::Sfx::Coin);
            particles.burst((float)(px * tileSize + tileSize),
                            (float)(screenY(feet) * tileSize), 6,
                            {230, 200, 60, 255}, 4.0f, 0.4f, 4.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                static_cast<int>(px * tileSize), 60, "+50"),
                static_cast<int>(px * tileSize), 60);
        }

        // ---- Game over: fell past the bottom --------------------------------
        const bool fell = feet < camera - 2.0f;
        if (fell) {
            const bool newBest = score > bestScore;
            bestScore = std::max(bestScore, score);
            // ---- Juice: death burst at the bottom edge ---------------------
            particles.burst((float)(GRID_W * 0.5f * tileSize),
                            (float)((GRID_H - 1) * tileSize), 20,
                            {120, 220, 90, 255}, 9.0f, 0.6f, 5.0f);
            particles.burst((float)(GRID_W * 0.5f * tileSize),
                            (float)((GRID_H - 1) * tileSize), 8,
                            {255, 255, 255, 255}, 6.0f, 0.45f, 4.0f);
            shake.add(0.55f);
            hitStop.trigger(0.12f);
            if (newBest) {
                sfx.play(uj::Sfx::Win);
                particles.burst(GRID_W * 0.5f * tileSize, 140.0f, 14,
                                {230, 200, 60, 255}, 8.0f, 0.7f, 5.0f);
                floatTexts.spawn(std::make_shared<TextDisplay>(
                    GRID_W * tileSize / 2 - 5 * tileSize, 120, "NEW BEST!"),
                    GRID_W * tileSize / 2 - 5 * tileSize, 120);
            } else {
                sfx.play(uj::Sfx::Lose);
            }
            setMessage("FELL! Best " + std::to_string(bestScore) +
                       " - Press R to restart");
            endGame();
        }

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // Platforms (world space).
        for (const Platform& p : platforms) {
            if (p.dead) continue;
            drawPlatform(sdl, p, sx, sy);
        }

        // The doodle: a green body with an eye and a snout. It squashes on
        // landing and stretches while rising; the nose points where it steers.
        const int dpx = static_cast<int>(std::lround(px));
        const float wMul = 1.0f + squash * 0.35f - stretch * 0.28f;
        const float hMul = 1.0f - squash * 0.35f + stretch * 0.28f;
        const int cx = (int)((dpx + PLAYER_W * 0.5f) * tileSize) + sx;
        const int bottom = screenY(feet) * tileSize + tileSize + sy;
        const int bw = std::max(8, (int)(PLAYER_W * tileSize * wMul));
        const int bh = std::max(8, (int)(PLAYER_H * tileSize * hMul));
        SDL_Rect body = {cx - bw / 2, bottom - bh, bw, bh};
        SDL_SetRenderDrawColor(sdl, 120, 220, 90, 255);
        SDL_RenderFillRect(sdl, &body);
        SDL_SetRenderDrawColor(sdl, 20, 20, 20, 255);
        SDL_Rect eye = {body.x + (faceDir >= 0 ? body.w - tileSize : 0),
                        body.y + std::max(2, bh / 4), tileSize, tileSize};
        SDL_RenderFillRect(sdl, &eye);
        SDL_SetRenderDrawColor(sdl, 70, 150, 60, 255);
        SDL_Rect snout = {body.x + (faceDir > 0 ? body.w - tileSize : 0),
                          body.y + std::max(tileSize, bh / 2),
                          tileSize, tileSize};
        SDL_RenderFillRect(sdl, &snout);

        // Particles live in world space (they shake with it); floating text
        // stays screen-stable.
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
        state.message = statusText;
        state.stats["score"] = score;
        state.stats["best"] = std::max(bestScore, score);
        state.stats["player_x"] = static_cast<int>(std::lround(px));
        state.stats["player_y"] = static_cast<int>(std::lround(feet));
        state.stats["player_vy"] = static_cast<int>(std::lround(vy));
        state.stats["camera"] = static_cast<int>(std::lround(camera));
        state.stats["platforms"] = static_cast<int>(platforms.size());
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.stats["squash"] = (int)std::lround(squash * 100.0f);
        state.stats["stretch"] = (int)std::lround(stretch * 100.0f);
        const Platform* t = bestTarget();
        state.stats["target_x"] = t ? static_cast<int>(std::lround(t->x)) : -1;
        state.stats["target_y"] = t ? static_cast<int>(std::lround(t->y)) : -1;
        state.entities["player"] = {
            static_cast<int>(std::lround(px)),
            static_cast<int>(std::lround(feet))
        };
        return state;
    }

private:
    // ---- Platform generation ----------------------------------------------------
    void generatePlatforms() {
        while (nextPlatformHeight < camera + GRID_H + 8.0f) {
            spawnPlatform(nextPlatformHeight);
            nextPlatformHeight += 3.0f + lcgF() * 3.0f;   // gap 3..6 cells
        }
    }

    void spawnPlatform(float y) {
        Platform p;
        p.y = y;
        p.x = 1.0f + lcgF() * (GRID_W - PLATFORM_W - 2);
        const float r = lcgF();
        const float ramp = std::min(0.6f, y / 250.0f);    // harder as you climb
        if (r < 0.08f + ramp * 0.30f) {
            p.kind = Spring;
        } else if (r < 0.20f + ramp * 0.30f) {
            p.kind = Breakable;
        } else if (r < 0.34f + ramp * 0.30f) {
            p.kind = Moving;
            p.vx = (lcgF() < 0.5f ? -1.0f : 1.0f) * 6.0f;
        } else {
            p.kind = Normal;
        }
        platforms.push_back(p);
    }

    // ---- Landing ----------------------------------------------------------------
    void landOn(Platform& p) {
        feet = p.y;
        if (p.kind == Spring) {
            vy = SPRING_VY;
            squash = 0.7f;
            sfx.play(uj::Sfx::Jump);
            shake.add(0.22f);
            hitStop.trigger(0.05f);
            particles.burst((float)(px * tileSize + tileSize), (float)(screenY(p.y) * tileSize),
                            18, {230, 200, 60, 255}, 10.0f, 0.5f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                static_cast<int>(px * tileSize), screenY(p.y) * tileSize - 16,
                "BOING!"), static_cast<int>(px * tileSize),
                screenY(p.y) * tileSize - 16);
            return;
        }
        if (p.kind == Breakable) {
            vy = BOUNCE_VY * 0.9f;
            p.dead = true;
            squash = 1.0f;
            sfx.play(uj::Sfx::Thock);
            shake.add(0.15f);
            hitStop.trigger(0.04f);
            particles.burst((float)((p.x + PLATFORM_W * 0.5f) * tileSize),
                            (float)(screenY(p.y) * tileSize), 14,
                            {158, 104, 52, 255}, 8.0f, 0.45f, 5.0f);
            return;
        }
        vy = BOUNCE_VY;
        squash = 1.0f;
        sfx.play(uj::Sfx::Jump);
        shake.add(0.10f);
        particles.burst((float)(px * tileSize + tileSize), (float)(screenY(p.y) * tileSize), 8,
                        {120, 220, 90, 255}, 6.0f, 0.4f, 4.0f);
    }

    // ---- Steering / autopilot ------------------------------------------------------
    ActionResult steerBy(float delta) {
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
        px += delta;
        if (px + PLAYER_W <= 0.0f) px = static_cast<float>(GRID_W - 1);
        if (px >= GRID_W) px = 0.0f;
        if (delta != 0.0f) faceDir = delta > 0.0f ? 1 : -1;
        result.success = true;
        result.message = "Doodle at column " +
                         std::to_string(static_cast<int>(std::lround(px)));
        return result;
    }

    // Highest platform within reach (the next landing target).
    const Platform* bestTarget() const {
        const Platform* best = nullptr;
        for (const Platform& p : platforms) {
            if (p.dead) continue;
            if (p.y < feet - 1.0f) continue;      // far below
            if (p.y > feet + 8.0f) continue;      // out of reach this bounce
            if (!best || p.y > best->y) best = &p;
        }
        return best;
    }

    int autopilotDir() {
        const Platform* t = bestTarget();
        if (!t) return 0;
        const float center = t->x + PLATFORM_W * 0.5f;
        const float me = px + PLAYER_W * 0.5f;
        if (me < center - 0.5f) return 1;
        if (me > center + 0.5f) return -1;
        return 0;
    }

    // ---- Rendering helpers -----------------------------------------------------------
    void puffAtWall(const Platform& p) {
        const int y = screenY(p.y);
        if (y < 0 || y >= GRID_H) return;      // off-screen: skip
        particles.burst((float)((p.x + PLATFORM_W * 0.5f) * tileSize),
                        (float)(y * tileSize), 4, {80, 150, 240, 255},
                        3.5f, 0.3f, 3.0f);
    }

    int screenY(float alt) const {
        return static_cast<int>(std::lround((camera + GRID_H - 1) - alt));
    }

    void drawPlatform(SDL_Renderer* sdl, const Platform& p, int sx, int sy) {
        const int x = static_cast<int>(std::lround(p.x)) * tileSize + sx;
        const int y = screenY(p.y) * tileSize + sy;
        SDL_Color col;
        switch (p.kind) {
            case Moving:    col = {80, 150, 240, 255}; break;
            case Breakable: col = {158, 104, 52, 255}; break;
            case Spring:    col = {120, 200, 120, 255}; break;
            default:        col = {60, 190, 90, 255}; break;
        }
        SDL_SetRenderDrawColor(sdl, col.r, col.g, col.b, 255);
        SDL_Rect body = {x, y, PLATFORM_W * tileSize, tileSize};
        SDL_RenderFillRect(sdl, &body);
        SDL_SetRenderDrawColor(sdl, col.r / 2, col.g / 2, col.b / 2, 255);
        SDL_RenderDrawRect(sdl, &body);
        if (p.kind == Spring) {
            // The spring coil poking up from the platform.
            SDL_SetRenderDrawColor(sdl, 230, 200, 60, 255);
            SDL_Rect coil = {x + tileSize, y - tileSize, tileSize * 2, tileSize};
            SDL_RenderFillRect(sdl, &coil);
        }
    }

    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
        // Landing squash and launch stretch ease back to rest.
        squash = std::max(0.0f, squash - 6.0f * dt);
        stretch = std::max(0.0f, stretch - 6.0f * dt);
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Height " + std::to_string(score) + "    Best " +
                     std::to_string(std::max(bestScore, score)));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the DoodleJump class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static DoodleJump game;
#else
    DoodleJump game;
#endif
    game.run();
    return 0;
}
#endif
