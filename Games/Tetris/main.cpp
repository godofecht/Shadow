// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Tetris - the block-stacking classic, game #9 of the 100-game program.
//
// A 10x20 playfield, the seven tetrominoes dealt from a 7-bag, SRS rotation
// with wall kicks, gravity that ramps with level, a hold queue, ghost piece,
// and 1/2/3/4-line scoring. The board is exposed to the LLM as a real grid
// (values 1-7 = piece types), so an agent can see exactly what a player sees.
//
// One code path serves human input and the LLM: A/D/arrows poll the same
// tryMove() as "move_left"/"move_right", SPACE / "hard_drop" share
// doHardDrop(), and the 1/2/3 keys map to rotate_cw / rotate_ccw / hold. An
// LLM can play the exact game a human plays.
//
// Controls: A/D or arrows = move, S/down = soft drop (hold for continuous),
//           SPACE/W/up = hard drop, 1 = rotate CW, 2 = rotate CCW,
//           3 = hold, R = restart.
//
// All "randomness" (the bag shuffle) flows through a fixed-seed LCG so a
// given playthrough is fully deterministic and unit-testable. A greedy
// placement solver powers PONG_SMOKE autoplay, so the dummy-driver CI run
// actually clears lines.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <string>
#include <utility>
#include <vector>

class Tetris : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int COLS = 10;
    static constexpr int ROWS = 20;

    enum PieceKind {
        PIECE_I = 0, PIECE_O, PIECE_T, PIECE_S, PIECE_Z, PIECE_J, PIECE_L,
        PIECE_COUNT
    };

    struct Cell { int dx, dy; };

    // SRS piece definitions: [kind][rotation][4 cells] within a 4x4 box
    // (JLSTZ use the top-left 3x3, I the full 4x4, O the top-left 2x2).
    static const Cell SHAPES[PIECE_COUNT][4][4];

    // SRS wall-kick tables: kicks[from][to][attempt] for JLSTZ (5 attempts)
    // and the I piece. O never rotates so it needs none.
    static const int JLSTZ_KICKS[4][4][5][2];
    static const int I_KICKS[4][4][5][2];

    static const SDL_Color PIECE_COLORS[PIECE_COUNT];
    static const SDL_Color GHOST_COLOR;
    static constexpr int TILE = 24;

    // Scoring (guideline): 100/300/500/800 x level for 1/2/3/4 lines.
    static constexpr int CLEAR_POINTS[5] = {0, 100, 300, 500, 800};

    // ---- World ------------------------------------------------------------
    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;
    std::shared_ptr<TextDisplay> scoreText;
    std::shared_ptr<TextDisplay> linesText;
    std::shared_ptr<TextDisplay> levelText;
    std::shared_ptr<TextDisplay> holdLabel;
    std::shared_ptr<TextDisplay> nextLabel;

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- State ------------------------------------------------------------
    int board[ROWS][COLS];          // 0 = empty, 1..7 = piece type
    std::deque<int> bag;            // 7-bag of upcoming pieces
    int kind = PIECE_I;             // current falling piece
    int rot = 0;                    // 0..3 (0 = spawn, 1 = CW, 2 = 180, 3 = CCW)
    int px = 3, py = 0;             // top-left of the piece's 4x4 box
    int heldKind = -1;              // -1 = nothing held yet
    bool canHold = true;            // one hold per piece

    int score = 0;
    int bestScore = 0;             // session best; survives restarts
    int lines = 0;
    int level = 1;
    float gravityTimer = 0.0f;
    int ghostY = 0;

    // Fixed-seed LCG: the bag order is reproducible.
    uint32_t lcgState = 0x7E7E7Eu;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;
    float smokeTimer = 0.0f;

