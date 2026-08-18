// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Cinderfall - pure simulation state (no SDL). Every rule of the game lives
// here so it is unit-testable and can run headless under the LLM interface,
// the native sanitizers, and the Emscripten/Node path. The renderer
// (Games/Cinderfall/main.cpp) only reads this state and draws it; it never
// mutates gameplay.
//
// Coordinates are in *tiles*: (0,0) is the top-left cell, and fractional
// positions put an entity's center within a tile (e.g. 2.5 is the middle of
// cell x=2). The world is a fixed grid; walls and locked doors are solid.
//
// The run seed + floor number deterministically regenerate every floor, so
// serialize()/load() only persist the run meta (seed, floor, HP, gold, keys)
// and the current floor is rebuilt identically on load.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace cinderfall {

enum class Phase { Title, Playing, Dead, Won };

enum class EnemyKind { Chaser = 0, Spitter = 1, Brute = 2, Ghost = 3, Turret = 4 };

enum class PickupKind { Gold = 0, Heart = 1, Key = 2 };

// Short gameplay sound effects, emitted as events the renderer/shell turns
// into actual audio (procedural, no asset files). The simulation only ever
// records *that* something happened - sound is a rendering concern.
enum class Sfx { Swing, Hit, Kill, Pickup, Hurt, Door, Descend, Win };

// Particle kinds used by the pure-sim particle system.
enum class ParticleKind { Ember, Blood, Gold };

// Tile kinds stored in the world grid.
enum Tile : int { Floor = 0, Wall = 1, DoorTile = 2 };

// Wish/command input for one step. moveX/moveY is a unit-ish direction
// (it is normalized inside step()).
struct Input {
    float moveX = 0.0f;
    float moveY = 0.0f;
    bool attack = false;
    bool roll = false;
    bool interact = false;
};

struct Enemy {
    EnemyKind kind = EnemyKind::Chaser;
    float x = 0.0f;
    float y = 0.0f;
    int hp = 1;
    int maxHp = 1;
    float fireTimer = 1.5f;    // spitters: shot cadence; turrets: volley cooldown
    float volleyTimer = 0.0f;  // turret: time until next shot inside a volley
    int volleyShots = 0;       // turret: shots remaining in the current volley
    float hitFlash = 0.0f;
    bool alive = true;
};

struct Projectile {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    int damage = 1;
    bool alive = true;
};

struct Particle {
    ParticleKind kind = ParticleKind::Ember;
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float life = 0.0f;
    float maxLife = 0.0f;
    int size = 2;
};

struct Pickup {
    PickupKind kind = PickupKind::Gold;
    int x = 0;
    int y = 0;
    bool taken = false;
};

struct Chest {
    int x = 0;
    int y = 0;
    bool opened = false;
    PickupKind contents = PickupKind::Gold;
};

struct Door {
    int x = 0;
    int y = 0;
    bool open = false;
};

struct Player {
    float x = 0.0f;
    float y = 0.0f;
    float facingX = 1.0f;
    float facingY = 0.0f;
    int hp = 6;
    int maxHp = 6;
    int gold = 0;
    int keys = 0;
    float attackCooldown = 0.0f;
    float attackTimer = 0.0f;
    float rollTimer = 0.0f;
    float rollCooldown = 0.0f;
    float rollX = 1.0f;
    float rollY = 0.0f;
    float invulnTimer = 0.0f;
    float hitFlash = 0.0f;
};

class State {
public:
    // Tuning constants (public so the renderer, tests and the GDD can refer
    // to the same numbers).
    static constexpr float kPlayerSpeed = 6.0f;     // tiles / second
    static constexpr float kRollSpeed = 15.0f;
    static constexpr float kRollDuration = 0.18f;   // seconds of i-frames
    static constexpr float kRollCooldown = 0.45f;
    static constexpr float kAttackRange = 1.7f;     // tiles
    static constexpr float kAttackDuration = 0.20f;
    static constexpr float kAttackCooldown = 0.32f;
    static constexpr float kInvulnAfterHit = 0.6f;
    static constexpr float kPlayerRadius = 0.32f;   // collision half-extent
    static constexpr int kBaseDamage = 2;
    static constexpr int kDefaultMaxHp = 6;
    static constexpr int kMaxFloors = 3;

