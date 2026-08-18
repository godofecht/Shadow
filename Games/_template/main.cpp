// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Coin Collector - the starter game.
//
// This is the canonical template every new game starts from. It is a
// complete, playable, LLM-aware game: move with arrows/WASD, grab coins,
// dodge the chasing enemies, survive four levels. See GAME_DEV_GUIDE.md
// for the anatomy walkthrough; GAMES.md for the 100-game catalog.
//
// Make it YOUR game in five edits:
//   1. Rename the class (CoinCollector -> YourGame) and the window title.
//   2. Change the rules in updateGame() (the heart of any game).
//   3. Change what renderGame() draws.
//   4. Extend registerAction() / getState() for the LLM interface.
//   5. Tune the numbers at the top of the class.
//
// Correctness notes this template demonstrates:
//   * initGame() CLEARS the entity vectors first, so restart (R key or the
//     LLM "restart" action) re-runs initGame without leaking entities.
//   * Input and LLM actions funnel through ONE method (movePlayer) so both
//     entry points can never drift apart.
//   * Text UI works everywhere (SDL_ttf when available, bitmap fallback
//     otherwise), so games render on desktop AND in the browser.

#include "Engine/Core/Game2D.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

class CoinCollector : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int GRID_W = 32;
    static constexpr int GRID_H = 24;
    static constexpr int MAX_LEVEL = 4;
    static constexpr float INVULN_TIME = 1.0f;
    static constexpr float BASE_ENEMY_INTERVAL = 1.2f;

    // ---- World ------------------------------------------------------------
    std::shared_ptr<GridEntity> player;
    std::vector<std::shared_ptr<GridEntity>> coins;
    std::vector<std::shared_ptr<GridEntity>> enemies;
    std::shared_ptr<TextDisplay> hud;      // score / level / lives
    std::shared_ptr<TextDisplay> message;  // event feedback line

    // ---- State ------------------------------------------------------------
    int score = 0;
    int lives = 3;
    int level = 1;
    int coinsCollected = 0;
    float enemyTimer = 0.0f;
    float invulnTimer = 0.0f;
    int smokeStep = 0;

