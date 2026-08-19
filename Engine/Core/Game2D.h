// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include "Engine/Core/SDLApp.h"
#include "Engine/Core/InputManager.h"
#include "Engine/EntityAndScene/Grid.h"
#include "Engine/Core/UI.h"
#include "Engine/Core/GameState.h"
#include <memory>
#include <vector>
#include <functional>
#include <algorithm>
#include <unordered_map>

// Base class for 2D grid-based games with LLM support
class Game2D : public Game, public LLMPlayable {
public:
    Game2D(const char* windowTitle, int width, int height, int tileSize = 20);
    
    // Override these in your game class
    virtual void initGame() = 0;           // Called once at game start
    virtual void updateGame(float dt) { (void)dt; }   // Called every frame
    virtual void renderGame() {}           // Called every frame after grid render
    virtual void onGameOver() {}           // Called when game ends
    
    // LLM interface - override for LLM-playable games
    GameState getState() const override;
    ActionResult executeAction(const std::string& action) override;
    std::vector<std::string> getAvailableActions() const override;
    void reset() override { startGame(); }
    bool isGameOver() const override { return gameOver; }
    bool isGameWon() const override { return gameWon; }

    // Headless stepping: advance the game simulation by dt without the SDL
    // input poll (which needs a window/display). Lets unit tests and the
    // Emscripten/Node ASan runs drive real game physics - e.g. Pong's
    // serve -> rally -> score path - through the exact updateGame() code a
    // frame would call. The native smoke runs reach the same code via
    // update() -> updateGame().
    void tick(float dt) { updateGame(dt); }
    
    // Action registration (for LLM interface)
    using ActionCallback = std::function<ActionResult()>;
    void registerAction(const std::string& name, ActionCallback callback);
    
protected:
    std::unordered_map<std::string, ActionCallback> registeredActions;
    std::vector<std::string> actionNames;
    bool gameWon = false;
    
    // Game state
    void startGame();
    void endGame();
    bool isGameRunning() const { return gameRunning; }
    
    // Grid access
    Grid* getGrid() const { return grid.get(); }
    void createGrid(int width, int height, int tileSize = 20);
    void renderGrid();  // Render the grid directly
    
    // Entity management
    template<typename T, typename... Args>
    std::shared_ptr<T> createEntity(Args&&... args) {
        auto entity = std::make_shared<T>(std::forward<Args>(args)...);
        entities.push_back(entity);
        return entity;
    }
    
    void removeInactiveEntities();
    
    // Input binding (fluent API)
    InputBinding& bindKey(KeyCode key) { return input.bind(key); }
    InputBinding& bindMouse(MouseButton button) { return input.bindMouse(button); }
    
    // UI helpers
    std::shared_ptr<TextDisplay> createText(int x, int y, const std::string& text = "");
    std::shared_ptr<Button> createButton(int x, int y, int w, int h, const std::string& label = "");
    std::shared_ptr<GameStats> createStats(int x, int y);
    
    // Utility
    void setGridColors(int value0, int value1, SDL_Color color0, SDL_Color color1);

    // Headless smoke mode (PONG_SMOKE=1): enables game-level autoplay hooks
    // AND auto-restarts the game whenever it ends, so a dummy-driver run
    // keeps exercising update/render paths instead of parking on a frozen
    // game-over frame. Games set this in their constructor from the env var.
    void setSmokeMode(bool enabled) { smokeMode = enabled; }
    bool isSmokeMode() const { return smokeMode; }

    // Timing
    float getDeltaTime() const { return deltaTime; }
    float getGameTime() const { return gameTime; }

    // Engine callbacks - these override Game's virtuals
    void onStart() override;
    void initializeComponents() override;
    void update() override;
    
    std::unique_ptr<Grid> grid;
    InputManager2D input;
    
    std::vector<std::shared_ptr<GridEntity>> entities;
    std::vector<std::shared_ptr<TextDisplay>> textDisplays;
    std::vector<std::shared_ptr<Button>> buttons;
    std::vector<std::shared_ptr<GameStats>> statsDisplays;
    
    int tileSize;
    bool gameRunning = false;
    bool gameOver = false;
    
    float deltaTime = 0;
    float gameTime = 0;
    bool smokeMode = false;   // headless autoplay; see setSmokeMode()
    
private:
    Uint32 lastFrameTime = 0;
    [[maybe_unused]] int gameWidth, gameHeight;
};
