// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Cinderfall - a top-down action-adventure roguelite. The flagship of the
// 100-game program; see DESIGN.md for the long-term plan and GAMES.md for
// the catalog.
//
// The simulation lives in CinderfallState.h (pure, unit-tested, headless).
// This file is only the shell: input -> Input -> state.step(), and state ->
// pixels. Every action a human takes is also an LLM action, and both funnel
// through the exact same state.step() call.
//
// Controls:
//   WASD / arrows  move          SPACE / J  sword swing
//   K / TAB        dodge roll    E          interact (doors/chests)
//   ENTER          start run     R          restart
//
// Headless smoke (PONG_SMOKE=1): an autopilot walks toward the stairs,
// swings periodically and rolls, so CI exercises movement, wall collision,
// combat, projectiles, contact damage, death and restart without a display.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"
#include "Games/Cinderfall/CinderfallState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr int kGridW = 35;
constexpr int kGridH = 35;
constexpr int kTile = 20;         // pixels per tile
constexpr int kHudH = 44;         // bottom HUD strip height (pixels)

SDL_Color col(Uint8 r, Uint8 g, Uint8 b) { return {r, g, b, 255}; }

// Palette (ember-on-ash).
const SDL_Color kFloorA = {24, 18, 32, 255};
const SDL_Color kFloorB = {27, 21, 36, 255};
const SDL_Color kWall = {54, 42, 68, 255};
const SDL_Color kWallEdge = {34, 26, 44, 255};
const SDL_Color kDoor = {52, 170, 180, 255};
const SDL_Color kStairs = {232, 226, 240, 255};
const SDL_Color kPlayer = {255, 148, 44, 255};
const SDL_Color kPlayerHit = {255, 220, 190, 255};
const SDL_Color kChaser = {214, 66, 84, 255};
const SDL_Color kSpitter = {120, 200, 92, 255};
const SDL_Color kBrute = {168, 84, 208, 255};
const SDL_Color kGhost = {208, 214, 236, 255};
const SDL_Color kTurret = {122, 108, 96, 255};
const SDL_Color kTurretCore = {255, 196, 84, 255};
const SDL_Color kGold = {246, 214, 74, 255};
const SDL_Color kHeart = {244, 92, 116, 255};
const SDL_Color kKey = {86, 200, 224, 255};
const SDL_Color kChest = {158, 104, 52, 255};
const SDL_Color kChestLid = {96, 62, 30, 255};
const SDL_Color kHud = {10, 8, 14, 255};

// 7x6 pixel heart, drawn at 2px per cell (14x12 on screen).
constexpr const char* kHeartGlyph[6] = {
    " xx xx ",
    "xxxxxxx",
    "xxxxxxx",
    " xxxxx ",
    "  xxx  ",
    "   x   ",
};

}  // namespace

