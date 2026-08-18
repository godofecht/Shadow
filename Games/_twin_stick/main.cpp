// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Twin Stick Starter - the arena shooter starting point.
//
// This is the generator template for `tools/new_game.sh --twin-stick`.
// It is a complete, playable, LLM-aware game that demonstrates the patterns
// every continuous-movement action game needs:
//
//   * FRACTIONAL positions with a real velocity model (cells/second),
//     integrated in updateGame(dt) - not grid-locked tile stepping.
//   * Mouse aim + projectile spawning with a fire cooldown.
//   * An enemy wave system (spawn timer, drift-toward-player AI, despawn
//     when they reach the player).
//   * One shared movement path: keyboard held-keys and the LLM actions both
//     call movePlayerBy() so human and agent control can never drift apart.
//   * initGame() CLEARS all vectors first - restart never leaks entities.
//
// Make it YOUR game: change the rules in updateGame(), the enemy AI in
// stepEnemy(), the numbers at the top, and the aim/fire feel.

#include "Engine/Core/Game2D.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

class TwinStickStarter : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 40;
    static constexpr int GRID_H = 30;
    static constexpr float PLAYER_R = 0.7f;        // collision radius (cells)
    static constexpr float PLAYER_SPEED = 13.0f;   // cells/second (keyboard)
    static constexpr float LLM_MOVE_STEP = 2.5f;   // cells per LLM action
    static constexpr float ENEMY_SPEED = 4.5f;     // cells/second
    static constexpr float FIRE_COOLDOWN = 0.22f;  // seconds between shots
    static constexpr float PROJ_SPEED = 30.0f;     // cells/second
    static constexpr float ENEMY_INTERVAL = 1.4f;  // seconds between spawns
    static constexpr float INVULN_TIME = 1.5f;     // seconds after a hit
    static constexpr int WIN_KILLS = 25;
    static constexpr int START_LIVES = 3;

    // ---- World ------------------------------------------------------------
    struct Entity {
        float x = 0.0f, y = 0.0f;      // fractional cell position (center)
        float vx = 0.0f, vy = 0.0f;    // cells/second
        bool active = true;
        SDL_Color color = {255, 255, 255, 255};
    };

    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;

    // ---- State ------------------------------------------------------------
    Entity player;
    std::vector<Entity> projectiles;
    std::vector<Entity> enemies;
    float aimX = 1.0f, aimY = 0.0f;    // aim direction (unit vector)
    float fireTimer = 0.0f;
    float enemyTimer = 0.0f;
    float invulnTimer = 0.0f;
    int kills = 0;
    int lives = START_LIVES;
    std::string statusText;            // LLM-readable message mirror