public:
    Tetris() : Game2D("Tetris", 960, 600, TILE) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests: lets tests drive the autoplay solver
    // through tick() (setSmokeMode itself is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    void initGame() override {
        score = 0;
        lines = 0;
        level = 1;
        heldKind = -1;
        canHold = true;
        gravityTimer = 0.0f;
        smokeTimer = 0.0f;
        paused = false;
        particles.clear();
        floatTexts = uj::FloatingText{};
        shake = uj::ScreenShake{};
        hitStop = uj::HitStop{};
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c)
                board[r][c] = 0;
        bag.clear();
        refillBag();
        spawnNext();

        // The 10x20 playfield grid; cells are colored per-piece in syncGrid.
        createGrid(COLS, ROWS, TILE);
        grid->fill({10, 12, 18, 255});
        grid->setBorderColor({16, 18, 26, 255});

        hud = createText(270, 20, "TETRIS");
        hud->setColor({255, 255, 255, 255});
        scoreText = createText(270, 70, "");
        scoreText->setColor({255, 255, 255, 255});
        linesText = createText(270, 130, "");
        linesText->setColor({255, 255, 255, 255});
        levelText = createText(270, 190, "");
        levelText->setColor({255, 255, 255, 255});
        holdLabel = createText(270, 250, "HOLD");
        holdLabel->setColor({255, 255, 255, 255});
        nextLabel = createText(270, 355, "NEXT");
        nextLabel->setColor({255, 255, 255, 255});
        message = createText(270, 555, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("SPACE or 'hard_drop' to drop");

        registerAction("move_left", [this]() { return movePiece(-1, 0); });
        registerAction("move_right", [this]() { return movePiece(1, 0); });
        registerAction("soft_drop", [this]() { return doSoftDrop(); });
        registerAction("hard_drop", [this]() { return doHardDrop(); });
        registerAction("rotate_cw", [this]() { return doRotate(1); });
        registerAction("rotate_ccw", [this]() { return doRotate(-1); });
        registerAction("hold", [this]() { return doHold(); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindKey(KEY_LEFT).onPress([this]() { movePiece(-1, 0); });
        bindKey(KEY_RIGHT).onPress([this]() { movePiece(1, 0); });
        bindKey(KEY_A).onPress([this]() { movePiece(-1, 0); });
        bindKey(KEY_D).onPress([this]() { movePiece(1, 0); });
        bindKey(KEY_SPACE).onPress([this]() { doHardDrop(); });
        bindKey(KEY_W).onPress([this]() { doHardDrop(); });
        bindKey(KEY_UP).onPress([this]() { doHardDrop(); });
        bindKey(KEY_1).onPress([this]() { doRotate(1); });
        bindKey(KEY_2).onPress([this]() { doRotate(-1); });
        bindKey(KEY_3).onPress([this]() { doHold(); });
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

        // Keyboard: continuous soft drop.
        if (input.isKeyHeld(KEY_S) || input.isKeyHeld(KEY_DOWN)) {
            if (!tryMoveDown()) {
                lockPiece();
            } else {
                score += 1;  // guideline soft-drop scoring
            }
        }

        // Headless smoke mode: a greedy placement solver plays the game (see
        // solvePlacement), so a dummy-driver run exercises rotation, kicks,
        // locks, line clears, and scoring - not just a waiting piece.
        if (smokeMode) {
            smokeTimer += dt;
            if (smokeTimer >= 0.4f) {
                smokeTimer = 0.0f;
                autoplayStep();
            }
        }

        // Gravity: falls faster with level.
        gravityTimer -= dt;
        if (gravityTimer <= 0.0f) {
            gravityTimer += gravityInterval();
            if (!tryMoveDown()) {
                lockPiece();
            }
        }

        updateFx(dt);
        syncGrid();
        updateHUD();
    }

    void renderGame() override {
        SDL_Renderer* sdl = getRenderer()->renderer;

        // World space shakes (the well, settled blocks, live piece, ghost,
        // and particles); the side panel, HUD, and floating text stay put.
        const auto [sx, sy] = shake.offset();

        // Playfield well (dark backdrop + border).
        SDL_Rect well = {sx, sy, COLS * TILE, ROWS * TILE};
        SDL_SetRenderDrawColor(sdl, 10, 12, 18, 255);
        SDL_RenderFillRect(sdl, &well);
        SDL_SetRenderDrawColor(sdl, 16, 18, 26, 255);
        SDL_RenderDrawRect(sdl, &well);

        // Settled blocks.
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                if (board[r][c] == 0) continue;
                drawBlock(sdl, c * TILE + sx, r * TILE + sy,
                          PIECE_COLORS[board[r][c] - 1]);
            }
        }

        // The live piece (bright).
        for (int i = 0; i < 4; ++i) {
            const int gx = px + SHAPES[kind][rot][i].dx;
            const int gy = py + SHAPES[kind][rot][i].dy;
            if (gy < 0 || gy >= ROWS) continue;
            drawBlock(sdl, gx * TILE + sx, gy * TILE + sy, PIECE_COLORS[kind]);
        }

        // The ghost piece: a dim outline of where the piece will land.
        SDL_SetRenderDrawColor(sdl, GHOST_COLOR.r, GHOST_COLOR.g, GHOST_COLOR.b, 255);
        for (int i = 0; i < 4; ++i) {
            const int gx = px + SHAPES[kind][rot][i].dx;
            const int gy = ghostY + SHAPES[kind][rot][i].dy;
            if (gy < 0 || gy >= ROWS) continue;
            SDL_Rect cell = {gx * TILE + sx, gy * TILE + sy, TILE, TILE};
            SDL_RenderDrawRect(sdl, &cell);
        }

        // Side panel: hold + next previews (mini 12px sprites).
        drawPanel();

        // Particles live in world space, so they shake with the board.
        particles.render(sdl, sx, sy);

        // Floating score labels (screen space).
        floatTexts.render(getRenderer());

        // Pause veil.
        if (paused) {
            SDL_Rect veil = {0, 0, 960, 600};
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
        state.stats["lines"] = lines;
        state.stats["level"] = level;
        state.stats["current_piece"] = kind;
        state.stats["current_x"] = px;
        state.stats["current_y"] = py;
        state.stats["current_rot"] = rot;
        state.stats["hold"] = heldKind;
        state.stats["can_hold"] = canHold ? 1 : 0;
        state.stats["next1"] = bag.empty() ? -1 : bag[0];
        state.stats["next2"] = bag.size() > 1 ? bag[1] : -1;
        state.stats["next3"] = bag.size() > 2 ? bag[2] : -1;
        state.stats["stack_height"] = stackHeight();
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        state.stats["shake"] = static_cast<int>(shake.level() * 1000.0f);
        return state;
    }

private:
    // ---- Pieces ---------------------------------------------------------------
    float gravityInterval() const {
        // 0.8s at level 1, ~0.05s floor; each level is ~15% faster.
        float interval = 0.8f;
        for (int l = 1; l < level; ++l) interval *= 0.85f;
        return std::max(0.05f, interval);
    }

    bool collides(int k, int r, int x, int y) const {
        return collidesOn(k, r, x, y, board);
    }

    bool collidesOn(int k, int r, int x, int y,
                    const int b[ROWS][COLS]) const {
        for (int i = 0; i < 4; ++i) {
            const int gx = x + SHAPES[k][r][i].dx;
            const int gy = y + SHAPES[k][r][i].dy;
            if (gx < 0 || gx >= COLS || gy >= ROWS) return true;
            if (gy >= 0 && b[gy][gx] != 0) return true;
        }
        return false;
    }

    int dropY(int k, int r, int x) const {
        return dropYOn(k, r, x, py, board);
    }

    int dropYOn(int k, int r, int x, int startY,
                const int b[ROWS][COLS]) const {
        int y = startY;
        while (!collidesOn(k, r, x, y + 1, b)) ++y;
        return y;
    }

    bool tryMoveDown() {
        if (collides(kind, rot, px, py + 1)) return false;
        ++py;
        return true;
    }

    ActionResult movePiece(int dx, int dy) {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        if (collides(kind, rot, px + dx, py + dy)) {
            result.message = "Blocked";
            return result;
        }
        px += dx;
        py += dy;
        ghostY = dropY(kind, rot, px);
        result.success = true;
        result.message = "Piece at column " + std::to_string(px);
        return result;
    }

    ActionResult doSoftDrop() {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        if (tryMoveDown()) {
            score += 1;
            result.success = true;
            result.message = "Dropped to row " + std::to_string(py);
        } else {
            lockPiece();
            result.success = gameRunning;  // false if the lock ended the game
            if (gameRunning) result.message = "Locked";
        }
        return result;
    }

    ActionResult doHardDrop() {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        const int startY = py;
        while (!collides(kind, rot, px, py + 1)) {
            ++py;
            score += 2;  // guideline hard-drop scoring
        }
        setMessage("Dropped " + std::to_string(py - startY) + " rows");
        lockPiece();
        result.success = gameRunning;  // false if the lock ended the game
        if (gameRunning) result.message = "Piece locked";
        return result;
    }

    ActionResult doRotate(int dir) {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        // Only adjacent 90-degree transitions are kick-tabled (SRS); a 180
        // turn happens as two successive 90-degree steps with kicks.
        const int from = rot;
        const int to = (from + dir + 4) % 4;
        if (kind == PIECE_O) {
            result.success = true;
            result.message = "O piece does not rotate";
            return result;
        }
        const bool isI = (kind == PIECE_I);
        const int (*kicks)[2] = isI ? I_KICKS[from][to] : JLSTZ_KICKS[from][to];
        for (int t = 0; t < 5; ++t) {
            const int kx = kicks[t][0], ky = kicks[t][1];
            if (!collides(kind, to, px + kx, py + ky)) {
                px += kx;
                py += ky;
                rot = to;
                ghostY = dropY(kind, rot, px);
                result.success = true;
                result.message = "Rotated (wall kick " + std::to_string(t) + ")";
                sfx.play(uj::Sfx::Ping);
                return result;
            }
        }
        result.message = "Rotation blocked";
        return result;
    }

    ActionResult doHold() {
        ActionResult result;
        result.success = false;
        if (!gameRunning) {
            result.message = "Game not running (restart to play)";
            return result;
        }
        if (!canHold) {
            result.message = "Hold already used for this piece";
            return result;
        }
        const int oldKind = kind;
        if (heldKind == -1) {
            heldKind = oldKind;
            spawnFromQueue();
        } else {
            kind = heldKind;
            heldKind = oldKind;
            rot = 0;
            px = 3;
            py = 0;
            if (collides(kind, rot, px, py)) {
                triggerGameOver();
            } else {
                ghostY = dropY(kind, rot, px);
            }
        }
        canHold = false;
        sfx.play(uj::Sfx::Coin);
        result.success = true;
        result.message = "Held " + std::to_string(oldKind);
        return result;
    }

    // ---- Bag / spawn / lock ---------------------------------------------------
    void refillBag() {
        std::vector<int> bag7 = {0, 1, 2, 3, 4, 5, 6};
        for (int i = 6; i > 0; --i) {
            const int j = static_cast<int>(lcgNext() % (i + 1));
            std::swap(bag7[static_cast<size_t>(i)], bag7[static_cast<size_t>(j)]);
        }
        bag.insert(bag.end(), bag7.begin(), bag7.end());
    }

    void spawnNext() {
        canHold = true;
        spawnFromQueue();
    }

    void spawnFromQueue() {
        if (bag.size() < 5) refillBag();
        kind = bag.front();
        bag.pop_front();
        rot = 0;
        px = 3;
        py = 0;
        if (collides(kind, rot, px, py)) {
            setMessage("GAME OVER - Press R to restart");
            endGame();
        } else {
            ghostY = dropY(kind, rot, px);
        }
    }

    void lockPiece() {
        for (int i = 0; i < 4; ++i) {
            const int gx = px + SHAPES[kind][rot][i].dx;
            const int gy = py + SHAPES[kind][rot][i].dy;
            if (gy < 0) {  // locked above the visible field
                triggerGameOver();
                return;
            }
            board[gy][gx] = kind + 1;
        }
        sfx.play(uj::Sfx::Thock);   // the piece settles
        clearLines();
        spawnNext();
    }

    void clearLines() {
        int cleared = 0;
        std::vector<int> clearedRows;   // rows before the shift (for the flash)
        for (int r = ROWS - 1; r >= 0;) {
            bool full = true;
            for (int c = 0; c < COLS; ++c) {
                if (board[r][c] == 0) { full = false; break; }
            }
            if (full) {
                clearedRows.push_back(r);
                for (int rr = r; rr > 0; --rr)
                    for (int c = 0; c < COLS; ++c)
                        board[rr][c] = board[rr - 1][c];
                for (int c = 0; c < COLS; ++c) board[0][c] = 0;
                ++cleared;
            } else {
                --r;
            }
        }
        if (cleared > 0) {
            score += CLEAR_POINTS[cleared] * level;
            lines += cleared;
            level = lines / 10 + 1;
            const char* names[5] = {"", "Single", "Double", "Triple", "TETRIS!"};
            setMessage(std::string(names[cleared]) + " cleared - " +
                       std::to_string(lines) + " lines total");

            // ---- Juice: line-clear flash, shake, hit-stop, score pop -------
            for (int row : clearedRows) {
                for (int c = 0; c < COLS; ++c) {
                    particles.burst((c + 0.5f) * TILE, (row + 0.5f) * TILE, 2,
                                    PIECE_COLORS[c % PIECE_COUNT],
                                    6.0f, 0.4f, 4.0f);
                }
            }
            shake.add(0.25f + 0.10f * static_cast<float>(cleared));
            hitStop.trigger(0.05f + 0.04f * static_cast<float>(cleared));
            sfx.play(cleared == 4 ? uj::Sfx::Win : uj::Sfx::Clear);
            const int fx = COLS * TILE / 2 - 40;
            const int fy = (clearedRows.empty() ? ROWS / 2 : clearedRows[0] + 1) * TILE;
            floatTexts.spawn(std::make_shared<TextDisplay>(
                fx, fy, std::string(names[cleared]) + " +" +
                std::to_string(CLEAR_POINTS[cleared] * level)), fx, fy);
            updateHUD();
        }
    }

    // ---- Rendering --------------------------------------------------------------
    void syncGrid() {
        if (!grid) return;
        // The LLM sees the settled board (values 1..7 = piece type) plus the
        // live piece added below, so an agent sees it mid-fall. The grid is
        // no longer the renderer (the playfield draws manually so it can
        // shake), so only the values matter here.
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c)
                grid->setValue(c, r, board[r][c]);
        for (int i = 0; i < 4; ++i) {
            const int gx = px + SHAPES[kind][rot][i].dx;
            const int gy = py + SHAPES[kind][rot][i].dy;
            if (gy < 0 || gy >= ROWS) continue;
            grid->setValue(gx, gy, kind + 1);
        }
    }

    void drawPanel() {
        SDL_Renderer* sdl = getRenderer()->renderer;
        // Hold preview.
        if (heldKind >= 0) {
            drawMiniPiece(sdl, 270, 275, heldKind, 12);
        } else {
            SDL_Rect box = {270, 275, 4 * 12, 4 * 12};
            SDL_SetRenderDrawColor(sdl, 60, 66, 82, 255);
            SDL_RenderDrawRect(sdl, &box);
        }
        // Next three previews.
        for (int n = 0; n < 3; ++n) {
            if (bag.size() > static_cast<size_t>(n)) {
                drawMiniPiece(sdl, 270, 380 + n * 52, bag[n], 12);
            }
        }
    }

    void drawMiniPiece(SDL_Renderer* sdl, int ox, int oy, int k, int scale) {
        SDL_SetRenderDrawColor(sdl, PIECE_COLORS[k].r, PIECE_COLORS[k].g,
                               PIECE_COLORS[k].b, 255);
        for (int i = 0; i < 4; ++i) {
            SDL_Rect cell = {ox + SHAPES[k][0][i].dx * scale,
                             oy + SHAPES[k][0][i].dy * scale, scale, scale};
            SDL_RenderFillRect(sdl, &cell);
        }
    }

    void drawBlock(SDL_Renderer* sdl, int x, int y, SDL_Color col) {
        SDL_Rect cell = {x, y, TILE, TILE};
        SDL_SetRenderDrawColor(sdl, col.r, col.g, col.b, 255);
        SDL_RenderFillRect(sdl, &cell);
        SDL_SetRenderDrawColor(sdl, 16, 18, 26, 255);
        SDL_RenderDrawRect(sdl, &cell);
    }

    // ---- Feel ----------------------------------------------------------------
    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
    }

    // ---- Smoke autopilot ---------------------------------------------------------
    // Greedy one-step solver: try every rotation x column, score the locked
    // result by (cleared lines, stack height, holes, bumpiness), then apply
    // the best placement. Simple enough to be deterministic and fast, smart
    // enough to clear lines in a headless run.
    struct Placement {
        int rot = 0, x = 3, y = 0, score = -1000000;
    };

    void autoplayStep() {
        // One-ply lookahead: score a placement of the current piece by the
        // best board we can reach after ALSO placing the next known piece
        // from the bag. A greedy one-step search always stacks the first
        // piece into a corner and never clears a line; the lookahead lets
        // the bot keep its options open (e.g. not cornering the O piece).
        const int nextKind = bag.empty() ? -1 : bag[0];
        Placement best;
        for (int r = 0; r < 4; ++r) {
            if (kind == PIECE_O && r != 0) continue;
            for (int x = 0; x < COLS; ++x) {
                const int y = dropY(kind, r, x);
                // dropY returns py (no drop) when the piece cannot sit at
                // this column at all - skip such placements.
                if (collides(kind, r, x, y)) continue;

                int b1[ROWS][COLS];
                for (int rr = 0; rr < ROWS; ++rr)
                    for (int c = 0; c < COLS; ++c)
                        b1[rr][c] = board[rr][c];
                lockOn(b1, kind, r, x, y);
                const int cleared1 = clearLinesOn(b1);

                int sc = 0;
                if (nextKind >= 0) {
                    int best2 = -1000000;
                    for (int r2 = 0; r2 < 4; ++r2) {
                        if (nextKind == PIECE_O && r2 != 0) continue;
                        for (int x2 = 0; x2 < COLS; ++x2) {
                            const int y2 = dropYOn(nextKind, r2, x2, 0, b1);
                            if (collidesOn(nextKind, r2, x2, y2, b1)) continue;
                            int b2[ROWS][COLS];
                            for (int rr = 0; rr < ROWS; ++rr)
                                for (int c = 0; c < COLS; ++c)
                                    b2[rr][c] = b1[rr][c];
                            lockOn(b2, nextKind, r2, x2, y2);
                            const int cleared2 = clearLinesOn(b2);
                            const int sc2 = cleared2 * 1000 + evalBoard(b2);
                            if (sc2 > best2) best2 = sc2;
                        }
                    }
                    sc = best2;
                } else {
                    sc = cleared1 * 1000 + evalBoard(b1);
                }
                if (sc > best.score) best = {r, x, y, sc};
            }
        }
        // Achieve the best placement: rotate (kicks allowed), move, drop.
        if (kind != PIECE_O && best.rot != rot) {
            const int steps = (best.rot - rot + 4) % 4;
            for (int s = 0; s < steps; ++s) {
                (void)doRotate(1);
                if (!gameRunning) return;
            }
        }
        while (px < best.x) {
            if (!movePiece(1, 0).success) break;
        }
        while (px > best.x) {
            if (!movePiece(-1, 0).success) break;
        }
        (void)doHardDrop();
    }

    // Lock a piece onto a scratch board (no bounds check needed - callers
    // only pass placements that passed collidesOn).
    static void lockOn(int b[ROWS][COLS], int k, int r, int x, int y) {
        for (int i = 0; i < 4; ++i) {
            const int gx = x + SHAPES[k][r][i].dx;
            const int gy = y + SHAPES[k][r][i].dy;
            if (gy >= 0 && gx >= 0 && gx < COLS && gy < ROWS) b[gy][gx] = 1;
        }
    }

    // Remove full rows from a scratch board; returns how many were cleared.
    static int clearLinesOn(int b[ROWS][COLS]) {
        int cleared = 0;
        for (int rr = ROWS - 1; rr >= 0;) {
            bool full = true;
            for (int c = 0; c < COLS; ++c) {
                if (b[rr][c] == 0) { full = false; break; }
            }
            if (full) {
                for (int r2 = rr; r2 > 0; --r2)
                    for (int c = 0; c < COLS; ++c)
                        b[r2][c] = b[r2 - 1][c];
                for (int c = 0; c < COLS; ++c) b[0][c] = 0;
                ++cleared;
            } else {
                --rr;
            }
        }
        return cleared;
    }

    // Board-quality evaluation of a (post-clear) board: penalize aggregate
    // height, holes, and bumpiness. Callers add cleared*1000 for lines.
    static int evalBoard(const int b[ROWS][COLS]) {
        int aggH = 0, holes = 0, bump = 0;
        int colH[COLS];
        for (int c = 0; c < COLS; ++c) {
            int h = 0;
            for (int rr = ROWS - 1; rr >= 0; --rr) {
                if (b[rr][c] != 0) { h = rr + 1; break; }
            }
            colH[c] = h;
            aggH += h;
        }
        for (int c = 0; c < COLS; ++c) {
            for (int rr = 0; rr < colH[c]; ++rr) {
                if (b[rr][c] == 0) ++holes;
            }
            if (c > 0) bump += std::abs(colH[c] - colH[c - 1]);
        }
        return -aggH * 4 - holes * 15 - bump;
    }

    // ---- Misc ----------------------------------------------------------------------
    int stackHeight() const {
        int h = 0;
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c)
                if (board[r][c] != 0) { h = ROWS - r; return h; }
        return h;
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    // Shared game-over path: bank the session best and give it a lose beat.
    void triggerGameOver() {
        bestScore = std::max(bestScore, score);
        sfx.play(uj::Sfx::Lose);
        shake.add(0.55f);
        hitStop.trigger(0.12f);
        setMessage("GAME OVER - Best " + std::to_string(bestScore) +
                   " - Press R to restart");
        endGame();
    }

    void updateHUD() {
        if (scoreText) scoreText->setText("Score  " + std::to_string(score) +
                                          "    Best " +
                                          std::to_string(std::max(bestScore, score)));
        if (linesText) linesText->setText("Lines  " + std::to_string(lines));
        if (levelText) levelText->setText("Level  " + std::to_string(level));
    }
};

