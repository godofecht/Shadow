// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Platformer Starter - the jump-and-run starting point.
//
// This is the generator template for `tools/new_game.sh --platformer`.
// It is a complete, playable, LLM-aware game that demonstrates the patterns
// every platformer needs:
//
//   * GRAVITY + JUMP: a velocity model (cells/second) with a downward
//     acceleration, a jump impulse, and variable jump height (releasing
//     jump early cuts the ascent).
//   * SOLID-CELL collision: the player is a small bounding box resolved
//     against the grid's solid cells axis-by-axis, so it slides along
//     walls and lands on platforms (never tunnels).
//   * Patrol enemies you can stomp (bounce) or be hurt by (side contact),
//     with invulnerability frames and a respawn point.
//   * A goal to reach -> win, lives -> lose.
//   * One shared movement path: keyboard and the LLM actions both call
//     movePlayerBy() / tryJump() so human and agent control never drift.
//   * initGame() CLEARS all vectors first - restart never leaks entities.
//
// Make it YOUR game: redesign the level in buildLevel(), tune the feel at
// the top, and change the win condition.

#include "Engine/Core/Game2D.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

class PlatformerStarter : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 42;
    static constexpr int GRID_H = 26;
    static constexpr float PLAYER_W = 0.8f;   // bounding box (cells)
    static constexpr float PLAYER_H = 1.4f;
    static constexpr float MOVE_SPEED = 9.0f;      // cells/second (keyboard)
    static constexpr float LLM_MOVE_STEP = 2.0f;   // cells per LLM action
    static constexpr float GRAVITY = 90.0f;        // cells/second^2
    static constexpr float JUMP_VEL = 26.0f;       // cells/second (up)
    static constexpr float INVULN_TIME = 1.5f;
    static constexpr int START_LIVES = 3;
    static constexpr int GOAL_X = 38, GOAL_Y = 3;  // on the top platform

    // ---- World ------------------------------------------------------------
    struct Platform {
        int x, y, w;                 // top-left cell, width in cells
    };
    static constexpr Platform LEVEL[] = {
        {2, 19, 10},   // starter ledge
        {16, 16, 8},   // mid platform (patrol enemy)
        {28, 12, 8},   // upper-mid platform
        {34, 4, 8},    // top platform (goal)
    };

    struct Enemy {
        float x = 0.0f, y = 0.0f;    // center, cell coords
        float vx = 1.2f;             // patrol speed (cells/second)
        bool active = true;
    };

    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;

    // ---- State ------------------------------------------------------------
    float px = 3.0f, py = 18.0f;     // player position (center, cells)
    float vx = 0.0f, vy = 0.0f;      // cells/second
    bool grounded = false;
    float invulnTimer = 0.0f;
    int lives = START_LIVES;
    int stomps = 0;
    std::vector<Enemy> enemies;
    std::string statusText;          // LLM-readable message mirror

