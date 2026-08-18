// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Asteroids - the drift-and-shoot classic, game #12 of the 100-game program.
//
// The ship is a point mass: rotation turns it, thrust accelerates it, and
// there is no friction - momentum carries it forever until you steer. The
// screen wraps on all four edges. Rocks drift and spin; a big rock splits
// into two mediums, a medium into two smalls. Clear the field to advance a
// level (each with more rocks); a bonus saucer crosses the top for extra
// points. Three lives; every 10,000 points earns another.
//
// Built with the GameJuice kit from day one (Engine/Core/GameJuice.h): a
// flickering thrust flame with exhaust particles, rock-break bursts with
// screen shake, hit-stop and floating scores, a ship-death explosion with a
// NEW BEST celebration, and all sound synthesized in memory - identical
// native / WASM / headless.
//
// One code path serves human input and the LLM: rotate_left/rotate_right/
// thrust/fire share the exact helpers the keyboard drives, so an LLM plays
// the exact game a human plays.
//
// Controls: A/D or left/right = rotate, W/up = thrust, SPACE = fire,
// P = pause, R = restart.

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

class Asteroids : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 80;          // 80x50 cells @ 12px = 960x600
    static constexpr int GRID_H = 50;
    static constexpr float MARGIN = 3.0f;      // wrap seam overshoot (cells)
    static constexpr float PI_F = 3.14159265f;

    static constexpr float TURN_RATE = 3.2f;   // rad/s
    static constexpr float THRUST = 55.0f;     // cells/s^2
    static constexpr float MAX_SPEED = 26.0f;  // cells/s
    static constexpr float BULLET_SPEED = 42.0f; // cells/s
    static constexpr float BULLET_LIFE = 1.6f;
    static constexpr int MAX_BULLETS = 4;      // classic in-flight cap
    static constexpr int MAX_LIVES = 3;
    static constexpr float SHIP_R = 1.4f;      // collision radius (cells)
    static constexpr float RESPAWN_TIME = 1.2f;
    static constexpr float INVULN_TIME = 2.0f;
    static constexpr float SAUCER_SPEED = 8.0f;
    static constexpr float SAUCER_INTERVAL = 20.0f;
    static constexpr float SAUCER_Y = 8.0f;
    static constexpr int EXTRA_LIFE_EVERY = 10000;

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    uj::ShipRespawn respawn{RESPAWN_TIME, INVULN_TIME};
    uj::SplitOnHit split;
    bool paused = false;

    // ---- World ------------------------------------------------------------
    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;

    struct Rock {
        float x, y, vx, vy;
        int size;              // 2 big, 1 medium, 0 small
        uint32_t seed = 0;     // sprite jitter + spin are derived from this
        float phase = 0.0f;    // sprite rotation, advances with spin
        float spin = 0.0f;
        float radius() const {
            return size == 2 ? 4.0f : size == 1 ? 2.5f : 1.5f;
        }
    };
    std::vector<Rock> rocks;
    uj::ProjectilePool bullets;

    // ---- State ------------------------------------------------------------
    float sx = 40.0f, sy = 25.0f;              // ship position (cells)
    float svx = 0.0f, svy = 0.0f;              // ship velocity (cells/s)
    float angle = -PI_F * 0.5f;                // rad; 0 = +x, -PI/2 = up
    int lives = MAX_LIVES;
    int score = 0;
    int bestScore = 0;                         // session best; survives restarts
    int level = 1;
    int extraLivesEarned = 0;
    bool thrusting = false;                    // only true for the current frame

    // Bonus saucer (passive, deterministic).
    bool saucerActive = false;
    float saucerX = 0.0f, saucerY = SAUCER_Y;
    int saucerDir = 1;
    float saucerTimer = SAUCER_INTERVAL * 0.5f;

    // Fixed-seed LCG: every playthrough (and test) is deterministic.
    // Mutable so const render helpers (flame flicker) can draw deterministically.
    mutable uint32_t lcgState = 0xA57E0D5u;
    uint32_t lcgNext() const {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }
    float lcgF() const { return (float)(lcgNext() & 0xFFFFu) / 65535.0f; }

    std::string statusText;
    float shotCooldown = 0.0f;

