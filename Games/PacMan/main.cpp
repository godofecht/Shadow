// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Pac-Man - the maze-chomping classic, game #13 of the 100-game program.
//
// A 24x15 tile maze with pellets and four power pellets. Eat every pellet to
// clear the maze. Four ghosts - red (chases you), pink (ambushes 4 tiles
// ahead), cyan (2 ahead), orange (cowardly) - run real scatter/chase cycles:
// they scatter to their corners, then hunt you; a power pellet flips them to
// blue frightened mode where they reverse, slow down, and can be eaten for
// 200/400/800/1600. Eaten ghosts zip home as eyes and re-emerge.
//
// Movement is authentic tile-center steering: you only turn at the center of
// a tile, and ghosts never reverse except when frightened or eaten.
//
// Built with the GameJuice kit from day one (Engine/Core/GameJuice.h): chomp
// sparkles with a rate-limited chomp, power-pellet surges, ghost-eat bursts
// with shake + hit-stop + floating combo scores, a death explosion with a NEW
// BEST celebration, and a MAZE CLEARED fanfare - all sound synthesized in
// memory, identical native / WASM / headless.
//
// One code path serves human input and the LLM: move_up/left/down/right set
// the same wish-direction the arrow keys drive, so an LLM plays the exact
// game a human plays.
//
// Controls: arrows / WASD = steer, P = pause, R = restart.

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

class PacMan : public Game2D {
    // ---- Maze ----------------------------------------------------------------
    static constexpr int WIDTH = 24;
    static constexpr int HEIGHT = 15;
    // Tiles: '#' wall, '.' pellet, 'o' power pellet, 'P' start (empty),
    // 'D' ghost door (wall for Pac-Man, passable for ghosts),
    // 'H' ghost-house interior (wall for everyone; ghosts park inside).
    // Validated by tools/maze layout checks: every pellet is reachable from
    // the start, and the house/door structure is intact.
    static constexpr const char* MAZE_STR[HEIGHT] = {
        "########################",
        "#......................#",
        "#o##.###.#.##.#.###.##o#",
        "#......................#",
        "#.##.#.###.##.###.#.##.#",
        "#......................#",
        "#....#....##DD##....#..#",
        "#....#....#HHHH#....#..#",
        "#....#....#HHHH#....#..#",
        "#....#....######....#..#",
        "#.##.#.###.##.###.#.##.#",
        "#......................#",
        "#o##.###.#.##.#.###.##o#",
        "#...........P..........#",
        "########################",
    };
    enum Tile : int { T_WALL, T_DOT, T_POWER, T_EMPTY, T_DOOR, T_HOUSE };

    // ---- Tunables ---------------------------------------------------------
    static constexpr int PX_START = 12, PY_START = 13;
    static constexpr int DOOR_X = 12, DOOR_Y = 6;
    static constexpr float PAC_SPEED = 7.0f;      // tiles/s
    static constexpr float GHOST_SPEED = 5.4f;
    static constexpr float FRIGHT_SPEED = 4.0f;
    static constexpr float EYES_SPEED = 8.0f;
    static constexpr float RELEASE_SPEED = 5.5f;
    static constexpr float FRIGHT_TIME = 6.0f;    // power-pellet duration
    static constexpr float SCATTER_SECS = 4.0f;   // scatter / chase cycle
    static constexpr float CHASE_SECS = 20.0f;
    static constexpr float CYCLE = SCATTER_SECS + CHASE_SECS + SCATTER_SECS + CHASE_SECS;
    static constexpr int MAX_LIVES = 3;
    static constexpr float RELEASE_TIMES[4] = {1.0f, 4.0f, 7.0f, 10.0f};
    static constexpr int SLOT_X[4] = {12, 13, 11, 14};  // house parking spots
    static constexpr int SLOT_Y[4] = {8, 8, 7, 7};
    static constexpr float PI_F = 3.14159265f;

    // Directions: 0 up, 1 right, 2 down, 3 left.
    static constexpr int DX[4] = {0, 1, 0, -1};
    static constexpr int DY[4] = {-1, 0, 1, 0};

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- World ------------------------------------------------------------
    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;

    int tiles[HEIGHT][WIDTH];   // tile kinds (walls, dots, doors, house)
    int dots[HEIGHT][WIDTH];    // 0 none, 1 pellet, 2 power pellet

    // ---- Entities ---------------------------------------------------------
    float pacx = (float)PX_START, pacy = (float)PY_START;
    int pdir = 3;               // 0..3; starts moving left
    int pwish = 3;              // requested direction (-1 = none)
    float mouthPhase = 0.0f;

