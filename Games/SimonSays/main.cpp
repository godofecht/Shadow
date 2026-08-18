// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
//
// Simon Says - the memory classic, game #15 of the 100-game program.
//
// Four colored tiles (red, blue, green, yellow) light up in a growing random
// sequence, each with its own musical note. Watch the sequence, then repeat
// it exactly: one tile at a time, in order. A wrong tile ends the run; a
// correct full replay grows the sequence by one. Reach round 10 to win.
//
// Shipped with the GameJuice kit from day one (Engine/Core/GameJuice.h):
// every flash glows the tile with its note and a spark, a completed round
// pays a rising "+N" with the clear fanfare, a wrong press slams the board
// with shake + hit-stop + the falling tone (and a gold "NEW BEST!"
// celebration on a session record), and a win fires a confetti fanfare - all
// sound synthesized in memory, identical native / WASM / headless. The four
// tiles use the kit's new Note1..Note4 musical pitches (A, C, E, G). Pause
// (P) and a session best score included.
//
// One code path serves human input and the LLM: mouse clicks, the 1-4 keys,
// and the press_red/blue/green/yellow actions all call the exact pressTile().
// getState() exposes the sequence itself (seq_0..seq_n-1) plus the input
// progress, so an LLM reads the flashes and replays them like a human.
//
// The smoke autopilot has perfect recall: it reads the current sequence and
// presses the exact next required tile, so it replays every round headlessly
// and wins at round 10 (then the smoke loop restarts).
//
// Controls: click a tile (or keys 1-4) | P pause | R restart.

#include "Engine/Core/Game2D.h"
#include "Engine/Core/GameJuice.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class SimonSays : public Game2D {
    // ---- Tunables ---------------------------------------------------------
    static constexpr int WINDOW_W = 640;
    static constexpr int WINDOW_H = 640;
    static constexpr int TOP_BAND = 36;        // HUD strip
    static constexpr int WIN_ROUND = 10;       // rounds to beat the game
    static constexpr int TILE_COUNT = 4;
    static constexpr float FLASH_ON = 0.32f;   // a tile lights up
    static constexpr float FLASH_GAP = 0.18f;  // gap between flashes
    static constexpr float BOT_INTERVAL = 0.12f; // autopilot press cadence

    // ---- Feel (GameJuice) --------------------------------------------------
    uj::ParticleSystem particles;
    uj::ScreenShake shake;
    uj::HitStop hitStop;
    uj::SfxSynth sfx;
    uj::FloatingText floatTexts;
    bool paused = false;

    // ---- State ------------------------------------------------------------
    std::shared_ptr<TextDisplay> hud;
    std::shared_ptr<TextDisplay> message;
    std::vector<int> sequence;    // tile indices 0..3, oldest first
    int phase = 0;                // 0 = showing, 1 = input
    int inputIndex = 0;           // how many tiles the player has repeated
    int score = 0;
    int bestScore = 0;            // session best; survives restarts
    int litTile = -1;             // tile currently lit during playback
    int pressFlashTile = -1;      // tile just pressed (brief white flash)
    float pressFlashTimer = 0.0f;
    std::size_t showIndex = 0;    // position in the sequence being shown
    float phaseTimer = 0.0f;
    float inputCooldown = 0.0f;   // debounce between presses
    float botTimer = 0.0f;

    // Fixed-seed LCG: the sequence is deterministic and unit-testable.
    uint32_t lcgState = 0x51A0E5u;
    uint32_t lcgNext() {
        lcgState = lcgState * 1664525u + 1013904223u;
        return lcgState;
    }

    std::string statusText;