public:
    TwinStickStarter()
        : Game2D("Twin Stick Starter", GRID_W * 20, GRID_H * 20, 20) {
        // Headless smoke mode (PONG_SMOKE=1): auto-aim and auto-fire so a
        // dummy-driver run exercises the real physics paths, and the base
        // class auto-restarts when the run ends (win or out of lives) so
        // the loop never parks on a frozen game-over frame. Used by CI.
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    void initGame() override {
        // Restart (R / "restart" action) re-runs initGame - clear the
        // vectors FIRST so no entities leak across restarts.
        projectiles.clear();
        enemies.clear();
        kills = 0;
        lives = START_LIVES;
        fireTimer = 0.0f;
        enemyTimer = 0.0f;
        invulnTimer = 0.0f;
        aimX = 1.0f;
        aimY = 0.0f;

        // Arena: dark floor with a subtle grid pattern and a border.
        createGrid(GRID_W, GRID_H, tileSize);
        grid->setBorderColor({60, 60, 90, 255});
        for (int y = 0; y < GRID_H; y++) {
            for (int x = 0; x < GRID_W; x++) {
                grid->setValue(x, y, (x + y) % 2);
            }
        }
        setGridColors(0, 1, {16, 16, 26, 255}, {20, 20, 34, 255});

        player.x = static_cast<float>(GRID_W) / 2.0f;
        player.y = static_cast<float>(GRID_H) / 2.0f;
        player.vx = 0.0f;
        player.vy = 0.0f;
        player.active = true;
        player.color = {0, 230, 255, 255};

        hud = createText(10, 8, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, 30, "");
        message->setColor({255, 220, 120, 255});
        updateHUD();

        // LLM interface - the SAME code paths as the keyboard bindings.
        registerAction("move_left", [this]() { return movePlayer(-1.0f, 0.0f); });
        registerAction("move_right", [this]() { return movePlayer(1.0f, 0.0f); });
        registerAction("move_up", [this]() { return movePlayer(0.0f, -1.0f); });
        registerAction("move_down", [this]() { return movePlayer(0.0f, 1.0f); });
        registerAction("aim_left", [this]() { aimX = -1.0f; aimY = 0.0f; return ActionResult{true, "Aim left"}; });
        registerAction("aim_right", [this]() { aimX = 1.0f; aimY = 0.0f; return ActionResult{true, "Aim right"}; });
        registerAction("aim_up", [this]() { aimX = 0.0f; aimY = -1.0f; return ActionResult{true, "Aim up"}; });
        registerAction("aim_down", [this]() { aimX = 0.0f; aimY = 1.0f; return ActionResult{true, "Aim down"}; });
        registerAction("fire", [this]() { return doFire(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        // Keyboard: held-key movement feeds the same movePlayerBy() clamp.
        bindKey(KEY_R).onPress([this]() {
            if (gameOver) startGame();
        });
    }

    void updateGame(float dt) override {
        // Continuous movement, dt-scaled - same clamp the LLM uses.
        float dx = 0.0f, dy = 0.0f;
        if (input.isKeyHeld(KEY_W) || input.isKeyHeld(KEY_UP)) dy -= 1.0f;
        if (input.isKeyHeld(KEY_S) || input.isKeyHeld(KEY_DOWN)) dy += 1.0f;
        if (input.isKeyHeld(KEY_A) || input.isKeyHeld(KEY_LEFT)) dx -= 1.0f;
        if (input.isKeyHeld(KEY_D) || input.isKeyHeld(KEY_RIGHT)) dx += 1.0f;
        if (dx != 0.0f || dy != 0.0f) {
            const float len = std::sqrt(dx * dx + dy * dy);
            movePlayerBy(dx / len * PLAYER_SPEED * dt, dy / len * PLAYER_SPEED * dt);
        }

        // Mouse aim (smoke mode aims right instead - no mouse in CI).
        if (!smokeMode) {
            int mx = 0, my = 0;
            input.getMousePosition(mx, my);
            const float gx = static_cast<float>(mx) / static_cast<float>(tileSize);
            const float gy = static_cast<float>(my) / static_cast<float>(tileSize);
            const float adx = gx - player.x;
            const float ady = gy - player.y;
            if (std::abs(adx) > 0.5f || std::abs(ady) > 0.5f) {
                const float alen = std::sqrt(adx * adx + ady * ady);
                aimX = adx / alen;
                aimY = ady / alen;
            }
            if (input.isMousePressed(MOUSE_LEFT) || input.isKeyHeld(KEY_SPACE)) {
                doFire();
            }
        } else if (smokeFireTimer <= 0.0f) {
            smokeFireTimer = 0.35f;
            doFire();
        }
        if (smokeFireTimer > 0.0f) smokeFireTimer -= dt;

        // Fire cooldown ticks down regardless of input source.
        if (fireTimer > 0.0f) fireTimer -= dt;
        if (invulnTimer > 0.0f) {
            invulnTimer -= dt;
            if (invulnTimer < 0.0f) invulnTimer = 0.0f;
        }

        // Enemy waves.
        enemyTimer += dt;
        if (enemyTimer >= ENEMY_INTERVAL && static_cast<int>(enemies.size()) < 12) {
            enemyTimer = 0.0f;
            spawnEnemy();
        }
        for (auto& enemy : enemies) {
            if (enemy.active) stepEnemy(enemy, dt);
        }

        // Projectiles move; collisions kill enemies and despawn at walls.
        for (auto& p : projectiles) {
            if (!p.active) continue;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            if (p.x < 0.0f || p.x >= static_cast<float>(GRID_W) ||
                p.y < 0.0f || p.y >= static_cast<float>(GRID_H)) {
                p.active = false;
                continue;
            }
            for (auto& enemy : enemies) {
                if (!enemy.active) continue;
                const float ex = enemy.x - p.x;
                const float ey = enemy.y - p.y;
                if (ex * ex + ey * ey < 1.0f) {
                    enemy.active = false;
                    p.active = false;
                    kills++;
                    message->setText("Hit! +1");
                    break;
                }
            }
        }

        // Enemies reaching the player cost a life.
        if (invulnTimer <= 0.0f) {
            for (auto& enemy : enemies) {
                if (!enemy.active) continue;
                const float ex = enemy.x - player.x;
                const float ey = enemy.y - player.y;
                if (ex * ex + ey * ey < (PLAYER_R + 0.6f) * (PLAYER_R + 0.6f)) {
                    enemy.active = false;
                    onHit();
                    break;
                }
            }
        }

        // Win / lose.
        if (kills >= WIN_KILLS) {
            gameWon = true;
            message->setText("ARENA CLEARED! Press R to play again");
            endGame();
        }
        removeInactiveEntities();
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        SDL_Renderer* sdl = getRenderer()->renderer;

        for (const auto& p : projectiles) {
            if (p.active) drawRect(sdl, p.x, p.y, 0.45f, p.color);
        }
        for (const auto& enemy : enemies) {
            if (enemy.active) drawRect(sdl, enemy.x, enemy.y, 0.8f, enemy.color);
        }

        // Player blinks while invulnerable.
        if (invulnTimer <= 0.0f || static_cast<int>(getGameTime() * 12.0f) % 2 == 0) {
            drawRect(sdl, player.x, player.y, 0.8f, player.color);
        }

        if (gameOver) {
            const std::string over = gameWon ? "YOU WIN! Press R to play again"
                                             : "GAME OVER - Press R to retry";
            message->setText(over);
        }
        for (auto& t : textDisplays) t->render(getRenderer());
    }

    // ---- LLM state ----------------------------------------------------------
    GameState getState() const override {
        GameState state = Game2D::getState();
        state.score = kills;
        state.stats["lives"] = lives;
        state.stats["kills"] = kills;
        state.stats["kills_to_win"] = WIN_KILLS;
        state.stats["enemies"] = static_cast<int>(enemies.size());
        state.stats["aim_x"] = static_cast<int>(std::lround(aimX));
        state.stats["aim_y"] = static_cast<int>(std::lround(aimY));
        state.entities["player"] = {
            static_cast<int>(std::lround(player.x)),
            static_cast<int>(std::lround(player.y))
        };
        for (size_t i = 0; i < enemies.size(); i++) {
            state.entities["enemy_" + std::to_string(i)] = {
                static_cast<int>(std::lround(enemies[i].x)),
                static_cast<int>(std::lround(enemies[i].y))
            };
        }
        return state;
    }

private:
    // ---- One movement path, shared by input and the LLM ---------------------
    void movePlayerBy(float dx, float dy) {
        player.x = std::max(PLAYER_R,
            std::min(player.x + dx, static_cast<float>(GRID_W) - PLAYER_R));
        player.y = std::max(PLAYER_R,
            std::min(player.y + dy, static_cast<float>(GRID_H) - PLAYER_R));
    }

    ActionResult movePlayer(float dx, float dy) {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        movePlayerBy(dx * LLM_MOVE_STEP, dy * LLM_MOVE_STEP);
        result.success = true;
        result.message = "Moved to (" + std::to_string(static_cast<int>(std::lround(player.x))) +
                         ", " + std::to_string(static_cast<int>(std::lround(player.y))) + ")";
        return result;
    }

    ActionResult doFire() {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        if (fireTimer > 0.0f) {
            result.message = "Recharging";
            return result;
        }
        fireTimer = FIRE_COOLDOWN;
        Entity p;
        p.x = player.x + aimX * 0.8f;
        p.y = player.y + aimY * 0.8f;
        p.vx = aimX * PROJ_SPEED;
        p.vy = aimY * PROJ_SPEED;
        p.color = {255, 240, 140, 255};
        projectiles.push_back(p);
        result.success = true;
        result.message = "Fired";
        return result;
    }

    void spawnEnemy() {
        Entity e;
        e.color = {255, 70, 70, 255};
        // Spawn on a random edge, never on top of the player.
        const int edge = rand() % 4;
        switch (edge) {
            case 0: e.x = 1.0f;                     e.y = static_cast<float>(1 + rand() % (GRID_H - 2)); break;
            case 1: e.x = static_cast<float>(GRID_W - 2); e.y = static_cast<float>(1 + rand() % (GRID_H - 2)); break;
            case 2: e.y = 1.0f;                     e.x = static_cast<float>(1 + rand() % (GRID_W - 2)); break;
            default: e.y = static_cast<float>(GRID_H - 2); e.x = static_cast<float>(1 + rand() % (GRID_W - 2)); break;
        }
        enemies.push_back(e);
    }

    // Drift toward the player at ENEMY_SPEED.
    void stepEnemy(Entity& enemy, float dt) {
        const float dx = player.x - enemy.x;
        const float dy = player.y - enemy.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.1f) {
            enemy.x += dx / len * ENEMY_SPEED * dt;
            enemy.y += dy / len * ENEMY_SPEED * dt;
        }
    }

    void onHit() {
        lives--;
        invulnTimer = INVULN_TIME;
        // Blink-dodge: re-center the player so a crowd of enemies can't
        // instantly chain-hit them.
        player.x = static_cast<float>(GRID_W) / 2.0f;
        player.y = static_cast<float>(GRID_H) / 2.0f;
        if (lives <= 0) {
            message->setText("GAME OVER - Press R to retry");
            endGame();
        } else {
            message->setText("Ouch! -1 life");
        }
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Kills: " + std::to_string(kills) + "/" +
                     std::to_string(WIN_KILLS) +
                     "   Lives: " + std::to_string(lives) +
                     "   Enemies: " + std::to_string(enemies.size()));
    }

    void drawRect(SDL_Renderer* sdl, float cx, float cy, float halfSize, SDL_Color color) {
        const int x = static_cast<int>(std::lround((cx - halfSize) * tileSize));
        const int y = static_cast<int>(std::lround((cy - halfSize) * tileSize));
        const int s = static_cast<int>(std::lround(halfSize * 2.0f * tileSize));
        SDL_Rect rect = {x, y, s, s};
        SDL_SetRenderDrawColor(sdl, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(sdl, &rect);
    }

    // ---- Headless smoke mode (PONG_SMOKE=1): auto-aim + auto-fire --------
    float smokeFireTimer = 0.0f;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static TwinStickStarter game;
#else
    TwinStickStarter game;
#endif
    game.run();
    return 0;
}
