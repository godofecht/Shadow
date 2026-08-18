#pragma once

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

enum class RoguelikePhase {
    Playing,
    Dead,
    Won
};

struct RoguelikeEnemyState {
    int x = 0;
    int y = 0;
    int health = 0;
    int attack = 0;
    bool alive = true;
};

struct RoguelikePlayerState {
    int x = 0;
    int y = 0;
    int health = 100;
    int maxHealth = 100;
    int attack = 10;
};

struct RoguelikeTurnResult {
    bool blocked = false;
    bool moved = false;
    bool fought = false;
    bool collectedGold = false;
    bool descended = false;
    bool died = false;
    bool won = false;
    int enemyHitIndex = -1;
    int damageToEnemy = 0;
    int damageToPlayer = 0;
};

class RoguelikeState {
public:
    RoguelikeState() = default;

    void startRun(int startLevel, int _maxLevel, int playerHp, int playerAtk) {
        level = std::max(1, startLevel);
        this->maxLevel = std::max(level, _maxLevel);
        goldCount = 0;
        turns = 0;
        phase = RoguelikePhase::Playing;
        message = "Welcome to the dungeon!";
        player.health = playerHp;
        player.maxHealth = playerHp;
        player.attack = playerAtk;
    }

    void startLevelMap(int width, int height) {
        w = width;
        h = height;
        solid.assign((size_t)w * (size_t)h, true);
        enemies.clear();
        goldCells.clear();
        stairs = {-1, -1};
    }

    bool inBounds(int x, int y) const {
        return x >= 0 && x < w && y >= 0 && y < h;
    }

    void setSolid(int x, int y, bool isSolid) {
        if (!inBounds(x, y)) return;
        solid[(size_t)y * (size_t)w + (size_t)x] = isSolid;
    }

    bool isSolid(int x, int y) const {
        if (!inBounds(x, y)) return true;
        return solid[(size_t)y * (size_t)w + (size_t)x];
    }

    void setPlayerPos(int x, int y) {
        player.x = x;
        player.y = y;
    }

    void setStairs(int x, int y) {
        stairs = {x, y};
    }

    void addGold(int x, int y) {
        goldCells.insert(encode(x, y));
    }

    void addEnemy(int x, int y, int hp, int atk) {
        enemies.push_back({x, y, hp, atk, true});
    }

    RoguelikeTurnResult playerStep(int dx, int dy, uint32_t combatRoll) {
        RoguelikeTurnResult result;
        if (phase != RoguelikePhase::Playing) return result;

        turns++;
        int nx = player.x + dx;
        int ny = player.y + dy;
        if (!inBounds(nx, ny) || isSolid(nx, ny)) {
            result.blocked = true;
            message = "Blocked by wall.";
            return result;
        }

        int enemyIndex = enemyAt(nx, ny);
        if (enemyIndex >= 0) {
            auto& enemy = enemies[(size_t)enemyIndex];
            result.fought = true;
            result.enemyHitIndex = enemyIndex;

            int dmg = player.attack + (int)(combatRoll % 5);
            enemy.health -= dmg;
            result.damageToEnemy = dmg;

            if (enemy.health <= 0) {
                enemy.alive = false;
                message = "Enemy defeated!";
            } else {
                int retaliation = enemy.attack;
                player.health -= retaliation;
                result.damageToPlayer = retaliation;
                if (player.health <= 0) {
                    player.health = 0;
                    phase = RoguelikePhase::Dead;
                    result.died = true;
                    message = "You died!";
                } else {
                    message = "You hit for " + std::to_string(dmg) + ", enemy hits back for " + std::to_string(retaliation) + ".";
                }
            }
            return result;
        }

        player.x = nx;
        player.y = ny;
        result.moved = true;

        int key = encode(nx, ny);
        if (goldCells.erase(key) > 0) {
            goldCount += 10;
            result.collectedGold = true;
            message = "Found gold! Total: " + std::to_string(goldCount);
        }

        if (nx == stairs.first && ny == stairs.second) {
            if (level >= maxLevel) {
                phase = RoguelikePhase::Won;
                result.won = true;
                message = "You escaped the dungeon!";
            } else {
                level++;
                result.descended = true;
                message = "Descended to level " + std::to_string(level) + ".";
            }
        }

        return result;
    }

    void enemyStep() {
        if (phase != RoguelikePhase::Playing) return;

        for (size_t i = 0; i < enemies.size(); ++i) {
            auto& e = enemies[i];
            if (!e.alive) continue;

            int dx = 0;
            int dy = 0;
            if (player.x > e.x) dx = 1;
            else if (player.x < e.x) dx = -1;
            else if (player.y > e.y) dy = 1;
            else if (player.y < e.y) dy = -1;

            int nx = e.x + dx;
            int ny = e.y + dy;
            if (!inBounds(nx, ny) || isSolid(nx, ny)) continue;
            if (nx == player.x && ny == player.y) continue;
            if (enemyAt(nx, ny, (int)i) >= 0) continue;
            e.x = nx;
            e.y = ny;
        }
    }

    bool goldAt(int x, int y) const {
        return goldCells.find(encode(x, y)) != goldCells.end();
    }

    // True when a live enemy occupies (x, y). Used by the BFS autopilot to
    // prefer paths that avoid fighting.
    bool hasEnemyAt(int x, int y) const { return enemyAt(x, y) >= 0; }

    std::vector<std::pair<int, int>> goldPositions() const {
        std::vector<std::pair<int, int>> out;
        out.reserve(goldCells.size());
        for (int key : goldCells) {
            out.emplace_back(key % w, key / w);
        }
        return out;
    }

    const RoguelikePlayerState& getPlayer() const { return player; }
    const std::vector<RoguelikeEnemyState>& getEnemies() const { return enemies; }
    RoguelikePhase getPhase() const { return phase; }
    int getLevel() const { return level; }
    int getGoldCount() const { return goldCount; }
    int getTurns() const { return turns; }
    const std::string& getMessage() const { return message; }
    std::pair<int, int> getStairs() const { return stairs; }

private:
    int w = 0;
    int h = 0;
    int level = 1;
    int maxLevel = 5;
    int goldCount = 0;
    int turns = 0;
    RoguelikePhase phase = RoguelikePhase::Playing;
    std::string message;
    RoguelikePlayerState player{};
    std::vector<RoguelikeEnemyState> enemies;
    std::set<int> goldCells;
    std::pair<int, int> stairs{-1, -1};
    std::vector<bool> solid;

    int encode(int x, int y) const {
        return y * w + x;
    }

    int enemyAt(int x, int y, int skipIndex = -1) const {
        for (size_t i = 0; i < enemies.size(); ++i) {
            if ((int)i == skipIndex) continue;
            const auto& e = enemies[i];
            if (!e.alive) continue;
            if (e.x == x && e.y == y) return (int)i;
        }
        return -1;
    }
};