    // M1 game-feel tuning.
    static constexpr float kHitStopHit = 0.05f;    // freeze on a connecting swing
    static constexpr float kHitStopKill = 0.09f;   // freeze on a kill
    static constexpr float kHitStopHurt = 0.12f;   // freeze on taking damage
    static constexpr float kTraumaDecay = 1.6f;    // screen-shake trauma per second
    static constexpr int kMaxParticles = 400;      // hard cap (headless safety)
    static constexpr int kMaxSfxQueue = 24;        // bounded event queue

    // M2 bestiary tuning.
    static constexpr float kGhostSpeed = 2.0f;     // phases through walls
    static constexpr float kTurretVolleyCooldown = 2.5f;  // between volleys
    static constexpr int kTurretVolleySize = 3;    // shots per volley
    static constexpr float kTurretShotSpacing = 0.12f;    // seconds between shots
    static constexpr float kTurretProjSpeed = 3.0f;
    static constexpr float kTurretRange = 8.0f;    // won't fire beyond this

    // ----- lifecycle -----
    void startTitle() { phase_ = Phase::Title; setMessage(""); }
    void newRun(uint32_t seed);
    void step(float dt, const Input& in);

    // ----- world queries -----
    int width() const { return w; }
    int height() const { return h; }
    bool inBounds(int x, int y) const { return x >= 0 && x < w && y >= 0 && y < h; }
    int tileAt(int x, int y) const { return inBounds(x, y) ? tiles_[(size_t)y * (size_t)w + (size_t)x] : (int)Wall; }
    bool isWall(int x, int y) const { return tileAt(x, y) == (int)Wall; }
    bool isDoor(int x, int y) const { return tileAt(x, y) == (int)DoorTile; }
    bool isSolid(int x, int y) const { return !inBounds(x, y) || tileAt(x, y) != (int)Floor; }

    // ----- interaction (level editor / LLM / tests) -----
    bool openDoor(int x, int y);   // consumes a key; returns success
    bool openChest(int x, int y);  // collects contents; returns success

    // ----- scenario construction (tests + future level editor) -----
    // Resets to an all-walkable, entity-free floor with a fresh player, so
    // callers can lay out an exact arena deterministically.
    void resetToEmpty(int width, int height);
    void setSolid(int x, int y, bool solid);
    void setPlayerPos(float x, float y) { p.x = x; p.y = y; }
    void setFacing(float x, float y) { p.facingX = x; p.facingY = y; }
    void setKeys(int n) { p.keys = n; }
    void setStairs(int x, int y) { stairs_ = {x, y}; }
    void setFloor(int n) { floorNum = n; }
    void setMaxFloors(int n) { maxFloor = n; }
    void addEnemy(EnemyKind kind, int x, int y, int hp);
    void initVolley(Enemy& e) const;
    void addPickup(PickupKind kind, int x, int y);
    void addChest(int x, int y, PickupKind contents);
    void addDoor(int x, int y);

    // ----- accessors -----
    const Player& player() const { return p; }
    const std::vector<Enemy>& enemies() const { return enemies_; }
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    const std::vector<Pickup>& pickups() const { return pickups_; }
    const std::vector<Chest>& chests() const { return chests_; }
    const std::vector<Door>& doors() const { return doors_; }
    std::pair<int, int> stairs() const { return stairs_; }
    Phase phase() const { return phase_; }
    int floor() const { return floorNum; }
    int maxFloors() const { return maxFloor; }
    uint32_t seed() const { return seed_; }
    const std::string& message() const { return message_; }

    // ----- M1 game feel -----
    float hitStopTimer() const { return hitStopTimer_; }
    float trauma() const { return trauma_; }
    const std::vector<Particle>& particles() const { return particles_; }
    // Drains (and clears) the queued sound events since the last call.
    std::vector<Sfx> takeSfx();

    // ----- save/load (between floors) -----
    std::string serialize() const;
    bool load(const std::string& data);

private:
    void generateFloor();
    void moveWithCollision(float dx, float dy);
    bool canOccupy(float fx, float fy) const;
    void tryAttack();
    void stepEnemies(float dt);
    void stepProjectiles(float dt);
    void collectPickups();
    void interactAdjacent();
    void hurtPlayer(int amount);
    int goldFor(EnemyKind kind) const;
    Enemy makeEnemy(EnemyKind kind, int x, int y, int floor) const;
    uint32_t nextRand();
    int randInt(int lo, int hi);
    void setMessage(const std::string& m) { message_ = m; }