// SRS piece definitions (see the header comment for the layout convention).
const Tetris::Cell Tetris::SHAPES[Tetris::PIECE_COUNT][4][4] = {
    // I
    { {{0,1},{1,1},{2,1},{3,1}},
      {{2,0},{2,1},{2,2},{2,3}},
      {{0,2},{1,2},{2,2},{3,2}},
      {{1,0},{1,1},{1,2},{1,3}} },
    // O
    { {{0,0},{1,0},{0,1},{1,1}},
      {{0,0},{1,0},{0,1},{1,1}},
      {{0,0},{1,0},{0,1},{1,1}},
      {{0,0},{1,0},{0,1},{1,1}} },
    // T
    { {{1,0},{0,1},{1,1},{2,1}},
      {{1,0},{1,1},{2,1},{1,2}},
      {{0,1},{1,1},{2,1},{1,2}},
      {{1,0},{0,1},{1,1},{1,2}} },
    // S
    { {{1,0},{2,0},{0,1},{1,1}},
      {{1,0},{1,1},{2,1},{2,2}},
      {{1,1},{2,1},{0,2},{1,2}},
      {{0,0},{0,1},{1,1},{1,2}} },
    // Z
    { {{0,0},{1,0},{1,1},{2,1}},
      {{2,0},{1,1},{2,1},{1,2}},
      {{0,1},{1,1},{1,2},{2,2}},
      {{1,0},{0,1},{1,1},{0,2}} },
    // J
    { {{0,0},{0,1},{1,1},{2,1}},
      {{1,0},{2,0},{1,1},{1,2}},
      {{0,1},{1,1},{2,1},{2,2}},
      {{1,0},{1,1},{0,2},{1,2}} },
    // L
    { {{2,0},{0,1},{1,1},{2,1}},
      {{1,0},{1,1},{1,2},{2,2}},
      {{0,1},{1,1},{2,1},{0,2}},
      {{0,0},{1,0},{1,1},{1,2}} },
};