public:
    SimonSays() : Game2D("Simon Says", WINDOW_W, WINDOW_H, 20) {
        setSmokeMode(std::getenv("PONG_SMOKE") != nullptr);
    }

    // Public hook for headless tests (setSmokeMode is protected on Game2D).
    void enableAutoplay() { setSmokeMode(true); }

    // Test hook: complete rounds back-to-back until the win path fires.
    // Not used by gameplay.
    void forceWinForTest() {
        for (int i = 0; i < WIN_ROUND + 1 && gameRunning; ++i) {
            roundComplete();
        }
    }

    void initGame() override {
        score = 0;
        paused = false;
        inputIndex = 0;
        inputCooldown = 0.0f;
        botTimer = 0.0f;
        litTile = -1;
        pressFlashTile = -1;
        pressFlashTimer = 0.0f;
        particles.clear();
        floatTexts = uj::FloatingText{};

        // Round 1: a single random tile.
        sequence.clear();
        sequence.push_back(static_cast<int>(lcgNext() % TILE_COUNT));
        startPlayback();

        hud = createText(10, 8, "");
        hud->setColor({255, 255, 255, 255});
        message = createText(10, WINDOW_H - 22, "");
        message->setColor({255, 220, 120, 255});

        updateHUD();
        setMessage("Watch the sequence, then repeat it!");

        registerAction("press_red", [this]() { return pressAction(0); });
        registerAction("press_blue", [this]() { return pressAction(1); });
        registerAction("press_green", [this]() { return pressAction(2); });
        registerAction("press_yellow", [this]() { return pressAction(3); });
        registerAction("restart", [this]() {
            startGame();
            return ActionResult{true, "Restarted"};
        });

        bindMouse(MOUSE_LEFT).onPress([this]() { handleClick(); });
        bindKey(KEY_1).onPress([this]() { (void)pressAction(0); });
        bindKey(KEY_2).onPress([this]() { (void)pressAction(1); });
        bindKey(KEY_3).onPress([this]() { (void)pressAction(2); });
        bindKey(KEY_4).onPress([this]() { (void)pressAction(3); });
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

        if (phase == 0) {
            // Playback: light each tile in turn with its note.
            phaseTimer -= dt;
            if (phaseTimer <= 0.0f) {
                if (litTile >= 0) {
                    litTile = -1;
                    phaseTimer = FLASH_GAP;
                    if (showIndex >= sequence.size()) {
                        phase = 1;      // sequence shown; player's turn
                        inputIndex = 0;
                    }
                } else if (showIndex < sequence.size()) {
                    litTile = sequence[showIndex];
                    ++showIndex;
                    sfx.play(noteFor(litTile));
                    const auto [px, py] = tileCenter(litTile);
                    particles.burst((float)px, (float)py, 5,
                                    tileColor(litTile), 4.0f, 0.3f, 3.5f);
                    phaseTimer = FLASH_ON;
                } else {
                    phase = 1;
                    inputIndex = 0;
                }
            }
        } else if (phase == 1) {
            // Player's turn. The autopilot replays the sequence perfectly.
            if (smokeMode) {
                botTimer -= dt;
                if (botTimer <= 0.0f) {
                    botTimer = BOT_INTERVAL;
                    botStep();
                }
            }
            if (inputCooldown > 0.0f) inputCooldown -= dt;
        }

        updateFx(dt);
        updateHUD();
    }

    void renderGame() override {
        const auto [sx, sy] = shake.offset();
        SDL_Renderer* sdl = getRenderer()->renderer;

        // HUD band (stable).
        SDL_SetRenderDrawColor(sdl, 12, 14, 22, 255);
        SDL_Rect band = {0, 0, WINDOW_W, TOP_BAND};
        SDL_RenderFillRect(sdl, &band);

        // The four tiles (the world shakes).
        for (int t = 0; t < TILE_COUNT; ++t) {
            const int col = t % 2, row = t / 2;
            SDL_Rect r = {col * (WINDOW_W / 2) + 6 + sx,
                          TOP_BAND + row * ((WINDOW_H - TOP_BAND) / 2) + 6 + sy,
                          WINDOW_W / 2 - 12,
                          (WINDOW_H - TOP_BAND) / 2 - 12};
            const bool active = (litTile == t) ||
                (pressFlashTile == t && pressFlashTimer > 0.0f);
            const SDL_Color c = active ? tileBright(t) : tileColor(t);
            SDL_SetRenderDrawColor(sdl, c.r, c.g, c.b, 255);
            SDL_RenderFillRect(sdl, &r);
            SDL_SetRenderDrawColor(sdl, c.r / 2, c.g / 2, c.b / 2, 255);
            SDL_RenderDrawRect(sdl, &r);
        }

        particles.render(sdl, sx, sy);
        floatTexts.render(getRenderer());

        if (paused) {
            SDL_Rect veil = {0, 0, WINDOW_W, WINDOW_H};
            SDL_SetRenderDrawColor(sdl, 0, 0, 0, 140);
            SDL_RenderFillRect(sdl, &veil);
        }

        // Message strip at the bottom (stable).
        SDL_SetRenderDrawColor(sdl, 12, 14, 22, 180);
        SDL_Rect strip = {0, WINDOW_H - 30, WINDOW_W, 30};
        SDL_RenderFillRect(sdl, &strip);

        for (auto& t : textDisplays) t->render(getRenderer());
    }

    // ---- LLM state ----------------------------------------------------------
    GameState getState() const override {
        GameState state = Game2D::getState();
        state.score = score;
        state.level = static_cast<int>(sequence.size());
        state.message = statusText;
        state.stats["score"] = score;
        state.stats["best"] = std::max(bestScore, score);
        state.stats["round"] = static_cast<int>(sequence.size());
        state.stats["seq_len"] = static_cast<int>(sequence.size());
        state.stats["phase"] = phase;            // 0 showing, 1 input
        state.stats["input_index"] = inputIndex;
        state.stats["lit"] = litTile;
        state.stats["paused"] = paused ? 1 : 0;
        state.stats["frozen"] = hitStop.frozen() ? 1 : 0;
        state.stats["particles"] = particles.count();
        // The sequence itself: the exact tiles an agent must repeat.
        for (std::size_t i = 0; i < sequence.size(); ++i) {
            state.stats["seq_" + std::to_string(i)] = sequence[i];
        }
        return state;
    }