    // ----- M1 game-feel helpers -----
    void resetFeelState();
    void bumpTrauma(float amount);
    void triggerHitStop(float seconds);
    void pushSfx(Sfx s);
    void spawnBurst(ParticleKind kind, float x, float y, int count, float speed);
    void stepParticles(float dt);

    int w = 0;
    int h = 0;
    std::vector<int> tiles_;
    uint32_t seed_ = 0;
    uint32_t rng_ = 0;
    int floorNum = 1;
    int maxFloor = kMaxFloors;
    Phase phase_ = Phase::Title;
    Player p;
    std::vector<Enemy> enemies_;
    std::vector<Projectile> projectiles_;
    std::vector<Pickup> pickups_;
    std::vector<Chest> chests_;
    std::vector<Door> doors_;
    std::pair<int, int> stairs_{-1, -1};
    std::string message_;

    // M1 game-feel state.
    float hitStopTimer_ = 0.0f;
    float trauma_ = 0.0f;
    std::vector<Particle> particles_;
    std::vector<Sfx> sfxQueue_;
};

// ============================================================================
// Implementation
// ============================================================================

inline uint32_t State::nextRand() {
    rng_ = rng_ * 1664525u + 1013904223u;  // Numerical Recipes LCG
    return rng_;
}

inline int State::randInt(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(nextRand() % (uint32_t)(hi - lo));
}

inline int State::goldFor(EnemyKind kind) const {
    switch (kind) {
        case EnemyKind::Chaser: return 5;
        case EnemyKind::Spitter: return 8;
        case EnemyKind::Brute: return 15;
        case EnemyKind::Ghost: return 12;
        case EnemyKind::Turret: return 10;
    }
    return 5;
}

inline void State::resetFeelState() {
    hitStopTimer_ = 0.0f;
    trauma_ = 0.0f;
    particles_.clear();
    sfxQueue_.clear();
}

inline void State::bumpTrauma(float amount) {
    trauma_ = std::min(1.0f, trauma_ + amount);
}

inline void State::triggerHitStop(float seconds) {
    hitStopTimer_ = std::max(hitStopTimer_, seconds);
}

inline void State::pushSfx(Sfx s) {
    if (sfxQueue_.size() < (size_t)kMaxSfxQueue) sfxQueue_.push_back(s);
}

inline std::vector<Sfx> State::takeSfx() {
    std::vector<Sfx> out = std::move(sfxQueue_);
    sfxQueue_.clear();
    return out;
}

inline void State::spawnBurst(ParticleKind kind, float x, float y, int count, float speed) {
    for (int i = 0; i < count; ++i) {
        if ((int)particles_.size() >= kMaxParticles) break;
        float ang = (float)nextRand() / (float)UINT32_MAX * 6.2831853f;
        float spd = speed * (0.35f + 0.65f * ((float)(nextRand() % 1000) / 1000.0f));
        float life = 0.35f + 0.55f * ((float)(nextRand() % 1000) / 1000.0f);
        Particle pt;
        pt.kind = kind;
        pt.x = x;
        pt.y = y;
        pt.vx = std::cos(ang) * spd;
        pt.vy = std::sin(ang) * spd;
        pt.life = life;
        pt.maxLife = life;
        pt.size = (kind == ParticleKind::Blood) ? 3 : 2;
        particles_.push_back(pt);
    }
}

inline void State::stepParticles(float dt) {
    for (auto& pt : particles_) {
        pt.life -= dt;
        pt.x += pt.vx * dt;
        pt.y += pt.vy * dt;
        pt.vx *= std::max(0.0f, 1.0f - 3.0f * dt);  // drag
        pt.vy *= std::max(0.0f, 1.0f - 3.0f * dt);
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [](const Particle& pt) { return pt.life <= 0.0f; }),
                     particles_.end());
}