public:
    Asteroids() : Game2D("Asteroids", 960, 600, 12) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Headless-test hook: place the ship exactly (deterministic wrap and
    // collision tests). Pass invulnSeconds to make the drift tests immune to
    // the (deterministic) rock field. Not used by gameplay.
    void setShipForTest(float x, float y, float ang, float invulnSeconds) {
        sx = x; sy = y;
        svx = 0.0f; svy = 0.0f;
        angle = ang;
        respawn.grant(invulnSeconds);
    }

    void initGame() override {
        lives = MAX_LIVES;
        score = 0;
        level = 1;
        extraLivesEarned = 0;
        sx = GRID_W * 0.5f;
        sy = GRID_H * 0.5f;
        svx = 0.0f; svy = 0.0f;
        angle = -PI_F * 0.5f;
        respawn.reset();
        thrusting = false;
        shotCooldown = 0.0f;
        paused = false;
        saucerActive = false;
        saucerTimer = SAUCER_INTERVAL * 0.5f;
        rocks.clear();
        bullets.clear();
        bullets.setCap(MAX_BULLETS);
        particles.clear();
        floatTexts = uj::FloatingText{};
        initLevel(level);

        createGrid(GRID_W, GRID_H, tileSize);
        grid->fill({4, 6, 12, 255});
        grid->setBorderColor({10, 12, 20, 255});

        hud = createText(10, 6, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, GRID_H * tileSize - 26, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("A/D rotate, W thrust, SPACE fire");

        registerAction("rotate_left", [this]() { return rotateAction(-1); });
        registerAction("rotate_right", [this]() { return rotateAction(1); });
        registerAction("thrust", [this]() { return thrustAction(); });
        registerAction("fire", [this]() { return doFire(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_SPACE).onPress([this]() { fire(); });
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

        // Hit-stop: the world stands still for a beat on a break.
        if (hitStop.frozen()) {
            hitStop.update(dt);
            return;
        }

        thrusting = false;

        // ---- Steer: keyboard + smoke autopilot -----------------------------
        if (input.isKeyHeld(KEY_A) || input.isKeyHeld(KEY_LEFT)) {
            rotateBy(-TURN_RATE * dt);
        }
        if (input.isKeyHeld(KEY_D) || input.isKeyHeld(KEY_RIGHT)) {
            rotateBy(TURN_RATE * dt);
        }
        if (input.isKeyHeld(KEY_W) || input.isKeyHeld(KEY_UP)) {
            applyThrust(dt);
        }
        if (smokeMode) autopilot(dt);

        // ---- Ship physics: momentum, no friction ----------------------------
        sx += svx * dt;
        sy += svy * dt;
        wrap(sx, sy);

        // Thrust flame + exhaust particles.
        if (thrusting) {
            const float tailX = sx - std::cos(angle) * 1.5f;
            const float tailY = sy - std::sin(angle) * 1.5f;
            particles.burst(tailX * tileSize, tailY * tileSize, 1,
                            {255, 160, 60, 255}, 2.5f, 0.25f, 3.0f);
        }

        // ---- Bullets ----------------------------------------------------------
        bullets.update(dt, GRID_W, GRID_H);

        // ---- Rocks: drift + spin ---------------------------------------------
        for (Rock& r : rocks) {
            r.x += r.vx * dt;
            r.y += r.vy * dt;
            r.phase += r.spin * dt;
            wrap(r.x, r.y);
        }

        // ---- Bonus saucer -------------------------------------------------------
        saucerTimer -= dt;
        if (saucerTimer <= 0.0f && !saucerActive) {
            saucerActive = true;
            saucerDir = (lcgNext() & 1u) ? 1 : -1;
            saucerX = saucerDir > 0 ? -3.0f : GRID_W + 3.0f;
            saucerY = SAUCER_Y;
            saucerTimer = std::max(10.0f, SAUCER_INTERVAL - 2.0f * level);
        }
        if (saucerActive) {
            saucerX += saucerDir * SAUCER_SPEED * dt;
            if (saucerX < -6.0f || saucerX > GRID_W + 6.0f) saucerActive = false;
        }

        // ---- Collisions --------------------------------------------------------
        // Bullets vs rocks (check before erasing dead bullets).
        for (size_t bi = 0; bi < bullets.size();) {
            bool consumed = false;
            const auto& b = bullets.all()[bi];
            for (size_t ri = 0; ri < rocks.size(); ++ri) {
                if (distSq(b.x, b.y, rocks[ri].x, rocks[ri].y) <
                    rocks[ri].radius() * rocks[ri].radius()) {
                    breakRock(ri, b.x, b.y);
                    consumed = true;
                    break;
                }
            }
            if (consumed) {
                bullets.kill(bi);
            } else {
                ++bi;
            }
        }
        // Bullet vs saucer.
        for (size_t bi = 0; bi < bullets.size();) {
            const auto& b = bullets.all()[bi];
            if (saucerActive && distSq(b.x, b.y, saucerX, saucerY) < 3.0f) {
                hitSaucer(b.x, b.y);
                bullets.kill(bi);
            } else {
                ++bi;
            }
        }
        // Ship vs rocks / saucer.
        if (respawn.hittable()) {
            for (const Rock& r : rocks) {
                const float rr = r.radius() + SHIP_R;
                if (distSq(r.x, r.y, sx, sy) < rr * rr) {
                    shipHit();
                    break;
                }
            }
            if (gameRunning && saucerActive &&
                distSq(saucerX, saucerY, sx, sy) < 2.2f * 2.2f) {
                shipHit();
            }
        }

        // ---- Level clear ---------------------------------------------------------
        if (gameRunning && rocks.empty()) levelUp();

        // ---- Timers --------------------------------------------------------------
        if (respawn.update(dt)) {
            setMessage("Respawned - " + std::to_string(lives) + " lives left");
        }
        if (shotCooldown > 0.0f) shotCooldown -= dt;

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        // World space shakes; the HUD and floating text do not.
        const auto [sx0, sy0] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // Rocks (wrapped at the seam so the field reads continuous).
        for (const Rock& r : rocks) {
            const SDL_Color col = rockColor(r.size);
            const int px = (int)std::lround(r.x) * tileSize;
            const int py = (int)std::lround(r.y) * tileSize;
            const int radPx = (int)(r.radius() * tileSize) + 4;
            renderWrapped(px + sx0, py + sy0, radPx,
                [&](int ox, int oy) {
                    const auto poly = rockPoly(r, ox, oy);
                    fillPoly(sdl, poly, col);
                    drawPoly(sdl, poly, {(Uint8)(col.r / 2),
                                         (Uint8)(col.g / 2),
                                         (Uint8)(col.b / 2), 255});
                });
        }

        // Bonus saucer: a purple 3-cell body with a glow tip.
        if (saucerActive) {
            const int spx = (int)std::lround(saucerX) * tileSize + sx0;
            const int spy = (int)std::lround(saucerY) * tileSize + sy0;
            SDL_Rect body = {spx - tileSize, spy - tileSize / 2,
                             3 * tileSize, tileSize};
            SDL_SetRenderDrawColor(sdl, 230, 120, 255, 255);
            SDL_RenderFillRect(sdl, &body);
            SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
            SDL_Rect tip = {spx + (saucerDir > 0 ? 2 * tileSize : -tileSize),
                            spy - tileSize / 2, tileSize, tileSize};
            SDL_RenderFillRect(sdl, &tip);
        }

        // Bullets: bright 1-cell dots.
        for (const auto& b : bullets.all()) {
            SDL_Rect br = {(int)std::lround(b.x) * tileSize + sx0,
                           (int)std::lround(b.y) * tileSize + sy0,
                           tileSize / 2, tileSize / 2};
            SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
            SDL_RenderFillRect(sdl, &br);
        }

        // The ship (blinks while invulnerable, hidden while respawning).
        if (respawn.visible()) drawShip(sdl, sx0, sy0);

        // Particles live in world space (they shake with it); floating text
        // stays screen-stable.
        particles.render(sdl, sx0, sy0);
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
        state.stats["lives"] = lives;
        state.stats["level"] = level;
        state.stats["ship_x"] = (int)std::lround(sx);
        state.stats["ship_y"] = (int)std::lround(sy);
        state.stats["ship_angle"] = deg(angle);
        state.stats["ship_vx"] = (int)std::lround(svx);
        state.stats["ship_vy"] = (int)std::lround(svy);
        state.stats["rocks"] = static_cast<int>(rocks.size());
        state.stats["bullets"] = static_cast<int>(bullets.size());
        state.stats["saucer_active"] = saucerActive ? 1 : 0;
        const Rock* n = nearestRock();
        state.stats["nearest_rock_x"] = n ? (int)std::lround(n->x) : -1;
        state.stats["nearest_rock_y"] = n ? (int)std::lround(n->y) : -1;
        state.stats["nearest_rock_dist"] = n
            ? (int)std::lround(std::hypot(n->x - sx, n->y - sy)) : -1;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.entities["ship"] = {(int)std::lround(sx), (int)std::lround(sy)};
        return state;
    }

private:
    // ---- Level / rocks ----------------------------------------------------------
    void initLevel(int n) {
        rocks.clear();
        bullets.clear();
        const int count = std::min(3 + n, 9);
        for (int i = 0; i < count; ++i) spawnRock(2, 10.0f);
    }

    void spawnRock(int size, float minDistFromShip) {
        Rock r;
        r.size = size;
        for (int tries = 0; tries < 30; ++tries) {
            r.x = 4.0f + lcgF() * (GRID_W - 8.0f);
            r.y = 4.0f + lcgF() * (GRID_H - 8.0f);
            if (std::hypot(r.x - sx, r.y - sy) >= minDistFromShip) break;
        }
        const float ang = lcgF() * 2.0f * PI_F;
        const float sp = 3.0f + lcgF() * 4.0f;
        r.vx = std::cos(ang) * sp;
        r.vy = std::sin(ang) * sp;
        r.seed = lcgNext();
        r.spin = (lcgF() - 0.5f) * 2.4f;
        rocks.push_back(r);
    }

    void breakRock(size_t ri, float hitX, float hitY) {
        Rock r = rocks[ri];
        const int pts = r.size == 2 ? 20 : r.size == 1 ? 50 : 100;
        score += pts;
        // ---- Juice: shatter, shake, hit-stop, score pop ---------------------
        const SDL_Color col = rockColor(r.size);
        particles.burst(r.x * tileSize, r.y * tileSize,
                        r.size == 2 ? 16 : 10, col,
                        r.size == 2 ? 9.0f : 7.0f, 0.5f, 5.0f);
        shake.add(r.size == 2 ? 0.25f : 0.15f);
        hitStop.trigger(r.size == 2 ? 0.05f : 0.03f);
        sfx.play(uj::Sfx::Explode);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            (int)std::lround(hitX) * tileSize,
            (int)std::lround(hitY) * tileSize - 14,
            "+" + std::to_string(pts)),
            (int)std::lround(hitX) * tileSize,
            (int)std::lround(hitY) * tileSize - 14);
        // Two smaller children inherit momentum + a split impulse.
        if (split.splits(r.size)) {
            for (int i = 0; i < 2; ++i) {
                const auto c = split.child(r.vx, r.vy, 0.7f, 4.0f, 7.0f, 1.2f);
                Rock child;
                child.size = split.childTier(r.size);
                child.x = r.x;
                child.y = r.y;
                child.vx = c.vx;
                child.vy = c.vy;
                child.seed = c.seed;
                child.spin = c.spin;
                rocks.push_back(child);
            }
        }
        rocks.erase(rocks.begin() + (long)ri);
        // Extra life at 10k intervals.
        if (score / EXTRA_LIFE_EVERY > extraLivesEarned) {
            extraLivesEarned = score / EXTRA_LIFE_EVERY;
            ++lives;
            sfx.play(uj::Sfx::Coin);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                GRID_W * tileSize / 2 - 6 * tileSize, 120, "EXTRA LIFE!"),
                GRID_W * tileSize / 2 - 6 * tileSize, 120);
        }
        updateHUD();
    }

    void levelUp() {
        ++level;
        score += 1000;
        // ---- Juice: fanfare + celebration -----------------------------------
        sfx.play(uj::Sfx::Win);
        shake.add(0.35f);
        hitStop.trigger(0.08f);
        particles.burst(GRID_W * 0.5f * tileSize, GRID_H * 0.4f * tileSize, 18,
                        {120, 220, 255, 255}, 9.0f, 0.8f, 6.0f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            GRID_W * tileSize / 2 - 5 * tileSize, 90,
            "LEVEL " + std::to_string(level)),
            GRID_W * tileSize / 2 - 5 * tileSize, 90);
        setMessage("Level " + std::to_string(level) + " - " +
                   std::to_string(std::min(3 + level, 9)) + " rocks");
        initLevel(level);
        updateHUD();
    }

    void hitSaucer(float bx, float by) {
        saucerActive = false;
        score += 200;
        // ---- Juice: payoff ---------------------------------------------------
        particles.burst(saucerX * tileSize, saucerY * tileSize, 14,
                        {230, 120, 255, 255}, 9.0f, 0.6f, 5.0f);
        shake.add(0.35f);
        hitStop.trigger(0.06f);
        sfx.play(uj::Sfx::Coin);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            (int)std::lround(bx) * tileSize,
            (int)std::lround(by) * tileSize - 14, "+200"),
            (int)std::lround(bx) * tileSize,
            (int)std::lround(by) * tileSize - 14);
        updateHUD();
    }

    void shipHit() {
        --lives;
        // ---- Juice: the ship bursts -----------------------------------------
        particles.burst(sx * tileSize, sy * tileSize, 24,
                        {120, 220, 255, 255}, 10.0f, 0.7f, 6.0f);
        particles.burst(sx * tileSize, sy * tileSize, 8,
                        {255, 255, 255, 255}, 7.0f, 0.5f, 5.0f);
        shake.add(0.6f);
        hitStop.trigger(0.15f);
        if (lives <= 0) {
            const bool newBest = score > bestScore;
            bestScore = std::max(bestScore, score);
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
            setMessage("GAME OVER - Best " + std::to_string(bestScore) +
                       " - Press R to restart");
            endGame();
            return;
        }
        sfx.play(uj::Sfx::Explode);
        // Respawn in the middle with a fresh heading and invulnerability.
        sx = GRID_W * 0.5f;
        sy = GRID_H * 0.5f;
        svx = 0.0f; svy = 0.0f;
        angle = -PI_F * 0.5f;
        respawn.start();
        setMessage("Ship lost - " + std::to_string(lives) + " lives left");
        updateHUD();
    }

    // ---- Ship ---------------------------------------------------------------
    void rotateBy(float rad) { angle += rad; }

    void applyThrust(float dt) {
        svx += std::cos(angle) * THRUST * dt;
        svy += std::sin(angle) * THRUST * dt;
        const float sp = std::hypot(svx, svy);
        if (sp > MAX_SPEED) {
            svx *= MAX_SPEED / sp;
            svy *= MAX_SPEED / sp;
        }
        thrusting = true;
    }

    bool fire() {
        if (!gameRunning || paused || respawn.waiting()) return false;
        const bool ok = bullets.fire(
            sx + std::cos(angle) * 2.0f,
            sy + std::sin(angle) * 2.0f,
            std::cos(angle) * BULLET_SPEED + svx * 0.3f,
            std::sin(angle) * BULLET_SPEED + svy * 0.3f,
            BULLET_LIFE);
        if (!ok) return false;
        sfx.play(uj::Sfx::Shoot);
        shake.add(0.04f);
        return true;
    }

    // ---- LLM actions ---------------------------------------------------------
    ActionResult rotateAction(int dir) {
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
        rotateBy(dir * 0.35f);   // ~20 degrees per action
        result.success = true;
        result.message = std::string(dir < 0 ? "Rotated left" : "Rotated right") +
                         " - heading " + std::to_string(deg(angle)) + " deg";
        return result;
    }

    ActionResult thrustAction() {
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
        applyThrust(0.1f);       // a fixed impulse, same physics as W/up
        result.success = true;
        result.message = "Thrust - velocity " +
            std::to_string((int)std::lround(std::hypot(svx, svy))) + " cells/s";
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
        if (!fire()) {
            result.message = (int)bullets.size() >= MAX_BULLETS
                ? "Max " + std::to_string(MAX_BULLETS) + " bullets in flight"
                : "Cannot fire yet";
            return result;
        }
        result.success = true;
        result.message = "Fired";
        return result;
    }

    // ---- Autopilot --------------------------------------------------------------
    const Rock* nearestRock() const {
        const Rock* best = nullptr;
        float bd = 1e18f;
        for (const Rock& r : rocks) {
            const float d = distSq(r.x, r.y, sx, sy);
            if (d < bd) { bd = d; best = &r; }
        }
        return best;
    }

    void autopilot(float dt) {
        const Rock* t = nearestRock();
        if (!t) return;
        const float aim = std::atan2(t->y - sy, t->x - sx);
        float diff = aim - angle;
        while (diff > PI_F) diff -= 2.0f * PI_F;
        while (diff < -PI_F) diff += 2.0f * PI_F;
        const float distR = std::hypot(t->x - sx, t->y - sy);
        if (std::fabs(diff) > 0.25f) {
            rotateBy((diff > 0.0f ? 1.0f : -1.0f) * TURN_RATE * dt);
        } else {
            // Roughly facing the target: close in, but stop short of ramming.
            if (distR > 12.0f) applyThrust(dt);
            shotCooldown -= dt;
            if (std::fabs(diff) < 0.30f && shotCooldown <= 0.0f &&
                (int)bullets.size() < MAX_BULLETS) {
                if (fire()) shotCooldown = 0.4f;
            }
        }
    }

    // ---- Helpers ----------------------------------------------------------------
    static float distSq(float x1, float y1, float x2, float y2) {
        const float dx = x1 - x2, dy = y1 - y2;
        return dx * dx + dy * dy;
    }

    static int deg(float rad) {
        int d = (int)std::lround(rad * 180.0f / PI_F) % 360;
        return d < 0 ? d + 360 : d;
    }

    void wrap(float& x, float& y) {
        if (x < -MARGIN) x += GRID_W + 2.0f * MARGIN;
        else if (x > GRID_W + MARGIN) x -= GRID_W + 2.0f * MARGIN;
        if (y < -MARGIN) y += GRID_H + 2.0f * MARGIN;
        else if (y > GRID_H + MARGIN) y -= GRID_H + 2.0f * MARGIN;
    }

    static SDL_Color rockColor(int size) {
        // Pale gray-blue; smaller rocks are lighter (classic).
        if (size == 2) return {150, 155, 175, 255};
        if (size == 1) return {180, 185, 205, 255};
        return {210, 215, 235, 255};
    }

    // Jagged n-gon sprite for a rock; the jitter comes from the rock's seed
    // and the whole shape rotates via phase (per-rock spin).
    std::vector<SDL_Point> rockPoly(const Rock& r, int px, int py) const {
        const int n = 11;
        const float rad = r.radius() * tileSize;
        std::vector<SDL_Point> pts;
        pts.reserve((size_t)n);
        uint32_t h = r.seed;
        for (int i = 0; i < n; ++i) {
            h = h * 1664525u + 1013904223u;
            const float jit = 0.72f + ((float)(h & 0xFFu) / 255.0f) * 0.5f;
            const float a = (float)i / (float)n * 2.0f * PI_F + r.phase;
            pts.push_back({px + (int)(std::cos(a) * rad * jit),
                           py + (int)(std::sin(a) * rad * jit)});
        }
        return pts;
    }

    // Scanline-filled polygon (flat scanline pairs), so rocks and the ship
    // render as clean pixel-art shapes without any asset files.
    void fillPoly(SDL_Renderer* sdl, const std::vector<SDL_Point>& pts,
                  SDL_Color col) const {
        if (pts.size() < 3) return;
        int ymin = pts[0].y, ymax = pts[0].y;
        for (const SDL_Point& p : pts) {
            ymin = std::min(ymin, p.y);
            ymax = std::max(ymax, p.y);
        }
        SDL_SetRenderDrawColor(sdl, col.r, col.g, col.b, col.a);
        const int n = (int)pts.size();
        for (int y = ymin; y <= ymax; ++y) {
            const float scanY = (float)y + 0.5f;
            float xs[64];
            int xc = 0;
            for (int i = 0; i < n; ++i) {
                const SDL_Point& a = pts[i];
                const SDL_Point& b = pts[(i + 1) % n];
                if (a.y == b.y) continue;                 // horizontal edge
                if (scanY < std::min(a.y, b.y) ||
                    scanY >= std::max(a.y, b.y)) continue;
                xs[xc++] = (float)a.x + (scanY - (float)a.y) *
                    ((float)b.x - (float)a.x) / ((float)b.y - (float)a.y);
            }
            std::sort(xs, xs + xc);
            for (int i = 0; i + 1 < xc; i += 2) {
                const int x0 = (int)xs[i];
                const int x1 = (int)xs[i + 1];
                SDL_Rect row = {x0, y, x1 - x0 + 1, 1};
                SDL_RenderFillRect(sdl, &row);
            }
        }
    }

    void drawPoly(SDL_Renderer* sdl, const std::vector<SDL_Point>& pts,
                  SDL_Color col) const {
        SDL_SetRenderDrawColor(sdl, col.r, col.g, col.b, col.a);
        const int n = (int)pts.size();
        for (int i = 0; i < n; ++i) {
            const SDL_Point& a = pts[i];
            const SDL_Point& b = pts[(i + 1) % n];
            SDL_RenderDrawLine(sdl, a.x, a.y, b.x, b.y);
        }
    }

    // Draw the ship at pixel (px, py), duplicated across the wrap seam.
    void drawShip(SDL_Renderer* sdl, int sx0, int sy0) const {
        const int px = (int)std::lround(sx) * tileSize + sx0;
        const int py = (int)std::lround(sy) * tileSize + sy0;
        const int rad = 3 * tileSize;
        renderWrapped(px, py, rad, [&](int ox, int oy) {
            const float L = 2.4f * tileSize;    // nose length
            const float W = 1.5f * tileSize;    // back half-width
            std::vector<SDL_Point> tri;
            tri.push_back({ox + (int)(std::cos(angle) * L),
                           oy + (int)(std::sin(angle) * L)});
            tri.push_back({ox + (int)(std::cos(angle + 2.45f) * W),
                           oy + (int)(std::sin(angle + 2.45f) * W)});
            tri.push_back({ox + (int)(std::cos(angle - 2.45f) * W),
                           oy + (int)(std::sin(angle - 2.45f) * W)});
            fillPoly(sdl, tri, {120, 220, 255, 255});
            drawPoly(sdl, tri, {60, 130, 160, 255});
            // Thrust flame: a flickering orange triangle at the tail.
            if (thrusting) {
                const float fl = (0.7f + lcgF() * 0.6f) * W;
                std::vector<SDL_Point> flame;
                flame.push_back({ox - (int)(std::cos(angle) * fl),
                                 oy - (int)(std::sin(angle) * fl)});
                flame.push_back({ox + (int)(std::cos(angle + 2.45f) * W),
                                 oy + (int)(std::sin(angle + 2.45f) * W)});
                flame.push_back({ox + (int)(std::cos(angle - 2.45f) * W),
                                 oy + (int)(std::sin(angle - 2.45f) * W)});
                fillPoly(sdl, flame, {255, 160, 60, 255});
            }
        });
    }

    template <typename F>
    void renderWrapped(int px, int py, int rad, F draw) const {
        draw(px, py);
        if (px - rad < 0) draw(px + GRID_W * tileSize, py);
        if (px + rad >= GRID_W * tileSize) draw(px - GRID_W * tileSize, py);
        if (py - rad < 0) draw(px, py + GRID_H * tileSize);
        if (py + rad >= GRID_H * tileSize) draw(px, py - GRID_H * tileSize);
    }

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
        hud->setText("Score " + std::to_string(score) + "    Best " +
                     std::to_string(std::max(bestScore, score)) +
                     "    Level " + std::to_string(level) +
                     "    Lives " + std::to_string(lives));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the Asteroids class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static Asteroids game;
#else
    Asteroids game;
#endif
    game.run();
    return 0;
}
#endif