// Only the 8 adjacent 90-degree transitions are specified by SRS; the
// non-adjacent entries are zero-filled and never read (180 turns happen as
// two successive 90-degree steps).
const int Tetris::JLSTZ_KICKS[4][4][5][2] = {
    // from 0
    { {{0,0},{0,0},{0,0},{0,0},{0,0}},              // 0->0 (unused)
      {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}},         // 0->R
      {{0,0},{0,0},{0,0},{0,0},{0,0}},              // 0->2 (unused)
      {{0,0},{1,0},{1,1},{0,-2},{1,-2}} },          // 0->L
    // from R
    { {{0,0},{1,0},{1,-1},{0,2},{1,2}},             // R->0
      {{0,0},{0,0},{0,0},{0,0},{0,0}},              // R->R (unused)
      {{0,0},{1,0},{1,-1},{0,2},{1,2}},             // R->2
      {{0,0},{0,0},{0,0},{0,0},{0,0}} },            // R->L (unused)
    // from 2
    { {{0,0},{0,0},{0,0},{0,0},{0,0}},              // 2->0 (unused)
      {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}},         // 2->R
      {{0,0},{0,0},{0,0},{0,0},{0,0}},              // 2->2 (unused)
      {{0,0},{1,0},{1,1},{0,-2},{1,-2}} },          // 2->L
    // from L
    { {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}},         // L->0
      {{0,0},{0,0},{0,0},{0,0},{0,0}},              // L->R (unused)
      {{0,0},{-1,0},{-1,-1},{0,2},{1,2}},           // L->2
      {{0,0},{0,0},{0,0},{0,0},{0,0}} },            // L->L (unused)
};