inline Enemy State::makeEnemy(EnemyKind kind, int x, int y, int floor) const {
    Enemy e;
    e.kind = kind;
    e.x = (float)x + 0.5f;
    e.y = (float)y + 0.5f;
    switch (kind) {
        case EnemyKind::Chaser: e.hp = 3; break;
        case EnemyKind::Spitter: e.hp = 2; break;
        case EnemyKind::Brute:   e.hp = 8; break;
        case EnemyKind::Ghost:   e.hp = 4; break;
        case EnemyKind::Turret:  e.hp = 5; break;
    }
    e.hp += floor - 1;  // enemies toughen as you descend
    e.maxHp = e.hp;
    e.fireTimer = 1.5f;
    initVolley(e);
    return e;
}

inline void State::newRun(uint32_t seed) {
    seed_ = seed;
    floorNum = 1;
    maxFloor = kMaxFloors;
    p = Player{};
    p.hp = kDefaultMaxHp;
    p.maxHp = kDefaultMaxHp;
    phase_ = Phase::Playing;
    setMessage("");
    resetFeelState();
    generateFloor();
}

inline void State::resetToEmpty(int width, int height) {
    w = width;
    h = height;
    tiles_.assign((size_t)w * (size_t)h, (int)Floor);
    enemies_.clear();
    projectiles_.clear();
    pickups_.clear();
    chests_.clear();
    doors_.clear();
    stairs_ = {-1, -1};
    p = Player{};
    p.x = 1.5f;
    p.y = 1.5f;
    p.facingX = 1.0f;
    p.facingY = 0.0f;
    floorNum = 1;
    maxFloor = kMaxFloors;
    seed_ = 0;
    rng_ = 0;
    phase_ = Phase::Playing;
    setMessage("");
    resetFeelState();
}

inline void State::setSolid(int x, int y, bool solid) {
    if (!inBounds(x, y)) return;
    tiles_[(size_t)y * (size_t)w + (size_t)x] = solid ? (int)Wall : (int)Floor;
}

inline void State::addEnemy(EnemyKind kind, int x, int y, int hp) {
    Enemy e;
    e.kind = kind;
    e.x = (float)x + 0.5f;
    e.y = (float)y + 0.5f;
    e.hp = hp;
    e.maxHp = hp;
    e.fireTimer = 1.5f;
    initVolley(e);
    enemies_.push_back(e);
}

inline void State::initVolley(Enemy& e) const {
    if (e.kind != EnemyKind::Turret) return;
    // First volley starts right away (fireTimer is only the between-volley
    // cooldown); volleyTimer counts down to each in-volley shot.
    e.volleyShots = kTurretVolleySize;
    e.volleyTimer = 0.15f;
}

inline void State::addPickup(PickupKind kind, int x, int y) {
    pickups_.push_back({kind, x, y, false});
}

inline void State::addChest(int x, int y, PickupKind contents) {
    chests_.push_back({x, y, false, contents});
}

inline void State::addDoor(int x, int y) {
    doors_.push_back({x, y, false});
    if (inBounds(x, y)) tiles_[(size_t)y * (size_t)w + (size_t)x] = (int)DoorTile;
}

inline bool State::openDoor(int x, int y) {
    for (auto& d : doors_) {
        if (d.x == x && d.y == y && !d.open) {
            if (p.keys <= 0) {
                setMessage("The door is locked. You need a key.");
                return false;
            }
            --p.keys;
            d.open = true;
            if (inBounds(x, y)) tiles_[(size_t)y * (size_t)w + (size_t)x] = (int)Floor;
            setMessage("Door unlocked.");
            pushSfx(Sfx::Door);
            return true;
        }
    }
    return false;
}

inline bool State::openChest(int x, int y) {
    for (auto& c : chests_) {
        if (c.x == x && c.y == y && !c.opened) {
            c.opened = true;
            switch (c.contents) {
                case PickupKind::Key:   ++p.keys; setMessage("Chest: a key!"); break;
                case PickupKind::Gold:  p.gold += 20; setMessage("Chest: 20 gold!"); break;
                case PickupKind::Heart: p.hp = std::min(p.maxHp, p.hp + 4); setMessage("Chest: a heart!"); break;
            }
            return true;
        }
    }
    return false;
}

inline void State::interactAdjacent() {
    int px = (int)p.x;
    int py = (int)p.y;
    for (const auto& d : doors_) {
        if (d.open) continue;
        if (std::abs(d.x - px) + std::abs(d.y - py) == 1) {
            openDoor(d.x, d.y);
            return;
        }
    }
    for (const auto& c : chests_) {
        if (c.opened) continue;
        if (std::abs(c.x - px) + std::abs(c.y - py) == 1) {
            openChest(c.x, c.y);
            return;
        }
    }
}