class Cinderfall : public Game2D {
public:
    Cinderfall() : Game2D("Cinderfall", kGridW * kTile, kGridH * kTile + kHudH, kTile) {
        // PONG_SMOKE is the shared CI smoke flag (see GAME_DEV_GUIDE.md).
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    void initGame() override {
        // Reset every collection the base class owns (restart must not leak).
        entities.clear();
        textDisplays.clear();
        buttons.clear();
        statsDisplays.clear();

        state = cinderfall::State{};
        state.startTitle();
        llmWishX = llmWishY = 0.0f;
        llmWishTimer = 0.0f;
        smokeRun = 0u;
        smokeTimer = 0.0f;
        smokeRollTimer = 0.0f;

        // Start / restart are edge-triggered through the binding layer; the
        // continuous movement + combat keys are polled in updateGame().
        bindKey(KEY_ENTER).onPress([this]() {
            if (state.phase() == cinderfall::Phase::Title) state.newRun(freshSeed());
        });
        bindKey(KEY_R).onPress([this]() {
            if (state.phase() != cinderfall::Phase::Title) state.newRun(freshSeed());
        });

        // LLM actions mirror the keyboard exactly (same state.step() path).
        registerAction("move_up", [this]() { queueWish(0.0f, -1.0f); return ok(); });
        registerAction("move_down", [this]() { queueWish(0.0f, 1.0f); return ok(); });
        registerAction("move_left", [this]() { queueWish(-1.0f, 0.0f); return ok(); });
        registerAction("move_right", [this]() { queueWish(1.0f, 0.0f); return ok(); });
        registerAction("attack", [this]() { llmAttack = true; return ok(); });
        registerAction("roll", [this]() { llmRoll = true; return ok(); });
        registerAction("interact", [this]() { llmInteract = true; return ok(); });
        registerAction("start", [this]() {
            if (state.phase() == cinderfall::Phase::Title) state.newRun(freshSeed());
            return ok();
        });
        registerAction("restart", [this]() {
            state.newRun(freshSeed());
            return ok();
        });
    }

    GameState getState() const override {
        GameState s = Game2D::getState();
        s.gameRunning = (state.phase() == cinderfall::Phase::Playing);
        s.gameOver = (state.phase() == cinderfall::Phase::Dead);
        s.gameWon = (state.phase() == cinderfall::Phase::Won);
        s.level = state.floor();
        s.score = state.player().gold;
        s.message = state.message();
        s.stats["hp"] = state.player().hp;
        s.stats["max_hp"] = state.player().maxHp;
        s.stats["gold"] = state.player().gold;
        s.stats["keys"] = state.player().keys;
        s.stats["floor"] = state.floor();
        s.stats["player_x"] = (int)state.player().x;
        s.stats["player_y"] = (int)state.player().y;
        s.stats["enemies"] = (int)state.enemies().size();
        return s;
    }

    std::vector<std::string> getAvailableActions() const override {
        if (state.phase() == cinderfall::Phase::Title) return {"start"};
        if (state.phase() == cinderfall::Phase::Dead || state.phase() == cinderfall::Phase::Won) {
            return {"restart"};
        }
        return actionNames;
    }

    void updateGame(float dt) override {
        // ---- Headless smoke autopilot (PONG_SMOKE=1) ---------------------
        if (smokeMode) {
            if (state.phase() == cinderfall::Phase::Title) { state.newRun(++smokeRun); return; }
            if (state.phase() == cinderfall::Phase::Dead || state.phase() == cinderfall::Phase::Won) {
                state.newRun(++smokeRun);  // keep the loop alive across win/lose
                return;
            }
            cinderfall::Input in;
            const auto& pl = state.player();
            const auto st = state.stairs();
            in.moveX = (float)st.first + 0.5f - pl.x;
            in.moveY = (float)st.second + 0.5f - pl.y;
            smokeTimer += dt;
            smokeRollTimer += dt;
            if (smokeTimer > 0.45f) { in.attack = true; smokeTimer = 0.0f; }
            if (smokeRollTimer > 2.0f) { in.roll = true; smokeRollTimer = 0.0f; }
            state.step(dt, in);
            drainSfx();
            return;
        }

        // ---- Title / dead / won ------------------------------------------
        if (state.phase() != cinderfall::Phase::Playing) return;

        // ---- Build input from keyboard -----------------------------------
        cinderfall::Input in;
        if (input.isKeyHeld(KEY_W) || input.isKeyHeld(KEY_UP)) in.moveY -= 1.0f;
        if (input.isKeyHeld(KEY_S) || input.isKeyHeld(KEY_DOWN)) in.moveY += 1.0f;
        if (input.isKeyHeld(KEY_A) || input.isKeyHeld(KEY_LEFT)) in.moveX -= 1.0f;
        if (input.isKeyHeld(KEY_D) || input.isKeyHeld(KEY_RIGHT)) in.moveX += 1.0f;
        if (input.wasKeyJustPressed(KEY_SPACE) || input.wasKeyJustPressed((KeyCode)SDL_SCANCODE_J)) in.attack = true;
        if (input.wasKeyJustPressed((KeyCode)SDL_SCANCODE_K) || input.wasKeyJustPressed(KEY_TAB)) in.roll = true;
        if (input.wasKeyJustPressed((KeyCode)SDL_SCANCODE_E)) in.interact = true;

        // ---- Merge queued LLM wishes (same code path as the keyboard) ----
        if (llmWishTimer > 0.0f) {
            in.moveX = llmWishX;
            in.moveY = llmWishY;
            llmWishTimer -= dt;
        }
        if (llmAttack) { in.attack = true; llmAttack = false; }
        if (llmRoll) { in.roll = true; llmRoll = false; }
        if (llmInteract) { in.interact = true; llmInteract = false; }

        state.step(dt, in);
        drainSfx();
    }

    // Sound events are queued by the pure simulation; the shell turns them
    // into actual audio once per frame (works natively, in WASM, and no-ops
    // gracefully when no audio device is available, e.g. headless CI).
    void drainSfx() {
        // cinderfall::Sfx shares the first 8 values of the shared uj::Sfx
        // vocabulary (Swing..Win) in the same order, so the cast is exact.
        for (const auto e : state.takeSfx()) sfx.play(static_cast<uj::Sfx>(e));
    }

    void renderGame() override {
        Renderer* r = getRenderer();
        if (!r) return;
        SDL_Renderer* s = r->renderer;

        // Screen shake: trauma^2 gives a punchy, quickly-fading offset.
        const float tr = state.trauma();
        const float mag = tr * tr * 9.0f;
        shakeX_ = (int)(std::sin(getGameTime() * 71.0f) * mag);
        shakeY_ = (int)(std::cos(getGameTime() * 53.0f) * mag);

        if (state.phase() == cinderfall::Phase::Title) {
            renderTitle(s);
            return;
        }

        renderWorld(s);
        renderParticles(s);
        renderPickups(s);
        renderChests(s);
        renderEnemies(s);
        renderPlayer(s);
        renderHUD(s);

        if (state.phase() == cinderfall::Phase::Dead) {
            renderCenter(s, "YOU DIED", "Press R to restart");
        } else if (state.phase() == cinderfall::Phase::Won) {
            renderCenter(s, "YOU WIN", "The Emberforge is rekindled - press R");
        }
    }

private:
    // ---- helpers ---------------------------------------------------------
    void fillRect(SDL_Renderer* s, int x, int y, int w, int h, SDL_Color c) {
        SDL_SetRenderDrawColor(s, c.r, c.g, c.b, c.a);
        SDL_Rect rect{x, y, w, h};
        SDL_RenderFillRect(s, &rect);
    }

    void outlineRect(SDL_Renderer* s, int x, int y, int w, int h, SDL_Color c) {
        SDL_SetRenderDrawColor(s, c.r, c.g, c.b, c.a);
        SDL_Rect rect{x, y, w, h};
        SDL_RenderDrawRect(s, &rect);
    }

    uint32_t freshSeed() const { return (uint32_t)SDL_GetTicks() ^ 0x9E3779B9u; }

    void queueWish(float x, float y) {
        llmWishX = x;
        llmWishY = y;
        llmWishTimer = 0.3f;  // an LLM "move" steers for a short burst
    }

    ActionResult ok() {
        return {true, state.message(), 0, state.phase() == cinderfall::Phase::Dead,
                state.phase() == cinderfall::Phase::Won};
    }

    // Draw a top-down "sprite": body square + dark border + two eye pixels
    // toward the facing direction. Flash swaps in a light tint (hit/invuln).
    void drawActor(SDL_Renderer* s, float cx, float cy, SDL_Color body, float fx, float fy, bool flash) {
        const int size = kTile - 6;
        const int px = (int)(cx * (float)kTile) - size / 2 + shakeX_;
        const int py = (int)(cy * (float)kTile) - size / 2 + shakeY_;
        fillRect(s, px, py, size, size, flash ? kPlayerHit : body);
        outlineRect(s, px, py, size, size, kWallEdge);
        // Eyes (2x2 px) on the facing side.
        int ex = px + size / 2 + (int)(fx * (float)(size / 2 - 4));
        int ey = py + size / 2 + (int)(fy * (float)(size / 2 - 4));
        fillRect(s, ex - 3, ey - 2, 2, 2, col(240, 240, 240));
        fillRect(s, ex + 1, ey - 2, 2, 2, col(240, 240, 240));
    }

    void drawHeart(SDL_Renderer* s, int px, int py, SDL_Color c, int scale = 2) {
        for (int y = 0; y < 6; ++y) {
            for (int x = 0; x < 7; ++x) {
                if (kHeartGlyph[y][x] == 'x') fillRect(s, px + x * scale, py + y * scale, scale, scale, c);
            }
        }
    }

    // ---- rendering layers -------------------------------------------------
    void renderWorld(SDL_Renderer* s) {
        for (int y = 0; y < state.height(); ++y) {
            for (int x = 0; x < state.width(); ++x) {
                const int px = x * kTile + shakeX_;
                const int py = y * kTile + shakeY_;
                if (state.isWall(x, y)) {
                    fillRect(s, px, py, kTile, kTile, kWall);
                    fillRect(s, px, py, kTile, 1, kWallEdge);
                } else if (state.isDoor(x, y)) {
                    fillRect(s, px, py, kTile, kTile, kWall);
                    fillRect(s, px + 2, py + 2, kTile - 4, kTile - 4, kDoor);
                } else {
                    fillRect(s, px, py, kTile, kTile, ((x + y) & 1) ? kFloorA : kFloorB);
                }
            }
        }
        const auto st = state.stairs();
        if (st.first >= 0) {
            const int px = st.first * kTile + shakeX_;
            const int py = st.second * kTile + shakeY_;
            fillRect(s, px, py, kTile, kTile, kFloorA);
            for (int i = 0; i < 3; ++i) {
                fillRect(s, px + 2, py + 2 + i * 6, kTile - 4, 3, kStairs);
            }
        }
    }

    void renderParticles(SDL_Renderer* s) {
        for (const auto& pt : state.particles()) {
            const float k = std::max(0.0f, pt.life / pt.maxLife);  // 1..0 fade
            SDL_Color c = kGold;
            if (pt.kind == cinderfall::ParticleKind::Blood) c = kChaser;
            else if (pt.kind == cinderfall::ParticleKind::Ember) {
                c = {255, (Uint8)(90 + (int)(120 * k)), 40, 255};
            }
            const int sz = pt.size + (int)(k * 2.0f);
            fillRect(s, (int)(pt.x * (float)kTile) - sz / 2 + shakeX_,
                     (int)(pt.y * (float)kTile) - sz / 2 + shakeY_, sz, sz, c);
        }
    }

    void renderPickups(SDL_Renderer* s) {
        for (const auto& pk : state.pickups()) {
            const int cx = pk.x * kTile + kTile / 2 + shakeX_;
            const int cy = pk.y * kTile + kTile / 2 + shakeY_;
            SDL_Color c = kGold;
            if (pk.kind == cinderfall::PickupKind::Heart) c = kHeart;
            else if (pk.kind == cinderfall::PickupKind::Key) c = kKey;
            if (pk.kind == cinderfall::PickupKind::Heart) {
                drawHeart(s, cx - 7, cy - 6, c);
            } else {
                fillRect(s, cx - 3, cy - 3, 6, 6, c);
                outlineRect(s, cx - 3, cy - 3, 6, 6, kWallEdge);
            }
        }
    }

    void renderChests(SDL_Renderer* s) {
        for (const auto& c : state.chests()) {
            const int px = c.x * kTile + 3 + shakeX_;
            const int py = c.y * kTile + 5 + shakeY_;
            fillRect(s, px, py, kTile - 6, kTile - 8, c.opened ? kChestLid : kChest);
            outlineRect(s, px, py, kTile - 6, kTile - 8, kWallEdge);
            fillRect(s, px, py + (kTile - 8) / 2, kTile - 6, 2, kChestLid);
        }
    }

    void renderEnemies(SDL_Renderer* s) {
        for (const auto& e : state.enemies()) {
            SDL_Color body = kChaser;
            if (e.kind == cinderfall::EnemyKind::Spitter) body = kSpitter;
            else if (e.kind == cinderfall::EnemyKind::Brute) body = kBrute;
            else if (e.kind == cinderfall::EnemyKind::Ghost) body = kGhost;
            else if (e.kind == cinderfall::EnemyKind::Turret) body = kTurret;
            const float dx = state.player().x - e.x;
            const float dy = state.player().y - e.y;
            if (e.kind == cinderfall::EnemyKind::Ghost) {
                // Pale + slow flicker: reads as "not quite solid".
                const bool blink = ((int)(getGameTime() * 9.0f) & 1) == 0;
                drawActor(s, e.x, e.y, blink ? body : kWallEdge, dx, dy, e.hitFlash > 0.0f);
            } else {
                drawActor(s, e.x, e.y, body, dx, dy, e.hitFlash > 0.0f);
            }
            if (e.kind == cinderfall::EnemyKind::Turret) {
                // Ember core: brightens just before each volley shot.
                const bool hot = e.volleyShots > 0 && e.volleyTimer < 0.06f;
                fillRect(s, (int)(e.x * (float)kTile) - 3 + shakeX_,
                         (int)(e.y * (float)kTile) - 3 + shakeY_, 6, 6,
                         hot ? kTurretCore : kWallEdge);
            }
        }
        for (const auto& pr : state.projectiles()) {
            fillRect(s, (int)(pr.x * (float)kTile) - 2 + shakeX_, (int)(pr.y * (float)kTile) - 2 + shakeY_, 5, 5, kStairs);
        }
    }

    void renderPlayer(SDL_Renderer* s) {
        const auto& p = state.player();
        const bool flash = p.hitFlash > 0.0f || (p.invulnTimer > 0.0f && ((int)(p.invulnTimer * 12.0f) & 1));
        drawActor(s, p.x, p.y, kPlayer, p.facingX, p.facingY, flash);
    }

    void renderHUD(SDL_Renderer* s) {
        const int hy = kGridH * kTile;
        fillRect(s, 0, hy, kGridW * kTile, kHudH, kHud);
        // Hearts.
        const auto& p = state.player();
        for (int i = 0; i < p.maxHp; ++i) {
            const int hx = 8 + i * 16;
            if (i < p.hp) drawHeart(s, hx, hy + 8, kHeart);
            else drawHeart(s, hx, hy + 8, kWallEdge);
        }
        // Gold / floor / keys.
        const std::string info = "Gold " + std::to_string(p.gold) + "    Floor " +
                                 std::to_string(state.floor()) + "/" + std::to_string(state.maxFloors()) +
                                 "    Keys " + std::to_string(p.keys);
        drawSimpleText(s, 8 + p.maxHp * 16 + 8, hy + 10, info, kStairs, 0.9f);
        // Message line.
        drawSimpleText(s, 8, hy + 30, state.message(), kPlayer, 0.8f);
    }

    void renderTitle(SDL_Renderer* s) {
        fillRect(s, 0, 0, kGridW * kTile, kGridH * kTile + kHudH, kFloorA);
        drawSimpleText(s, kGridW * kTile / 2 - 100, 220, "CINDERFALL", kPlayer, 2.0f);
        drawSimpleText(s, kGridW * kTile / 2 - 120, 270, "Descend. Rekindle the forge.", kStairs, 0.9f);
        drawSimpleText(s, kGridW * kTile / 2 - 80, 320, "Press ENTER to begin", kGold, 0.9f);
        drawSimpleText(s, kGridW * kTile / 2 - 110, 350, "WASD move - SPACE attack - K roll - E interact", kStairs, 0.7f);
    }

    void renderCenter(SDL_Renderer* s, const std::string& big, const std::string& small) {
        const int cy = kGridH * kTile / 2;
        fillRect(s, kGridW * kTile / 2 - 120, cy - 40, 240, 90, kHud);
        outlineRect(s, kGridW * kTile / 2 - 120, cy - 40, 240, 90, kPlayer);
        drawSimpleText(s, kGridW * kTile / 2 - 45, cy - 20, big, kPlayer, 1.4f);
        drawSimpleText(s, kGridW * kTile / 2 - 85, cy + 20, small, kStairs, 0.8f);
    }

    // ---- state ------------------------------------------------------------
    cinderfall::State state;
    float llmWishX = 0.0f;
    float llmWishY = 0.0f;
    float llmWishTimer = 0.0f;
    bool llmAttack = false;
    bool llmRoll = false;
    bool llmInteract = false;
    uint32_t smokeRun = 0;
    float smokeTimer = 0.0f;
    float smokeRollTimer = 0.0f;
    int shakeX_ = 0;
    int shakeY_ = 0;
    uj::SfxSynth sfx;
};

// UMBRA_GAME_NO_MAIN lets future headless tests include this file to reach
// the Cinderfall class without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
#ifdef __EMSCRIPTEN__
    static Cinderfall game;
#else
    Cinderfall game;
#endif
    game.run();
    return 0;
}
#endif