public:
    CoinCollector() : Game2D("Coin Collector", 700, 540, 20) {
        // Headless smoke mode (PONG_SMOKE=1): the autopilot in updateGame()
        // chases the nearest coin, and the base class auto-restarts the
        // game when it ends, so a dummy-driver run keeps exercising the
        // full loop instead of parking on a frozen game-over frame.
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    void initGame() override {
        // Clear EVERYTHING first: restart re-runs initGame, and the entity
        // vectors must start empty or restarts would leak old entities.
        entities.clear();
        coins.clear();
        enemies.clear();
        score = 0;
        lives = 3;
        level = 1;
        coinsCollected = 0;
        enemyTimer = 0.0f;
        invulnTimer = 0.0f;

        // Board: checkerboard floor with a colored border.
        createGrid(GRID_W, GRID_H, tileSize);
        grid->setBorderColor({60, 60, 80, 255});
        for (int y = 0; y < GRID_H; y++) {
            for (int x = 0; x < GRID_W; x++) {
                grid->setValue(x, y, (x + y) % 2);
            }
        }
        setGridColors(0, 1, {20, 20, 30, 255}, {30, 30, 50, 255});

        // Player starts at the center.
        player = createEntity<GridEntity>(grid.get(), GRID_W / 2, GRID_H / 2);
        player->setColor({0, 230, 255, 255});

        // First wave.
        spawnLevel();

        // HUD: score/level/lives line plus a feedback line below it.
        hud = createText(10, 10, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, 30, "");
        message->setColor({255, 220, 120, 255});
        updateHUD();

        // LLM interface: one action per legal move + restart.
        registerAction("move_up", [this]() { return movePlayer(0, -1); });
        registerAction("move_down", [this]() { return movePlayer(0, 1); });
        registerAction("move_left", [this]() { return movePlayer(-1, 0); });
        registerAction("move_right", [this]() { return movePlayer(1, 0); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        // Human input drives the SAME code path as the LLM actions.
        bindKey(KEY_UP).onPress([this]() { movePlayer(0, -1); });
        bindKey(KEY_DOWN).onPress([this]() { movePlayer(0, 1); });
        bindKey(KEY_LEFT).onPress([this]() { movePlayer(-1, 0); });
        bindKey(KEY_RIGHT).onPress([this]() { movePlayer(1, 0); });
        bindKey(KEY_W).onPress([this]() { movePlayer(0, -1); });
        bindKey(KEY_S).onPress([this]() { movePlayer(0, 1); });
        bindKey(KEY_A).onPress([this]() { movePlayer(-1, 0); });
        bindKey(KEY_D).onPress([this]() { movePlayer(1, 0); });
        bindKey(KEY_R).onPress([this]() {
            if (gameOver) startGame();
        });
    }

    void updateGame(float dt) override {
        // Headless smoke autopilot: step toward the nearest coin (or wander
        // in a small square when the board is clear). Runs through the SAME
        // movePlayer() path as input and the LLM, so the autopilot can never
        // drift from what a human/agent does.
        if (smokeMode) {
            const GridEntity* target = nullptr;
            int bestDist = INT_MAX;
            for (const auto& coin : coins) {
                if (!coin->isActive()) continue;
                const int d = std::abs(coin->getX() - player->getX()) +
                              std::abs(coin->getY() - player->getY());
                if (d < bestDist) {
                    bestDist = d;
                    target = coin.get();
                }
            }
            if (target && bestDist > 0) {
                const int dx = target->getX() - player->getX();
                const int dy = target->getY() - player->getY();
                if (std::abs(dx) >= std::abs(dy)) {
                    movePlayer(dx > 0 ? 1 : -1, 0);
                } else {
                    movePlayer(0, dy > 0 ? 1 : -1);
                }
            } else if (bestDist == 0) {
                // On top of a coin: nudge to trigger the pickup logic.
                movePlayer(1, 0);
            } else {
                // No coins left (level cleared): wander so the loop stays
                // live until the level increments.
                smokeStep++;
                const int px = player->getX();
                const int py = player->getY();
                const int cx = GRID_W / 2;
                const int cy = GRID_H / 2;
                movePlayer(px < cx ? 1 : (px > cx ? -1 : (smokeStep % 2 == 0 ? 1 : -1)),
                           py < cy ? 1 : (py > cy ? -1 : 0));
            }
        }

        // Enemies step toward the player on a level-tuned timer.
        enemyTimer += dt;
        const float interval = BASE_ENEMY_INTERVAL / (1.0f + 0.25f * static_cast<float>(level - 1));
        if (enemyTimer >= interval) {
            enemyTimer = 0.0f;
            for (auto& enemy : enemies) {
                if (enemy->isActive()) stepEnemy(enemy.get());
            }
            if (isPlayerCaught()) onHit();
        }

        // Invulnerability wears off.
        if (invulnTimer > 0.0f) {
            invulnTimer -= dt;
            if (invulnTimer < 0.0f) invulnTimer = 0.0f;
        }

        // Win condition: clear every level.
        if (coins.empty() && level >= MAX_LEVEL) {
            gameWon = true;
            endGame();
        }
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        // Coins (yellow), enemies (red) - plain rects, no assets needed.
        for (const auto& coin : coins) {
            if (coin->isActive()) coin->render(getRenderer());
        }
        for (const auto& enemy : enemies) {
            if (enemy->isActive()) enemy->render(getRenderer());
        }

        // Player blinks while invulnerable (alpha is ignored unless
        // blending is enabled, so skip the frame to blink).
        if (invulnTimer <= 0.0f || static_cast<int>(getGameTime() * 12.0f) % 2 == 0) {
            if (player) player->render(getRenderer());
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
        state.score = score;
        state.level = level;
        state.stats["lives"] = lives;
        state.stats["coins_remaining"] = static_cast<int>(coins.size());
        state.stats["coins_collected"] = coinsCollected;
        if (player) {
            state.entities["player"] = {player->getX(), player->getY()};
        }
        for (size_t i = 0; i < enemies.size(); i++) {
            state.entities["enemy_" + std::to_string(i)] = {enemies[i]->getX(), enemies[i]->getY()};
        }
        return state;
    }

private:
    // ---- One move, shared by input and the LLM ------------------------------
    ActionResult movePlayer(int dx, int dy) {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        if (invulnTimer > 0.0f) {
            result.message = "Invulnerable";
            return result;
        }
        const int nx = player->getX() + dx;
        const int ny = player->getY() + dy;
        if (!grid->isInBounds(nx, ny)) {
            result.message = "Cannot move off the board";
            return result;
        }
        player->setPosition(nx, ny);
        result.success = true;
        result.message = "Moved to (" + std::to_string(nx) + ", " + std::to_string(ny) + ")";

        // Coin pickup.
        for (auto& coin : coins) {
            if (coin->isActive() && coin->getX() == nx && coin->getY() == ny) {
                coin->setActive(false);
                score += 10;
                coinsCollected++;
                result.scoreChange = 10;
                result.message += " - coin! (+10)";
                message->setText("Coin! +10");
                if (coins.empty()) {
                    level++;
                    message->setText("Level " + std::to_string(level) + "!");
                    spawnLevel();
                }
                break;
            }
        }

        // Ran into an enemy?
        if (isPlayerCaught()) {
            onHit();
            result.success = false;
        }
        updateHUD();
        return result;
    }

    void spawnLevel() {
        // Coins everywhere but the player's cell.
        const int coinCount = 6 + level * 2;
        for (int i = 0; i < coinCount; i++) {
            const int x = rand() % GRID_W;
            const int y = rand() % GRID_H;
            if (player && x == player->getX() && y == player->getY()) {
                i--;
                continue;
            }
            auto coin = createEntity<GridEntity>(grid.get(), x, y);
            coin->setColor({255, 210, 0, 255});
            coins.push_back(coin);
        }
        // Enemies: keep the first one far from the player's spawn corner.
        const int enemyCount = 2 + level;
        for (int i = 0; i < enemyCount; i++) {
            int x, y;
            do {
                x = rand() % GRID_W;
                y = rand() % GRID_H;
            } while (player &&
                     std::abs(x - player->getX()) + std::abs(y - player->getY()) < 8);
            auto enemy = createEntity<GridEntity>(grid.get(), x, y);
            enemy->setColor({255, 60, 60, 255});
            enemies.push_back(enemy);
        }
    }

    // One greedy step toward the player on the dominant axis.
    void stepEnemy(GridEntity* enemy) {
        const int dx = player->getX() - enemy->getX();
        const int dy = player->getY() - enemy->getY();
        const int nx = enemy->getX() + (std::abs(dx) >= std::abs(dy) ? (dx > 0 ? 1 : -1) : 0);
        const int ny = enemy->getY() + (std::abs(dx) >= std::abs(dy) ? 0 : (dy > 0 ? 1 : -1));
        if (grid->isInBounds(nx, ny)) {
            enemy->setPosition(nx, ny);
            return;
        }
        // Dominant axis blocked - try the other one.
        const int nx2 = enemy->getX() + (dx > 0 ? 1 : -1);
        const int ny2 = enemy->getY() + (dy > 0 ? 1 : -1);
        if (grid->isInBounds(nx2, enemy->getY())) {
            enemy->setPosition(nx2, enemy->getY());
        } else if (grid->isInBounds(enemy->getX(), ny2)) {
            enemy->setPosition(enemy->getX(), ny2);
        }
    }

    bool isPlayerCaught() const {
        for (const auto& enemy : enemies) {
            if (enemy->isActive() && enemy->getX() == player->getX() && enemy->getY() == player->getY()) {
                return true;
            }
        }
        return false;
    }

    void onHit() {
        lives--;
        invulnTimer = INVULN_TIME;
        player->setPosition(GRID_W / 2, GRID_H / 2);
        if (lives <= 0) {
            endGame();
        } else {
            message->setText("Ouch! -1 life");
        }
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Score: " + std::to_string(score) +
                     "   Level: " + std::to_string(level) + "/" + std::to_string(MAX_LEVEL) +
                     "   Lives: " + std::to_string(lives) +
                     "   Coins: " + std::to_string(coins.size()));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the CoinCollector class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static CoinCollector game;
#else
    CoinCollector game;
#endif
    game.run();
    return 0;
}
#endif