inline void State::generateFloor() {
    w = 35;
    h = 35;
    tiles_.assign((size_t)w * (size_t)h, (int)Wall);
    enemies_.clear();
    projectiles_.clear();
    pickups_.clear();
    chests_.clear();
    doors_.clear();

    // Per-floor deterministic stream (same run seed + floor => same map).
    rng_ = seed_ ^ (uint32_t)(floorNum * 0x9E3779B9u);

    // Carve non-overlapping rooms.
    int roomCount = 5 + randInt(0, 5);  // 5..9 rooms
    std::vector<std::pair<int, int>> centers;
    centers.reserve((size_t)roomCount);
    for (int i = 0; i < roomCount; ++i) {
        int rw = 4 + randInt(0, 5);  // 4..8
        int rh = 4 + randInt(0, 5);
        int rx = 1 + randInt(0, w - rw - 2);
        int ry = 1 + randInt(0, h - rh - 2);
        for (int y = ry; y < ry + rh; ++y)
            for (int x = rx; x < rx + rw; ++x)
                tiles_[(size_t)y * (size_t)w + (size_t)x] = (int)Floor;
        centers.emplace_back(rx + rw / 2, ry + rh / 2);
    }

    // Corridors between consecutive rooms (L-shaped).
    for (size_t i = 1; i < centers.size(); ++i) {
        int x1 = centers[i - 1].first, y1 = centers[i - 1].second;
        int x2 = centers[i].first, y2 = centers[i].second;
        for (int x = std::min(x1, x2); x <= std::max(x1, x2); ++x)
            tiles_[(size_t)y1 * (size_t)w + (size_t)x] = (int)Floor;
        for (int y = std::min(y1, y2); y <= std::max(y1, y2); ++y)
            tiles_[(size_t)y * (size_t)w + (size_t)x2] = (int)Floor;
    }

    // Player in the first room; stairs in the last.
    p.x = (float)centers[0].first + 0.5f;
    p.y = (float)centers[0].second + 0.5f;
    p.facingX = 1.0f;
    p.facingY = 0.0f;
    stairs_ = centers.back();

    // Door at the elbow of the final corridor: the choke point guarding the
    // stairs (rooms[last-1] -> rooms[last]).
    {
        int y1 = centers[centers.size() - 2].second;
        int x2 = centers.back().first;
        addDoor(x2, y1);
    }

    // Key in a chest in any room before the last (reachable pre-door).
    int chestRoom = randInt(0, (int)centers.size() - 1);  // [0, size-2]
    addChest(centers[(size_t)chestRoom].first, centers[(size_t)chestRoom].second, PickupKind::Key);

    // Scattered gold + one heart.
    int goldCount = 5 + randInt(0, 5);
    for (int i = 0; i < goldCount; ++i) {
        int x = randInt(1, w - 1);
        int y = randInt(1, h - 1);
        if (tiles_[(size_t)y * (size_t)w + (size_t)x] == (int)Floor)
            addPickup(PickupKind::Gold, x, y);
    }
    for (int tries = 0; tries < 40; ++tries) {
        int x = randInt(1, w - 1);
        int y = randInt(1, h - 1);
        if (tiles_[(size_t)y * (size_t)w + (size_t)x] == (int)Floor) {
            addPickup(PickupKind::Heart, x, y);
            break;
        }
    }

    // Enemies, away from the player's spawn room and not adjacent.
    int enemyCount = 2 + floorNum;
    for (int i = 0; i < enemyCount; ++i) {
        for (int tries = 0; tries < 200; ++tries) {
            int x = randInt(1, w - 1);
            int y = randInt(1, h - 1);
            if (tiles_[(size_t)y * (size_t)w + (size_t)x] != (int)Floor) continue;
            float dx = (float)x - p.x;
            float dy = (float)y - p.y;
            if (dx * dx + dy * dy < 36.0f) continue;
            // Floor 1 keeps the basic bestiary; deeper floors mix in the
            // M2 archetypes (ghost, turret).
            EnemyKind kind = (EnemyKind)randInt(0, floorNum >= 2 ? 5 : 3);
            enemies_.push_back(makeEnemy(kind, x, y, floorNum));
            break;
        }
    }

    setMessage("Floor " + std::to_string(floorNum) + ": find the key, open the door, reach the stairs.");
}