    struct Ghost {
        float x, y;
        int dir = -1;
        // 0 wait (parked in house), 1 release (exiting the door),
        // 2 normal (scatter/chase), 3 frightened, 4 eaten (eyes).
        int state = 0;
        float waitTimer = 0.0f;
        int color = 0;          // 0 red, 1 pink, 2 cyan, 3 orange
    };
    Ghost ghosts[4];

    // ---- State ------------------------------------------------------------
    int lives = MAX_LIVES;
    int score = 0;
    int bestScore = 0;          // session best; survives restarts
    int pelletsLeft = 0;
    int combo = 1;              // ghost combo multiplier (200/400/800/1600)
    float frightTimer = 0.0f;
    float phaseClock = 0.0f;    // scatter/chase phase timer
    float deathPause = 0.0f;    // brief beat after losing a life
    float chompTimer = 0.0f;    // chomp-sound rate limiter
    int bfsDist[HEIGHT][WIDTH];
    int bfsFrom[HEIGHT][WIDTH];

    // Fixed-seed LCG: frightened turns and any other choice are deterministic.
    uint32_t lcgState = 0xCA11F00Du;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;

public:
    PacMan() : Game2D("Pac-Man", 960, 600, 12) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hook: exercise the no-heading ghostTarget branch. Freeze Pac-Man's
    // heading (pdir = -1, "stopped at a wall") inside a chase window and
    // return where a pink/cyan ghost would target: it must fall back to
    // Pac-Man's tile instead of indexing DX/DY[-1] (the out-of-bounds read
    // the sanitizer found).
    std::pair<int,int> ghostTargetStoppedForTest(int color) {
        pdir = -1;
        phaseClock = SCATTER_SECS + 0.5f;   // first chase window (scatter off)
        Ghost g;
        g.x = pacx;
        g.y = pacy;
        g.color = color;
        return ghostTarget(g);
    }