private:
    // ---- Sequence / playback ---------------------------------------------------
    void startPlayback() {
        phase = 0;
        showIndex = 0;
        litTile = -1;
        phaseTimer = 0.0f;
    }

    void roundComplete() {
        const int done = static_cast<int>(sequence.size());
        const int gain = done * 10;
        score += gain;
        // ---- Juice: round cleared -------------------------------------------
        sfx.play(uj::Sfx::Clear);
        shake.add(0.10f);
        particles.burst((float)(WINDOW_W / 2), (float)(TOP_BAND + 160), 10,
                        {230, 220, 180, 255}, 6.0f, 0.5f, 4.5f);
        floatTexts.spawn(std::make_shared<TextDisplay>(
            WINDOW_W / 2 - 30, TOP_BAND + 90,
            "+" + std::to_string(gain)),
            WINDOW_W / 2 - 30, TOP_BAND + 90);
        if (done >= WIN_ROUND) {
            winGame();
            return;
        }
        sequence.push_back(static_cast<int>(lcgNext() % TILE_COUNT));
        startPlayback();
        updateHUD();
        setMessage("Round " + std::to_string(sequence.size()) +
                   " - watch the sequence!");
    }

    // ---- Press handling (the shared code path) ----------------------------------
    void pressTile(int t) {
        if (!gameRunning || paused) return;
        if (phase != 1) return;                  // ignore presses during playback
        if (inputCooldown > 0.0f) return;        // debounce

        inputCooldown = 0.1f;
        pressFlashTile = t;
        pressFlashTimer = 0.15f;
        sfx.play(noteFor(t));
        const auto [px, py] = tileCenter(t);
        particles.burst((float)px, (float)py, 6, tileColor(t),
                        5.0f, 0.35f, 4.0f);

        if (t != sequence[static_cast<std::size_t>(inputIndex)]) {
            loseGame();
            return;
        }
        ++inputIndex;
        if (inputIndex >= static_cast<int>(sequence.size())) {
            roundComplete();
        }
        updateHUD();
    }

    // ---- Win / lose -------------------------------------------------------------
    void loseGame() {
        const bool newBest = score > bestScore;
        bestScore = std::max(bestScore, score);
        // ---- Juice: wrong press slams the board -------------------------------
        sfx.play(uj::Sfx::Lose);
        shake.add(0.6f);
        hitStop.trigger(0.15f);
        particles.burst((float)(WINDOW_W / 2), (float)(TOP_BAND + 160), 20,
                        {255, 60, 50, 255}, 10.0f, 0.6f, 5.0f);
        if (newBest && score > 0) {
            sfx.play(uj::Sfx::Win);
            particles.burst((float)(WINDOW_W / 2), (float)(TOP_BAND + 200), 14,
                            {230, 200, 60, 255}, 8.0f, 0.7f, 5.0f);
            floatTexts.spawn(std::make_shared<TextDisplay>(
                WINDOW_W / 2 - 60, TOP_BAND + 120, "NEW BEST!"),
                WINDOW_W / 2 - 60, TOP_BAND + 120);
        }
        setMessage("GAME OVER - Round " +
                   std::to_string(sequence.size()) + " - Best " +
                   std::to_string(bestScore) + " - Press R to restart");
        updateHUD();
        gameWon = false;
        endGame();
    }

    void winGame() {
        bestScore = std::max(bestScore, score);
        // ---- Juice: fanfare + confetti ----------------------------------------
        sfx.play(uj::Sfx::Win);
        shake.add(0.5f);
        for (int i = 0; i < 4; ++i) {
            particles.burst((float)(WINDOW_W / 2 + (i - 1) * 90),
                            (float)(TOP_BAND + 160), 20,
                            i % 3 == 0 ? SDL_Color{255, 220, 60, 255} :
                            i % 3 == 1 ? SDL_Color{80, 220, 255, 255} :
                                         SDL_Color{140, 255, 120, 255},
                            10.0f, 0.9f, 6.0f);
        }
        floatTexts.spawn(std::make_shared<TextDisplay>(
            WINDOW_W / 2 - 60, TOP_BAND + 100, "YOU WIN!"),
            WINDOW_W / 2 - 60, TOP_BAND + 100);
        setMessage("YOU WIN! Best " + std::to_string(bestScore) +
                   " - Press R to play again");
        updateHUD();
        gameWon = true;
        endGame();
    }

    // ---- Autopilot (perfect recall) ---------------------------------------------
    void botStep() {
        if (phase == 1 && gameRunning) {
            // The bot has seen the sequence and replays it exactly.
            pressTile(sequence[static_cast<std::size_t>(inputIndex)]);
        }
    }

    // ---- LLM actions -------------------------------------------------------------
    ActionResult pressAction(int t) {
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
        if (phase != 1) {
            result.message = "Watch the sequence first";
            return result;
        }
        pressTile(t);
        result.success = true;
        result.message = std::string("Pressed ") + colorName(t);
        result.gameOver = gameOver;
        result.gameWon = gameWon;
        return result;
    }

    // ---- Mouse --------------------------------------------------------------------
    void handleClick() {
        if (!gameRunning || paused) return;
        if (input.mouseY < TOP_BAND || input.mouseY >= WINDOW_H) return;
        const int col = input.mouseX / (WINDOW_W / 2);
        const int row = (input.mouseY - TOP_BAND) / ((WINDOW_H - TOP_BAND) / 2);
        if (col < 0 || col > 1 || row < 0 || row > 1) return;
        pressTile(row * 2 + col);
    }

    // ---- Helpers -------------------------------------------------------------------
    static uj::Sfx noteFor(int t) {
        switch (t) {
            case 0: return uj::Sfx::Note1;
            case 1: return uj::Sfx::Note2;
            case 2: return uj::Sfx::Note3;
            default: return uj::Sfx::Note4;
        }
    }

    static const char* colorName(int t) {
        switch (t) {
            case 0: return "red";
            case 1: return "blue";
            case 2: return "green";
            default: return "yellow";
        }
    }

    static SDL_Color tileColor(int t) {
        switch (t) {
            case 0: return {190, 40, 40, 255};
            case 1: return {40, 90, 200, 255};
            case 2: return {40, 170, 80, 255};
            default: return {210, 170, 40, 255};
        }
    }

    static SDL_Color tileBright(int t) {
        switch (t) {
            case 0: return {255, 90, 80, 255};
            case 1: return {90, 160, 255, 255};
            case 2: return {90, 230, 130, 255};
            default: return {255, 230, 90, 255};
        }
    }

    std::pair<int, int> tileCenter(int t) const {
        const int col = t % 2, row = t / 2;
        return {col * (WINDOW_W / 2) + WINDOW_W / 4,
                TOP_BAND + row * ((WINDOW_H - TOP_BAND) / 2) +
                    (WINDOW_H - TOP_BAND) / 4};
    }

    // ---- Juice / HUD ----------------------------------------------------------------
    void updateFx(float dt) {
        particles.update(dt);
        floatTexts.update(dt);
        shake.update(dt);
        if (pressFlashTimer > 0.0f) pressFlashTimer -= dt;
    }

    void setMessage(const std::string& text) {
        statusText = text;
        if (message) message->setText(text);
    }

    void updateHUD() {
        if (!hud) return;
        hud->setText("Round " + std::to_string(sequence.size()) +
                     "/" + std::to_string(WIN_ROUND) +
                     "    Score " + std::to_string(score) +
                     "    Best " + std::to_string(std::max(bestScore, score)));
    }
};

// UMBRA_GAME_NO_MAIN lets tests (tests/test_games.cpp) include this file to
// reach the SimonSays class headlessly without a second main() symbol.
#ifndef UMBRA_GAME_NO_MAIN
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
#ifdef __EMSCRIPTEN__
    static SimonSays game;
#else
    SimonSays game;
#endif
    game.run();
    return 0;
}
#endif