inline bool State::canOccupy(float fx, float fy) const {
    int x = (int)std::floor(fx);
    int y = (int)std::floor(fy);
    return inBounds(x, y) && tileAt(x, y) == (int)Floor;
}

inline void State::moveWithCollision(float dx, float dy) {
    const float r = kPlayerRadius;
    // X axis: only slide into the cell if all four corners stay walkable.
    float nx = p.x + dx;
    if (canOccupy(nx - r, p.y - r) && canOccupy(nx - r, p.y + r) &&
        canOccupy(nx + r, p.y - r) && canOccupy(nx + r, p.y + r)) {
        p.x = nx;
    }
    // Y axis.
    float ny = p.y + dy;
    if (canOccupy(p.x - r, ny - r) && canOccupy(p.x - r, ny + r) &&
        canOccupy(p.x + r, ny - r) && canOccupy(p.x + r, ny + r)) {
        p.y = ny;
    }
}

inline void State::tryAttack() {
    for (auto& e : enemies_) {
        if (!e.alive) continue;
        float dx = e.x - p.x;
        float dy = e.y - p.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > kAttackRange || dist < 0.001f) continue;
        // 120-degree forward arc: cos(60 deg) = 0.5.
        float dot = (dx * p.facingX + dy * p.facingY) / dist;
        if (dot < 0.5f) continue;
        int dmg = kBaseDamage + (dist > 1.2f ? 0 : 1);  // close hits hit harder
        e.hp -= dmg;
        e.hitFlash = 0.12f;
        bumpTrauma(0.18f);
        triggerHitStop(kHitStopHit);
        pushSfx(Sfx::Hit);
        spawnBurst(ParticleKind::Ember, e.x, e.y, 6, 3.5f);
        if (dist > 0.001f) {  // knockback
            e.x += dx / dist * 0.6f;
            e.y += dy / dist * 0.6f;
        }
        if (e.hp <= 0) {
            e.alive = false;
            p.gold += goldFor(e.kind);
            bumpTrauma(0.3f);
            triggerHitStop(kHitStopKill);
            pushSfx(Sfx::Kill);
            spawnBurst(ParticleKind::Blood, e.x, e.y, 12, 4.5f);
            setMessage("Enemy slain (+" + std::to_string(goldFor(e.kind)) + " gold)");
        }
    }
}

inline void State::stepEnemies(float dt) {
    for (auto& e : enemies_) {
        if (!e.alive) continue;
        if (e.hitFlash > 0.0f) e.hitFlash -= dt;
        float dx = p.x - e.x;
        float dy = p.y - e.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        float spd = 0.0f;
        bool phasing = false;
        switch (e.kind) {
            case EnemyKind::Chaser: spd = 2.2f; break;
            case EnemyKind::Brute:  spd = 1.2f; break;
            case EnemyKind::Ghost:  spd = kGhostSpeed; phasing = true; break;
            case EnemyKind::Spitter:
                if (dist > 4.0f) spd = 2.0f;
                else if (dist < 2.5f) spd = -1.6f;  // keep distance
                break;
            case EnemyKind::Turret: break;  // stationary
        }
        if (spd != 0.0f && dist > 0.001f) {
            float nx = e.x + dx / dist * spd * dt;
            float ny = e.y + dy / dist * spd * dt;
            if (phasing) {
                // Ghosts drift through walls; only stay inside the map.
                if (inBounds((int)nx, (int)ny)) { e.x = nx; e.y = ny; }
            } else if (canOccupy(nx, ny)) {
                e.x = nx; e.y = ny;
            }
        }

        // Spitters launch projectiles on a timer.
        if (e.kind == EnemyKind::Spitter) {
            e.fireTimer -= dt;
            if (e.fireTimer <= 0.0f) {
                e.fireTimer = 2.0f;
                if (dist < 6.0f && dist > 0.001f) {
                    float v = 3.5f;
                    projectiles_.push_back({e.x, e.y, dx / dist * v, dy / dist * v, 1, true});
                }
            }
        }

        // Turrets fire 3-shot volleys aimed at the player on a cooldown.
        if (e.kind == EnemyKind::Turret) {
            if (e.volleyShots > 0) {
                e.volleyTimer -= dt;
                if (e.volleyTimer <= 0.0f) {
                    e.volleyTimer = kTurretShotSpacing;
                    --e.volleyShots;
                    if (dist < kTurretRange && dist > 0.001f) {
                        // Deterministic jitter so a volley spreads a little.
                        float jitter = 0.08f * ((float)(nextRand() % 1000) / 1000.0f - 0.5f);
                        float v = kTurretProjSpeed;
                        float vx = (dx / dist + jitter * (-dy / dist)) * v;
                        float vy = (dy / dist + jitter * (dx / dist)) * v;
                        projectiles_.push_back({e.x, e.y, vx, vy, 1, true});
                    }
                }
            } else {
                e.fireTimer -= dt;
                if (e.fireTimer <= 0.0f) {
                    e.fireTimer = kTurretVolleyCooldown;
                    e.volleyShots = kTurretVolleySize;
                    e.volleyTimer = 0.0f;  // first shot fires immediately
                }
            }
        }

        // Contact damage (skipped while the player is rolling or invulnerable).
        if (dist < 0.7f && p.invulnTimer <= 0.0f && p.rollTimer <= 0.0f) {
            int dmg = (e.kind == EnemyKind::Brute) ? 2 : 1;
            hurtPlayer(dmg);
            if (dist > 0.001f) {
                p.x += dx / dist * 0.5f;
                p.y += dy / dist * 0.5f;
            }
        }
    }
    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(),
                                  [](const Enemy& e) { return !e.alive; }),
                   enemies_.end());
}