const int Tetris::I_KICKS[4][4][5][2] = {
    { {},
      {{0,0},{-2,0},{1,0},{-2,-1},{1,2}},
      {{0,0},{-1,0},{2,0},{-1,2},{2,-1}},
      {{0,0},{2,0},{-1,0},{2,1},{-1,-2}} },
    { {{0,0},{2,0},{-1,0},{2,1},{-1,-2}},
      {},
      {{0,0},{-1,0},{2,0},{-1,2},{2,-1}},
      {{0,0},{1,0},{-2,0},{1,-2},{-2,1}} },
    { {{0,0},{1,0},{-2,0},{1,-2},{-2,1}},
      {{0,0},{2,0},{-1,0},{2,1},{-1,-2}},
      {},
      {{0,0},{2,0},{-1,0},{2,1},{-1,-2}} },
    { {{0,0},{-2,0},{1,0},{-2,-1},{1,2}},
      {{0,0},{1,0},{-2,0},{1,-2},{-2,1}},
      {{0,0},{-1,0},{2,0},{-1,2},{2,-1}},
      {} },
};

const SDL_Color Tetris::PIECE_COLORS[Tetris::PIECE_COUNT] = {
    {60, 220, 240, 255},   // I cyan
    {240, 220, 60, 255},   // O yellow
    {200, 90, 230, 255},   // T purple
    {90, 220, 90, 255},    // S green
    {240, 80, 80, 255},    // Z red
    {90, 130, 240, 255},   // J blue
    {240, 150, 60, 255},   // L orange
};

const SDL_Color Tetris::GHOST_COLOR = {110, 124, 160, 255};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the Tetris class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static Tetris game;
#else
    Tetris game;
#endif
    game.run();
    return 0;
}
#endif