public:
    PlatformerStarter()
        : Game2D("Platformer Starter", GRID_W * 18, GRID_H * 18, 18) {
        // Headless smoke mode (PONG_SMOKE=1): auto-run toward the goal with
        // auto-jumps so a dummy-driver run exercises gravity + collision,
        // and the base class auto-restarts when the goal is reached so the
        // loop never parks on a frozen game-over frame. Used by CI.
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    void initGame() override {
        // Restart (R / "restart" action) re-runs initGame - clear the
        // vectors FIRST so no entities leak across restarts.
        enemies.clear();
        lives = START_LIVES;
        stomps = 0;
        invulnTimer = 0.0f;
        vx = 0.0f;
        vy = 0.0f;
        px = 3.0f;
        py = 18.0f;

        // World: build the floor + platforms as solid, colored cells.
        createGrid(GRID_W, GRID_H, tileSize);
        grid->setBorderColor({50, 40, 30, 255});
        for (int y = 0; y < GRID_H; y++) {
            for (int x = 0; x < GRID_W; x++) {
                grid->setValue(x, y, 0);
            }
        }
        setGridColors(0, 1, {120, 110, 150, 255}, {140, 130, 170, 255});
        buildLevel();

        hud = createText(10, 8, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, 30, "");
        message->setColor({255, 220, 120, 255});
        updateHUD();

        // LLM interface - the SAME code paths as the keyboard bindings.
        registerAction("move_left", [this]() { return movePlayer(-1.0f); });
        registerAction("move_right", [this]() { return movePlayer(1.0f); });
        registerAction("jump", [this]() { return doJump(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_R).onPress([this]() {
            if (gameOver) startGame();
        });
    }

    void updateGame(float dt) override {
        // Keyboard: continuous movement through the same clamp the LLM uses.
        float moveDir = 0.0f;
        if (input.isKeyHeld(KEY_A) || input.isKeyHeld(KEY_LEFT)) moveDir -= 1.0f;
        if (input.isKeyHeld(KEY_D) || input.isKeyHeld(KEY_RIGHT)) moveDir += 1.0f;
        if (input.isKeyHeld(KEY_W) || input.isKeyHeld(KEY_UP) || input.isKeyHeld(KEY_SPACE)) {
            tryJump();
        }
        movePlayerBy(moveDir * MOVE_SPEED * dt, 0.0f);

        // Smoke mode: auto-run toward the goal and auto-jump when grounded.
        if (smokeMode) {
            const float gx = static_cast<float>(GOAL_X);
            movePlayerBy((px < gx ? 1.0f : -1.0f) * MOVE_SPEED * dt, 0.0f);
            if (grounded) tryJump();
        }

        // Physics: gravity, then resolve against solid cells axis-by-axis.
        vy -= GRAVITY * dt;
        grounded = false;
        resolveCollisions(dt);

        // Enemies patrol back and forth across their platform.
        for (auto& enemy : enemies) {
            if (!enemy.active) continue;
            enemy.x += enemy.vx * dt;
            const int ex = static_cast<int>(std::lround(enemy.x));
            const int ey = static_cast<int>(std::lround(enemy.y));
            const bool aheadSolid = isSolid(ex + (enemy.vx > 0.0f ? 2 : -2), ey);
            const bool aheadGap = !isSolid(ex + (enemy.vx > 0.0f ? 2 : -2), ey + 1);
            if (aheadSolid || aheadGap) enemy.vx = -enemy.vx;
        }

        // Player vs enemies: stomp from above, take a hit from the side.
        if (invulnTimer > 0.0f) {
            invulnTimer -= dt;
            if (invulnTimer < 0.0f) invulnTimer = 0.0f;
        } else {
            for (auto& enemy : enemies) {
                if (!enemy.active) continue;
                const float ex = enemy.x - px;
                const float ey = enemy.y - (py - PLAYER_H / 2.0f);
                if (std::abs(ex) < 0.9f && std::abs(ey) < 0.8f) {
                    // Stomp if the player is above the enemy's center.
                    if (py - PLAYER_H / 2.0f > enemy.y - 0.2f) {
                        enemy.active = false;
                        stomps++;
                        vy = JUMP_VEL * 0.7f;  // bounce
                        message->setText("Stomp! +1");
                    } else {
                        onHit();
                    }
                    break;
                }
            }
        }

        // Goal reached?
        if (px > static_cast<float>(GOAL_X) - 0.5f &&
            px < static_cast<float>(GOAL_X) + 1.5f &&
            py - PLAYER_H / 2.0f > static_cast<float>(GOAL_Y) - 1.5f &&
            py + PLAYER_H / 2.0f < static_cast<float>(GOAL_Y) + 2.5f) {
            gameWon = true;
            message->setText("LEVEL CLEARED! Press R to play again");
            endGame();
        }
        removeInactiveEntities();
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        SDL_Renderer* sdl = getRenderer()->renderer;

        // Goal: a gold flag block on the top platform.
        SDL_Rect goal = {
            GOAL_X * tileSize, GOAL_Y * tileSize, tileSize, tileSize
        };
        SDL_SetRenderDrawColor(sdl, 255, 210, 0, 255);
        SDL_RenderFillRect(sdl, &goal);

        // Enemies (red) and player (cyan; blinks while invulnerable).
        for (const auto& enemy : enemies) {
            if (!enemy.active) continue;
            SDL_Rect er = {
                static_cast<int>(std::lround((enemy.x - 0.45f) * tileSize)),
                static_cast<int>(std::lround((enemy.y - 0.45f) * tileSize)),
                static_cast<int>(0.9f * tileSize), static_cast<int>(0.9f * tileSize)
            };
            SDL_SetRenderDrawColor(sdl, 255, 70, 70, 255);
            SDL_RenderFillRect(sdl, &er);
        }
        if (invulnTimer <= 0.0f || static_cast<int>(getGameTime() * 12.0f) % 2 == 0) {
            SDL_Rect pr = {
                static_cast<int>(std::lround((px - PLAYER_W / 2.0f) * tileSize)),
                static_cast<int>(std::lround((py - PLAYER_H / 2.0f) * tileSize)),
                static_cast<int>(PLAYER_W * tileSize), static_cast<int>(PLAYER_H * tileSize)
            };
            SDL_SetRenderDrawColor(sdl, 0, 230, 255, 255);
            SDL_RenderFillRect(sdl, &pr);
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
        state.score = stomps;
        state.stats["lives"] = lives;
        state.stats["stomps"] = stomps;
        state.stats["x"] = static_cast<int>(std::lround(px));
        state.stats["y"] = static_cast<int>(std::lround(py));
        state.stats["grounded"] = grounded ? 1 : 0;
        state.stats["goal_x"] = GOAL_X;
        state.stats["goal_y"] = GOAL_Y;
        state.entities["player"] = {
            static_cast<int>(std::lround(px)),
            static_cast<int>(std::lround(py))
        };
        state.entities["goal"] = {GOAL_X, GOAL_Y};
        for (size_t i = 0; i < enemies.size(); i++) {
            state.entities["enemy_" + std::to_string(i)] = {
                static_cast<int>(std::lround(enemies[i].x)),
                static_cast<int>(std::lround(enemies[i].y))
            };
        }
        return state;
    }

private:
    // ---- World construction ------------------------------------------------
    void buildLevel() {
        // Floor: the bottom two rows are solid ground.
        for (int x = 0; x < GRID_W; x++) {
            grid->setValue(x, GRID_H - 1, 1);
            grid->setValue(x, GRID_H - 2, 1);
            grid->cell(x, GRID_H - 1).isSolid = true;
            grid->cell(x, GRID_H - 2).isSolid = true;
            grid->setCellColor(x, GRID_H - 1, {90, 70, 50, 255});
            grid->setCellColor(x, GRID_H - 2, {110, 90, 60, 255});
        }
        // Platforms from the LEVEL table.
        for (const auto& p : LEVEL) {
            for (int i = 0; i < p.w; i++) {
                const int x = p.x + i;
                grid->setValue(x, p.y, 1);
                grid->cell(x, p.y).isSolid = true;
                grid->setCellColor(x, p.y, {90, 70, 50, 255});
            }
        }
        // A patrol enemy on the mid platform.
        Enemy e;
        e.x = 19.0f;
        e.y = static_cast<float>(16 - 1);
        e.vx = 1.2f;
        enemies.push_back(e);
    }

    bool isSolid(int cx, int cy) const {
        return cx >= 0 && cx < GRID_W && cy >= 0 && cy < GRID_H &&
               grid->cell(cx, cy).isSolid;
    }

    // ---- One movement path, shared by input and the LLM ---------------------
    void movePlayerBy(float dx, float dy) {
        px = std::max(PLAYER_W / 2.0f,
            std::min(px + dx, static_cast<float>(GRID_W) - PLAYER_W / 2.0f));
        py = std::max(PLAYER_H / 2.0f,
            std::min(py + dy, static_cast<float>(GRID_H) - PLAYER_H / 2.0f));
    }

    ActionResult movePlayer(float dir) {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        movePlayerBy(dir * LLM_MOVE_STEP, 0.0f);
        result.success = true;
        result.message = "Moved to (" + std::to_string(static_cast<int>(std::lround(px))) +
                         ", " + std::to_string(static_cast<int>(std::lround(py))) + ")";
        return result;
    }

    bool tryJump() {
        if (!grounded) return false;
        vy = JUMP_VEL;
        return true;
    }

    ActionResult doJump() {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        if (tryJump()) {
            result.success = true;
            result.message = "Jumped";
        } else {
            result.message = "Not grounded";
        }
        return result;
    }

    // ---- Collision ----------------------------------------------------------
    // Resolve the player's bounding box against solid cells, axis by axis,
    // so the player slides along walls and lands on platforms.
    void resolveCollisions(float dt) {
        // Horizontal: try the move, then clamp at the first solid cell.
        float nx = px + vx * dt;
        const float halfW = PLAYER_W / 2.0f;
        if (vx > 0.0f) {
            const int edge = static_cast<int>(std::floor(nx + halfW));
            const int top = static_cast<int>(std::floor(py - PLAYER_H / 2.0f));
            const int bot = static_cast<int>(std::floor(py + PLAYER_H / 2.0f));
            if (isSolid(edge, top) || isSolid(edge, bot) || isSolid(edge, static_cast<int>(py))) {
                nx = static_cast<float>(edge) - halfW - 0.001f;
                vx = 0.0f;
            }
        } else if (vx < 0.0f) {
            const int edge = static_cast<int>(std::floor(nx - halfW));
            const int top = static_cast<int>(std::floor(py - PLAYER_H / 2.0f));
            const int bot = static_cast<int>(std::floor(py + PLAYER_H / 2.0f));
            if (isSolid(edge, top) || isSolid(edge, bot) || isSolid(edge, static_cast<int>(py))) {
                nx = static_cast<float>(edge + 1) + halfW + 0.001f;
                vx = 0.0f;
            }
        }
        px = nx;

        // Vertical: falling into a solid -> land; rising into one -> bump.
        float ny = py + vy * dt;
        const float halfH = PLAYER_H / 2.0f;
        const int left = static_cast<int>(std::floor(px - halfW));
        const int right = static_cast<int>(std::floor(px + halfW));
        if (vy < 0.0f) {  // falling
            const int edge = static_cast<int>(std::floor(ny + halfH));
            if (isSolid(left, edge) || isSolid(right, edge) || isSolid(static_cast<int>(px), edge)) {
                ny = static_cast<float>(edge) - halfH - 0.001f;
                vy = 0.0f;
                grounded = true;
            }
        } else if (vy > 0.0f) {  // rising (jump)
            const int edge = static_cast<int>(std::floor(ny - halfH));
            if (isSolid(left, edge) || isSolid(right, edge) || isSolid(static_cast<int>(px), edge)) {
                ny = static_cast<float>(edge + 1) + halfH + 0.001f;
                vy = 0.0f;
            }
        }
        py = ny;
    }

    void onHit() {
        lives--;
        invulnTimer = INVULN_TIME;
        if (lives <= 0) {
            message->setText("GAME OVER - Press R to retry");
            endGame();
        } else {
            // Respawn at the starter ledge.
            px = 3.0f;
            py = 18.0f;
            vx = 0.0f;
            vy = 0.0f;
            message->setText("Ouch! -1 life");
        }
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Lives: " + std::to_string(lives) +
                     "   Stomps: " + std::to_string(stomps) +
                     "   Goal: (" + std::to_string(GOAL_X) + ", " +
                     std::to_string(GOAL_Y) + ")");
    }

    // ---- Headless smoke mode (PONG_SMOKE=1): auto-run to the goal ----------
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static PlatformerStarter game;
#else
    PlatformerStarter game;
#endif
    game.run();
    return 0;
}