inline void State::stepProjectiles(float dt) {
    for (auto& pr : projectiles_) {
        if (!pr.alive) continue;
        pr.x += pr.vx * dt;
        pr.y += pr.vy * dt;
        if (!canOccupy(pr.x, pr.y)) { pr.alive = false; continue; }
        float dx = pr.x - p.x;
        float dy = pr.y - p.y;
        if (dx * dx + dy * dy < 0.25f) {
            pr.alive = false;
            if (p.invulnTimer <= 0.0f && p.rollTimer <= 0.0f) hurtPlayer(pr.damage);
        }
    }
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                                      [](const Projectile& pr) { return !pr.alive; }),
                       projectiles_.end());
}

inline void State::collectPickups() {
    int px = (int)p.x;
    int py = (int)p.y;
    for (auto& pk : pickups_) {
        if (pk.taken) continue;
        if (pk.x != px || pk.y != py) continue;
        pk.taken = true;
        pushSfx(Sfx::Pickup);
        spawnBurst(ParticleKind::Gold, (float)px + 0.5f, (float)py + 0.5f, 8, 2.5f);
        switch (pk.kind) {
            case PickupKind::Gold:  p.gold += 5; break;
            case PickupKind::Heart: p.hp = std::min(p.maxHp, p.hp + 2); break;
            case PickupKind::Key:   ++p.keys; setMessage("You found a key!"); break;
        }
    }
    pickups_.erase(std::remove_if(pickups_.begin(), pickups_.end(),
                                  [](const Pickup& pk) { return pk.taken; }),
                   pickups_.end());
}

inline void State::hurtPlayer(int amount) {
    if (p.invulnTimer > 0.0f || p.rollTimer > 0.0f) return;
    p.hp -= amount;
    p.hitFlash = 0.3f;
    p.invulnTimer = kInvulnAfterHit;
    bumpTrauma(0.5f);
    triggerHitStop(kHitStopHurt);
    pushSfx(Sfx::Hurt);
    spawnBurst(ParticleKind::Blood, p.x, p.y, 8, 3.0f);
    if (p.hp <= 0) {
        p.hp = 0;
        phase_ = Phase::Dead;
        setMessage("You died. Press R to restart.");
    }
}