    void initGame() override {
        parseMaze();

        pacx = (float)PX_START;
        pacy = (float)PY_START;
        pdir = 3;
        pwish = 3;
        mouthPhase = 0.0f;
        lives = MAX_LIVES;
        score = 0;
        combo = 1;
        frightTimer = 0.0f;
        phaseClock = 0.0f;
        deathPause = 0.0f;
        chompTimer = 0.0f;
        paused = false;
        particles.clear();
        floatTexts = uj::FloatingText{};
        setupGhosts();

        createGrid(80, 50, tileSize);
        grid->fill({2, 4, 12, 255});
        grid->setBorderColor({8, 10, 20, 255});

        hud = createText(10, 6, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, 50 * tileSize - 26, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("Arrows/WASD to steer - eat all the pellets");

        registerAction("move_up", [this]() { return steerAction(0); });
        registerAction("move_right", [this]() { return steerAction(1); });
        registerAction("move_down", [this]() { return steerAction(2); });
        registerAction("move_left", [this]() { return steerAction(3); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_P).onPress([this]() { paused = !paused; });
        bindKey(KEY_R).onPress([this]() {
            if (gameOver || gameWon) startGame();
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

        // Short beat after losing a life: the world holds, fx keep running.
        if (deathPause > 0.0f) {
            deathPause -= dt;
            if (deathPause <= 0.0f) resetRound();
            updateFx(dt);
            return;
        }

        mouthPhase += dt * 9.0f;
        phaseClock += dt;
        if (frightTimer > 0.0f) {
            frightTimer -= dt;
            if (frightTimer <= 0.0f) {
                for (Ghost& g : ghosts) {
                    if (g.state == 3) g.state = 2;   // frightened -> normal
                }
            }
        }
        if (chompTimer > 0.0f) chompTimer -= dt;

        // ---- Steer: keyboard + smoke autopilot ----------------------------
        if (input.isKeyHeld(KEY_UP) || input.isKeyHeld(KEY_W)) pwish = 0;
        else if (input.isKeyHeld(KEY_RIGHT) || input.isKeyHeld(KEY_D)) pwish = 1;
        else if (input.isKeyHeld(KEY_DOWN) || input.isKeyHeld(KEY_S)) pwish = 2;
        else if (input.isKeyHeld(KEY_LEFT) || input.isKeyHeld(KEY_A)) pwish = 3;

        if (smokeMode) {
            // Re-plan at each tile center: BFS to the nearest pellet.
            const int cx = (int)std::lround(pacx);
            const int cy = (int)std::lround(pacy);
            const float eps = PAC_SPEED * dt + 0.0001f;
            if (std::fabs(pacx - (float)cx) < eps &&
                std::fabs(pacy - (float)cy) < eps) {
                const int d = autopilotDir();
                if (d >= 0) pwish = d;
            }
        }

        // ---- Pac-Man: move, then chomp at the tile center ------------------
        stepMover(pacx, pacy, pdir, pwish, PAC_SPEED, dt,
                  [this](int x, int y) { return pacWalk(x, y); });
        eatPellets();

        // ---- Ghosts ----------------------------------------------------------
        for (Ghost& g : ghosts) {
            switch (g.state) {
                case 0:  // waiting in the house
                    g.waitTimer -= dt;
                    if (g.waitTimer <= 0.0f) {
                        g.state = 1;
                        g.x = (float)DOOR_X;
                        g.y = (float)DOOR_Y;
                        g.dir = 0;             // face up, out the door
                    }
                    break;
                case 1:  // exiting the door
                    stepMover(g.x, g.y, g.dir, 0, RELEASE_SPEED, dt,
                              [this](int x, int y) { return ghostWalk(x, y); });
                    if (g.y < 5.0f) g.state = 2;
                    break;
                case 2:  // scatter / chase
                    stepMover(g.x, g.y, g.dir, ghostPick(g), GHOST_SPEED, dt,
                              [this](int x, int y) { return ghostWalk(x, y); });
                    break;
                case 3:  // frightened
                    stepMover(g.x, g.y, g.dir, ghostPickFright(g),
                              FRIGHT_SPEED, dt,
                              [this](int x, int y) { return ghostWalk(x, y); });
                    break;
                case 4:  // eyes zipping home
                    stepMover(g.x, g.y, g.dir, ghostPickEyes(g), EYES_SPEED, dt,
                              [this](int x, int y) { return ghostWalk(x, y); });
                    if ((int)std::lround(g.x) == DOOR_X &&
                        (int)std::lround(g.y) == DOOR_Y) {
                        g.state = 0;
                        g.waitTimer = 1.5f;
                        g.x = (float)SLOT_X[g.color];
                        g.y = (float)SLOT_Y[g.color];
                    }
                    break;
            }
        }

        // ---- Collisions -----------------------------------------------------
        const int pcx = (int)std::lround(pacx);
        const int pcy = (int)std::lround(pacy);
        for (Ghost& g : ghosts) {
            if (g.state == 0 || g.state == 4) continue;   // parked / eyes
            if ((int)std::lround(g.x) == pcx &&
                (int)std::lround(g.y) == pcy) {
                if (g.state == 3) eatGhost(g);
                else {
                    onPacDeath();
                    break;
                }
            }
        }

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        renderGrid();

        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // The maze shakes with the world; HUD and floating text do not.
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                const int px = (x * 3 + 4) * tileSize + sx;
                const int py = (y * 3 + 2) * tileSize + sy;
                switch (tiles[y][x]) {
                    case T_WALL: {
                        SDL_Rect r = {px, py, 3 * tileSize, 3 * tileSize};
                        SDL_SetRenderDrawColor(sdl, 34, 64, 140, 255);
                        SDL_RenderFillRect(sdl, &r);
                        SDL_SetRenderDrawColor(sdl, 22, 40, 96, 255);
                        SDL_RenderDrawRect(sdl, &r);
                        break;
                    }
                    case T_DOOR: {
                        // The pink door: a gap ghosts pass, Pac-Man can't.
                        SDL_SetRenderDrawColor(sdl, 255, 150, 190, 255);
                        SDL_Rect bar = {px, py + tileSize,
                                        3 * tileSize, tileSize};
                        SDL_RenderFillRect(sdl, &bar);
                        break;
                    }
                    case T_HOUSE: {
                        SDL_Rect r = {px, py, 3 * tileSize, 3 * tileSize};
                        SDL_SetRenderDrawColor(sdl, 18, 22, 42, 255);
                        SDL_RenderFillRect(sdl, &r);
                        break;
                    }
                    default:
                        break;
                }
                // Pellets (only on walkable tiles).
                if (dots[y][x] == 1) {
                    SDL_Rect d = {px + 3 * tileSize / 2 - 3,
                                  py + 3 * tileSize / 2 - 3, 6, 6};
                    SDL_SetRenderDrawColor(sdl, 255, 210, 110, 255);
                    SDL_RenderFillRect(sdl, &d);
                } else if (dots[y][x] == 2) {
                    const bool on = ((int)(phaseClock * 4.0f) % 2 == 0);
                    if (on) {
                        SDL_Rect d = {px + 3 * tileSize / 2 - 7,
                                      py + 3 * tileSize / 2 - 7, 14, 14};
                        SDL_SetRenderDrawColor(sdl, 255, 200, 90, 255);
                        SDL_RenderFillRect(sdl, &d);
                    }
                }
            }
        }

        // Ghosts.
        for (const Ghost& g : ghosts) {
            drawGhost(sdl, g);
        }

        // Pac-Man (hidden during the death beat).
        if (deathPause <= 0.0f) {
            drawPacman(sdl, pixX(pacx) + sx, pixY(pacy) + sy);
        }

        // Particles live in world space (they shake with it); floating text
        // stays screen-stable.
        particles.render(sdl, sx, sy);
        floatTexts.render(getRenderer());

        if (paused) {
            SDL_Rect veil = {0, 0, 80 * tileSize, 50 * tileSize};
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
        state.stats["pellets_left"] = pelletsLeft;
        state.stats["pac_x"] = (int)std::lround(pacx);
        state.stats["pac_y"] = (int)std::lround(pacy);
        state.stats["pac_dir"] = pdir;
        state.stats["power_left"] = (int)std::ceil(frightTimer);
        int frightened = 0, eaten = 0;
        for (const Ghost& g : ghosts) {
            if (g.state == 3) ++frightened;
            if (g.state == 4) ++eaten;
        }
        state.stats["frightened"] = frightened;
        state.stats["eaten"] = eaten;
        state.stats["ghosts"] = 4;
        int ngx = -1, ngy = -1, ngd = -1;
        float bd = 1e18f;
        for (const Ghost& g : ghosts) {
            if (g.state == 0 || g.state == 4) continue;   // not a threat
            const float d = std::hypot(g.x - pacx, g.y - pacy);
            if (d < bd) {
                bd = d;
                ngx = (int)std::lround(g.x);
                ngy = (int)std::lround(g.y);
            }
        }
        state.stats["nearest_ghost_x"] = ngx;
        state.stats["nearest_ghost_y"] = ngy;
        state.stats["nearest_ghost_dist"] = ngd;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.entities["player"] = {(int)std::lround(pacx),
                                    (int)std::lround(pacy)};
        for (int i = 0; i < 4; ++i) {
            state.entities["ghost_" + std::to_string(i)] = {
                (int)std::lround(ghosts[i].x),
                (int)std::lround(ghosts[i].y)
            };
        }
        return state;
    }

private:
    // ---- Maze ------------------------------------------------------------------
    void parseMaze() {
        pelletsLeft = 0;
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                const char c = MAZE_STR[y][x];
                dots[y][x] = 0;
                switch (c) {
                    case '#': tiles[y][x] = T_WALL; break;
                    case '.': tiles[y][x] = T_DOT; dots[y][x] = 1; ++pelletsLeft; break;
                    case 'o': tiles[y][x] = T_POWER; dots[y][x] = 2; ++pelletsLeft; break;
                    case 'D': tiles[y][x] = T_DOOR; break;
                    case 'H': tiles[y][x] = T_HOUSE; break;
                    default:  tiles[y][x] = T_EMPTY; break;   // ' ' and 'P'
                }
            }
        }
    }

    void setupGhosts() {
        for (int i = 0; i < 4; ++i) {
            ghosts[i].color = i;
            ghosts[i].state = 0;
            ghosts[i].waitTimer = RELEASE_TIMES[i];
            ghosts[i].dir = -1;
            ghosts[i].x = (float)SLOT_X[i];
            ghosts[i].y = (float)SLOT_Y[i];
        }
    }

    bool pacWalk(int x, int y) const {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return false;
        const int t = tiles[y][x];
        return t != T_WALL && t != T_DOOR && t != T_HOUSE;
    }

    bool ghostWalk(int x, int y) const {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return false;
        const int t = tiles[y][x];
        return t != T_WALL && t != T_HOUSE;   // door is passable for ghosts
    }

    // ---- Movement ----------------------------------------------------------------
    // Tile-center steering: turns only apply at tile centers; a blocked wish
    // keeps the current heading; a blocked heading stops at the center.
    // eps is a fraction of a step so a snapped entity never re-snaps to the
    // tile it just left (that would freeze it in place), while still snapping
    // as it approaches the next center.
    void stepMover(float& x, float& y, int& dir, int wish, float speed,
                   float dt, const std::function<bool(int, int)>& canWalk) {
        const int cx = (int)std::lround(x);
        const int cy = (int)std::lround(y);
        const float eps = speed * dt * 0.5f;
        if (std::fabs(x - (float)cx) < eps && std::fabs(y - (float)cy) < eps) {
            x = (float)cx;
            y = (float)cy;
            if (wish >= 0 && canWalk(cx + DX[wish], cy + DY[wish])) {
                dir = wish;
            } else if (dir < 0 || !canWalk(cx + DX[dir], cy + DY[dir])) {
                dir = -1;
            }
        }
        if (dir >= 0) {
            x += DX[dir] * speed * dt;
            y += DY[dir] * speed * dt;
        }
    }

    void eatPellets() {
        const int cx = (int)std::lround(pacx);
        const int cy = (int)std::lround(pacy);
        if (dots[cy][cx] == 0) return;
        const bool power = dots[cy][cx] == 2;
        dots[cy][cx] = 0;
        --pelletsLeft;
        if (power) {
            score += 50;
            frightTimer = FRIGHT_TIME;
            combo = 1;
            for (Ghost& g : ghosts) {
                if (g.state == 2) {
                    g.state = 3;
                    g.dir = (g.dir + 2) % 4;   // classic instant reversal
                }
            }
            // ---- Juice: power surge ----------------------------------------
            particles.burst((float)pixX((float)cx), (float)pixY((float)cy), 18,
                            {230, 200, 60, 255}, 9.0f, 0.6f, 5.0f);
            shake.add(0.25f);
            hitStop.trigger(0.05f);
            sfx.play(uj::Sfx::Pickup);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                pixX((float)cx) - 24, pixY((float)cy) - 20, "POWER!"),
                pixX((float)cx) - 24, pixY((float)cy) - 20);
            setMessage("POWER! Eat the ghosts!");
        } else {
            score += 10;
            // ---- Juice: chomp sparkle + rate-limited chomp -----------------
            particles.burst((float)pixX((float)cx), (float)pixY((float)cy), 3,
                            {255, 210, 110, 255}, 2.5f, 0.3f, 3.0f);
            if (chompTimer <= 0.0f) {
                sfx.play(uj::Sfx::Ping);
                chompTimer = 0.12f;
            }
        }
        if (pelletsLeft <= 0) {
            onWin();
            return;
        }
        updateHUD();
    }

    // ---- Ghost AI -------------------------------------------------------------
    bool scatterPhase() const {
        const float t = std::fmod(phaseClock, CYCLE);
        return t < SCATTER_SECS || (t >= SCATTER_SECS + CHASE_SECS);
    }

    std::pair<int, int> ghostTarget(const Ghost& g) const {
        const int px = (int)std::lround(pacx);
        const int py = (int)std::lround(pacy);
        if (scatterPhase()) {
            switch (g.color) {
                case 0: return {22, 1};   // red: top-right
                case 1: return {1, 1};    // pink: top-left
                case 2: return {22, 13};  // cyan: bottom-right
                default: return {1, 13};  // orange: bottom-left
            }
        }
        // Pink/cyan chase "ahead" of Pac-Man's heading. pdir is -1 while he
        // is stopped at a wall (blocked, no valid wish), which has no heading
        // - indexing DX/DY[-1] would be out-of-bounds, so they aim at his
        // tile directly in that case.
        switch (g.color) {
            case 0: return {px, py};                      // red: direct chase
            case 1: return pdir >= 0
                        ? std::make_pair(clampT(px + DX[pdir] * 4),  // pink: 4 ahead
                                         clampT(py + DY[pdir] * 4))
                        : std::make_pair(px, py);
            case 2: return pdir >= 0
                        ? std::make_pair(clampT(px + DX[pdir] * 2),  // cyan: 2 ahead
                                         clampT(py + DY[pdir] * 2))
                        : std::make_pair(px, py);
            default: {                                    // orange: coward
                const float d = std::hypot(g.x - px, g.y - py);
                return d > 8.0f ? std::make_pair(px, py)
                                : std::make_pair(1, 13);
            }
        }
    }

    int ghostPick(const Ghost& g) {
        const int cx = (int)std::lround(g.x);
        const int cy = (int)std::lround(g.y);
        const auto [tx, ty] = ghostTarget(g);
        int best = -1;
        float bestD = 1e18f;
        for (int d = 0; d < 4; ++d) {
            if (d == (g.dir + 2) % 4) continue;           // never reverse
            if (!ghostWalk(cx + DX[d], cy + DY[d])) continue;
            const float dd = std::abs((float)(cx + DX[d] - tx)) +
                             std::abs((float)(cy + DY[d] - ty));
            if (dd < bestD) {
                bestD = dd;
                best = d;
            }
        }
        if (best < 0) {                                   // dead end: reverse
            const int r = (g.dir + 2) % 4;
            best = ghostWalk(cx + DX[r], cy + DY[r]) ? r : g.dir;
        }
        return best;
    }

    int ghostPickFright(const Ghost& g) {
        const int cx = (int)std::lround(g.x);
        const int cy = (int)std::lround(g.y);
        int cand[4];
        int n = 0;
        for (int d = 0; d < 4; ++d) {
            if (d == (g.dir + 2) % 4) continue;
            if (!ghostWalk(cx + DX[d], cy + DY[d])) continue;
            cand[n++] = d;
        }
        return n > 0 ? cand[lcgNext() % (uint32_t)n] : g.dir;
    }

    int ghostPickEyes(const Ghost& g) {
        const int cx = (int)std::lround(g.x);
        const int cy = (int)std::lround(g.y);
        int best = -1;
        float bestD = 1e18f;
        for (int d = 0; d < 4; ++d) {
            if (!ghostWalk(cx + DX[d], cy + DY[d])) continue;
            const float dd = std::abs((float)(cx + DX[d] - DOOR_X)) +
                             std::abs((float)(cy + DY[d] - DOOR_Y));
            if (dd < bestD) {
                bestD = dd;
                best = d;
            }
        }
        return best >= 0 ? best : g.dir;
    }

    // ---- Outcomes ---------------------------------------------------------------
    void eatGhost(Ghost& g) {
        const int pts = 200 * combo;
        combo *= 2;
        score += pts;
        g.state = 4;                          // becomes eyes
        // ---- Juice: burst in the ghost's color -----------------------------
        particles.burst((float)pixX(g.x), (float)pixY(g.y), 18, ghostColor(g.color),
                        9.0f, 0.6f, 5.0f);
        shake.add(0.3f);
        hitStop.trigger(0.06f);
        sfx.play(uj::Sfx::Kill);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            pixX(g.x) - 18, pixY(g.y) - 20, "+" + std::to_string(pts)),
            pixX(g.x) - 18, pixY(g.y) - 20);
        updateHUD();
    }

    void onPacDeath() {
        --lives;
        // ---- Juice: the chomper bursts --------------------------------------
        particles.burst((float)pixX(pacx), (float)pixY(pacy), 24,
                        {255, 220, 60, 255}, 10.0f, 0.7f, 6.0f);
        particles.burst((float)pixX(pacx), (float)pixY(pacy), 8,
                        {255, 255, 255, 255}, 7.0f, 0.5f, 5.0f);
        shake.add(0.6f);
        hitStop.trigger(0.15f);
        if (lives <= 0) {
            const bool newBest = score > bestScore;
            bestScore = std::max(bestScore, score);
            if (newBest) {
                sfx.play(uj::Sfx::Win);
                particles.burst(480.0f, 300.0f, 14,
                                {230, 200, 60, 255}, 8.0f, 0.7f, 5.0f);
                floatTexts.spawn(std::make_shared<TextDisplay>(
                    400, 120, "NEW BEST!"), 400, 120);
            } else {
                sfx.play(uj::Sfx::Lose);
            }
            setMessage("GAME OVER - Best " + std::to_string(bestScore) +
                       " - Press R to restart");
            endGame();
            return;
        }
        sfx.play(uj::Sfx::Explode);
        deathPause = 0.9f;
        setMessage("Oof! " + std::to_string(lives) + " lives left");
        updateHUD();
    }

    void resetRound() {
        pacx = (float)PX_START;
        pacy = (float)PY_START;
        pdir = 3;
        pwish = 3;
        combo = 1;
        frightTimer = 0.0f;
        phaseClock = 0.0f;
        deathPause = 0.0f;
        setupGhosts();
        setMessage("Arrows/WASD to steer - eat all the pellets");
    }

    void onWin() {
        bestScore = std::max(bestScore, score);
        // ---- Juice: fanfare + confetti --------------------------------------
        sfx.play(uj::Sfx::Win);
        shake.add(0.5f);
        for (int i = 0; i < 3; ++i) {
            particles.burst(240.0f + (float)i * 240.0f, 260.0f, 20,
                (i == 0) ? SDL_Color{255, 220, 60, 255} :
                (i == 1) ? SDL_Color{80, 220, 255, 255} :
                           SDL_Color{140, 255, 120, 255},
                9.0f, 0.8f, 6.0f);
        }
        floatTexts.spawn(std::make_shared<TextDisplay>(
            380, 100, "MAZE CLEARED!"), 380, 100);
        setMessage("MAZE CLEARED! Best " + std::to_string(bestScore) +
                   " - Press R to play again");
        gameWon = true;
        endGame();
    }

    // ---- LLM actions ---------------------------------------------------------
    ActionResult steerAction(int dir) {
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
        pwish = dir;
        result.success = true;
        result.message = "Heading " + dirName(dir);
        return result;
    }

    static std::string dirName(int d) {
        switch (d) {
            case 0: return "up";
            case 1: return "right";
            case 2: return "down";
            default: return "left";
        }
    }

    // ---- Autopilot ---------------------------------------------------------
    // BFS to the nearest pellet, then step along the shortest route while
    // dodging threatening ghosts (never step onto/next to one when an
    // alternative exists), so the bot genuinely clears the maze instead of
    // walking into the hunters. Deterministic queue order; recomputed at
    // every tile center, so it can never loop.
    int autopilotDir() {
        const int sx = (int)std::lround(pacx);
        const int sy = (int)std::lround(pacy);
        // 1) Forward BFS from the player: find the nearest pellet.
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x) bfsDist[y][x] = -1;
        struct Q { int x, y; };
        std::vector<Q> q;
        q.push_back({sx, sy});
        bfsDist[sy][sx] = 0;
        int px = -1, py = -1;
        for (size_t h = 0; h < q.size(); ++h) {
            const Q cur = q[h];
            if (!(cur.x == sx && cur.y == sy) && dots[cur.y][cur.x] != 0) {
                px = cur.x;
                py = cur.y;
                break;
            }
            for (int d = 0; d < 4; ++d) {
                const int nx = cur.x + DX[d];
                const int ny = cur.y + DY[d];
                if (!pacWalk(nx, ny) || bfsDist[ny][nx] >= 0) continue;
                bfsDist[ny][nx] = bfsDist[cur.y][cur.x] + 1;
                q.push_back({nx, ny});
            }
        }
        if (px < 0) return -1;                    // no pellets left
        // 2) Backward BFS from that pellet: maze distance to it for every tile.
        for (int y = 0; y < HEIGHT; ++y)
            for (int x = 0; x < WIDTH; ++x) bfsFrom[y][x] = -1;
        q.clear();
        q.push_back({px, py});
        bfsFrom[py][px] = 0;
        for (size_t h = 0; h < q.size(); ++h) {
            const Q cur = q[h];
            for (int d = 0; d < 4; ++d) {
                const int nx = cur.x + DX[d];
                const int ny = cur.y + DY[d];
                if (!pacWalk(nx, ny) || bfsFrom[ny][nx] >= 0) continue;
                bfsFrom[ny][nx] = bfsFrom[cur.y][cur.x] + 1;
                q.push_back({nx, ny});
            }
        }
        // 3) First step: the walkable neighbor with the smallest distance to
        //    the pellet. Prefer routes that stay 2 tiles clear of a ghost,
        //    then 1 tile clear, then any shortest route (rarely cornered).
        int best = -1;
        int bestDist = 1 << 30;
        for (int radius = 2; radius >= 0 && best < 0; --radius) {
            for (int d = 0; d < 4; ++d) {
                const int nx = sx + DX[d];
                const int ny = sy + DY[d];
                if (!pacWalk(nx, ny) || bfsFrom[ny][nx] < 0) continue;
                if (nearThreat(nx, ny, radius)) continue;
                if (bfsFrom[ny][nx] < bestDist) {
                    bestDist = bfsFrom[ny][nx];
                    best = d;
                }
            }
        }
        return best;
    }

    // Is any threatening ghost (releasing / normal / frightened) within
    // Manhattan distance <= radius of (x, y)? Used by the autopilot to route
    // around hunters before they can pounce.
    bool nearThreat(int x, int y, int radius) const {
        for (const Ghost& g : ghosts) {
            if (g.state != 1 && g.state != 2 && g.state != 3) continue;
            const int gx = (int)std::lround(g.x);
            const int gy = (int)std::lround(g.y);
            if (std::abs(gx - x) + std::abs(gy - y) <= radius) return true;
        }
        return false;
    }

    // ---- Helpers ----------------------------------------------------------------
    static int clampT(int v) { return v < 1 ? 1 : (v > 22 ? 22 : v); }

    static SDL_Color ghostColor(int c) {
        switch (c) {
            case 0: return {240, 70, 70, 255};
            case 1: return {250, 150, 200, 255};
            case 2: return {80, 220, 240, 255};
            default: return {240, 180, 70, 255};
        }
    }

    // Tile coordinate -> pixel center (maze offset 4,2 cells; 3 cells per tile).
    int pixX(float tx) const {
        return (int)std::lround((4.0f + tx * 3.0f) * tileSize + 1.5f * tileSize);
    }
    int pixY(float ty) const {
        return (int)std::lround((2.0f + ty * 3.0f) * tileSize + 1.5f * tileSize);
    }

    // Scanline-filled polygon (the same helper the pixel-art games use).
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
                if (a.y == b.y) continue;
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

    // ---- Rendering ------------------------------------------------------------
    void drawPacman(SDL_Renderer* sdl, int cx, int cy) const {
        // A yellow wedge with an animated chomping mouth.
        const float r = 17.0f;
        const float m = 0.18f + 0.30f * std::fabs(std::sin(mouthPhase));
        std::vector<SDL_Point> pts;
        pts.push_back({cx, cy});
        const int n = 12;
        const float a0 = m;
        const float a1 = 2.0f * PI_F - m;
        for (int i = 0; i <= n; ++i) {
            const float a = a0 + (a1 - a0) * (float)i / (float)n;
            pts.push_back({cx + (int)(std::cos(a) * r),
                           cy + (int)(std::sin(a) * r)});
        }
        fillPoly(sdl, pts, {255, 220, 60, 255});
    }

    void drawGhost(SDL_Renderer* sdl, const Ghost& g) const {
        const int cx = pixX(g.x);
        const int cy = pixY(g.y);
        const float r = 15.0f;

        if (g.state == 4) {           // eaten: eyes only
            drawEyes(sdl, cx, cy, g.dir);
            return;
        }

        SDL_Color col;
        if (g.state == 3) {
            const bool flash = frightTimer < 1.5f &&
                ((int)(phaseClock * 10.0f) % 2 == 0);
            col = flash ? SDL_Color{250, 250, 250, 255}
                        : SDL_Color{70, 90, 230, 255};
        } else {
            col = ghostColor(g.color);
            if (g.state == 0) {       // parked in the house: dimmed
                col.r = (Uint8)(col.r / 2);
                col.g = (Uint8)(col.g / 2);
                col.b = (Uint8)(col.b / 2);
            }
        }

        // Dome + body: top half-circle over a square bottom.
        std::vector<SDL_Point> pts;
        for (int i = 0; i <= 10; ++i) {
            const float a = PI_F + PI_F * (float)i / 10.0f;
            pts.push_back({cx + (int)(std::cos(a) * r),
                           cy + (int)(std::sin(a) * r)});
        }
        pts.push_back({cx + (int)r, cy + (int)r});
        pts.push_back({cx - (int)r, cy + (int)r});
        fillPoly(sdl, pts, col);

        if (g.state == 3) {
            // Frightened: white eyes + a jagged mouth.
            drawEyes(sdl, cx, cy, g.dir);
            SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
            SDL_Rect m1 = {cx - 8, cy + (int)r - 5, 5, 4};
            SDL_Rect m2 = {cx - 2, cy + (int)r - 9, 5, 4};
            SDL_Rect m3 = {cx + 4, cy + (int)r - 5, 5, 4};
            SDL_RenderFillRect(sdl, &m1);
            SDL_RenderFillRect(sdl, &m2);
            SDL_RenderFillRect(sdl, &m3);
        } else {
            drawEyes(sdl, cx, cy, g.dir);
        }
    }

    void drawEyes(SDL_Renderer* sdl, int cx, int cy, int dir) const {
        // Ghosts idle with dir == -1 (stopped at a wall or parked in the
        // house); look straight ahead instead of indexing DX/DY[-1].
        const int ex = dir >= 0 ? DX[dir] : 0;
        const int ey = dir >= 0 ? DY[dir] : 0;
        SDL_SetRenderDrawColor(sdl, 255, 255, 255, 255);
        SDL_Rect e1 = {cx - 11, cy - 9, 8, 8};
        SDL_Rect e2 = {cx + 3, cy - 9, 8, 8};
        SDL_RenderFillRect(sdl, &e1);
        SDL_RenderFillRect(sdl, &e2);
        SDL_SetRenderDrawColor(sdl, 20, 40, 90, 255);
        SDL_Rect p1 = {cx - 11 + ex * 3, cy - 9 + ey * 3, 3, 3};
        SDL_Rect p2 = {cx + 3 + ex * 3, cy - 9 + ey * 3, 3, 3};
        SDL_RenderFillRect(sdl, &p1);
        SDL_RenderFillRect(sdl, &p2);
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
                     "    Lives " + std::to_string(lives));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the PacMan class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static PacMan game;
#else
    PacMan game;
#endif
    game.run();
    return 0;
}
#endif