inline void State::step(float dt, const Input& in) {
    if (phase_ != Phase::Playing) return;
    if (dt > 0.05f) dt = 0.05f;  // clamp tab-away / debugger pauses

    // Trauma (screen shake) eases even during the hit-stop freeze.
    trauma_ = std::max(0.0f, trauma_ - kTraumaDecay * dt);

    // Hit-stop: freeze the whole world for a beat on impactful hits.
    if (hitStopTimer_ > 0.0f) {
        hitStopTimer_ -= dt;
        return;
    }

    // Timer decay.
    if (p.attackCooldown > 0.0f) p.attackCooldown -= dt;
    if (p.attackTimer > 0.0f) p.attackTimer -= dt;
    if (p.rollCooldown > 0.0f) p.rollCooldown -= dt;
    if (p.invulnTimer > 0.0f) p.invulnTimer -= dt;
    if (p.hitFlash > 0.0f) p.hitFlash -= dt;
    if (p.rollTimer > 0.0f) p.rollTimer -= dt;

    stepParticles(dt);

    // Roll initiation.
    if (in.roll && p.rollCooldown <= 0.0f && p.rollTimer <= 0.0f) {
        p.rollTimer = kRollDuration;
        p.rollCooldown = kRollCooldown;
        float dx = in.moveX, dy = in.moveY;
        if (dx == 0.0f && dy == 0.0f) { dx = p.facingX; dy = p.facingY; }
        float len = std::sqrt(dx * dx + dy * dy);
        p.rollX = len > 0.0001f ? dx / len : 1.0f;
        p.rollY = len > 0.0001f ? dy / len : 0.0f;
    }

    // Movement.
    float spd = kPlayerSpeed;
    float mx = in.moveX;
    float my = in.moveY;
    if (p.rollTimer > 0.0f) {
        spd = kRollSpeed;
        mx = p.rollX;
        my = p.rollY;
    }
    float len = std::sqrt(mx * mx + my * my);
    if (len > 0.0001f) {
        mx /= len;
        my /= len;
        p.facingX = mx;
        p.facingY = my;
    }
    moveWithCollision(mx * spd * dt, my * spd * dt);

    // Attack.
    if (in.attack && p.attackCooldown <= 0.0f) {
        p.attackCooldown = kAttackCooldown;
        p.attackTimer = kAttackDuration;
        pushSfx(Sfx::Swing);
        tryAttack();
    }

    stepEnemies(dt);
    stepProjectiles(dt);
    collectPickups();

    if (in.interact) interactAdjacent();

    // Stairs (descend or win).
    int tx = (int)p.x;
    int ty = (int)p.y;
    if (stairs_.first >= 0 && tx == stairs_.first && ty == stairs_.second) {
        if (floorNum >= maxFloor) {
            phase_ = Phase::Won;
            setMessage("You reignited the Emberforge. You win!");
            pushSfx(Sfx::Win);
        } else {
            ++floorNum;
            setMessage("Descending to floor " + std::to_string(floorNum) + "...");
            pushSfx(Sfx::Descend);
            generateFloor();
        }
    }
}

inline std::string State::serialize() const {
    std::ostringstream oss;
    oss << "seed=" << seed_ << "\n";
    oss << "floor=" << floorNum << "\n";
    oss << "maxfloor=" << maxFloor << "\n";
    oss << "hp=" << p.hp << "\n";
    oss << "maxhp=" << p.maxHp << "\n";
    oss << "gold=" << p.gold << "\n";
    oss << "keys=" << p.keys << "\n";
    return oss.str();
}

inline bool State::load(const std::string& data) {
    int floor = 1, maxFloor = kMaxFloors, hp = kDefaultMaxHp, maxHp = kDefaultMaxHp, gold = 0, keys = 0;
    uint32_t seed = 0;
    std::istringstream iss(data);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq);
        const std::string v = line.substr(eq + 1);
        if (k == "seed") seed = (uint32_t)std::stoul(v);
        else if (k == "floor") floor = std::stoi(v);
        else if (k == "maxfloor") maxFloor = std::stoi(v);
        else if (k == "hp") hp = std::stoi(v);
        else if (k == "maxhp") maxHp = std::stoi(v);
        else if (k == "gold") gold = std::stoi(v);
        else if (k == "keys") keys = std::stoi(v);
    }
    seed_ = seed;
    this->maxFloor = maxFloor;
    floorNum = std::max(1, floor);
    p.hp = std::max(0, hp);
    p.maxHp = std::max(1, maxHp);
    p.gold = std::max(0, gold);
    p.keys = std::max(0, keys);
    phase_ = Phase::Playing;
    generateFloor();
    return true;
}

}  // namespace cinderfall
