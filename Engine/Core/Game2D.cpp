// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

#include "Engine/Core/Game2D.h"
#include <iostream>

Game2D::Game2D(const char* title, int width, int height, int tileSize)
    : Game(title, width, height), tileSize(tileSize), gameWidth(width), gameHeight(height) {
}

void Game2D::startGame() {
    gameRunning = true;
    gameOver = false;
    gameTime = 0;
    initGame();
}

void Game2D::endGame() {
    gameRunning = false;
    gameOver = true;
    onGameOver();
}

void Game2D::createGrid(int width, int height, int _tileSize) {
    this->tileSize = _tileSize;
    grid = std::make_unique<Grid>(width, height, _tileSize);
}

void Game2D::removeInactiveEntities() {
    entities.erase(
        std::remove_if(entities.begin(), entities.end(),
            [](const std::shared_ptr<GridEntity>& e) { return !e->isActive(); }),
        entities.end()
    );
}

std::shared_ptr<TextDisplay> Game2D::createText(int x, int y, const std::string& text) {
    auto display = std::make_shared<TextDisplay>(x, y, text);
    textDisplays.push_back(display);
    return display;
}

std::shared_ptr<Button> Game2D::createButton(int x, int y, int w, int h, const std::string& label) {
    auto button = std::make_shared<Button>(x, y, w, h, label);
    buttons.push_back(button);
    return button;
}

std::shared_ptr<GameStats> Game2D::createStats(int x, int y) {
    auto stats = std::make_shared<GameStats>(x, y);
    statsDisplays.push_back(stats);
    return stats;
}

void Game2D::setGridColors(int value0, int value1, SDL_Color color0, SDL_Color color1) {
    if (!grid) return;
    
    for (int y = 0; y < grid->getHeight(); y++) {
        for (int x = 0; x < grid->getWidth(); x++) {
            int val = grid->getValue(x, y);
            if (val == value0) {
                grid->setCellColor(x, y, color0);
            } else if (val == value1) {
                grid->setCellColor(x, y, color1);
            }
        }
    }
}

// LLM Interface Implementation
GameState Game2D::getState() const {
    GameState state;
    state.gameRunning = gameRunning;
    state.gameOver = gameOver;
    state.gameWon = gameWon;
    
    if (grid) {
        state.gridWidth = grid->getWidth();
        state.gridHeight = grid->getHeight();
        const std::size_t h = static_cast<std::size_t>(grid->getHeight());
        const std::size_t w = static_cast<std::size_t>(grid->getWidth());
        state.grid.resize(h);
        for (std::size_t y = 0; y < h; y++) {
            state.grid[y].resize(w);
            for (std::size_t x = 0; x < w; x++) {
                state.grid[y][x] = grid->getValue(static_cast<int>(x), static_cast<int>(y));
            }
        }
    }
    
    return state;
}

ActionResult Game2D::executeAction(const std::string& action) {
    ActionResult result;
    
    auto it = registeredActions.find(action);
    if (it != registeredActions.end()) {
        result = it->second();
    } else {
        result.success = false;
        result.message = "Unknown action: " + action;
    }
    
    return result;
}

std::vector<std::string> Game2D::getAvailableActions() const {
    if (gameRunning) {
        return actionNames;
    }
    return {"restart"};
}

void Game2D::registerAction(const std::string& name, ActionCallback callback) {
    if (registeredActions.find(name) == registeredActions.end()) {
        actionNames.push_back(name);
    }
    registeredActions[name] = std::move(callback);
}

void Game2D::onStart() {
    startGame();
}

void Game2D::initializeComponents() {
    // No TileMap needed - we render the grid directly
    auto scene = std::make_unique<Scene>();
    auto assetManager = getAssetManager();
    scene->setAssetManager(assetManager);
    addScene(std::move(scene));
    
    lastFrameTime = SDL_GetTicks();
}

void Game2D::renderGrid() {
    if (grid && getRenderer()) {
        grid->render(getRenderer());
    }
}

void Game2D::update() {
    // Calculate delta time
    Uint32 currentTime = SDL_GetTicks();
    deltaTime = (currentTime - lastFrameTime) / 1000.0f;
    lastFrameTime = currentTime;
    gameTime += deltaTime;
    
    // Update input
    input.update();

    // Headless smoke mode: restart as soon as the game ends so the loop
    // never parks on a frozen game-over frame (a dummy-driver run would
    // otherwise show zero coverage after a win/lose). Only active when a
    // game opted in via setSmokeMode().
    if (smokeMode && gameOver) {
        startGame();
    }
    
    if (gameRunning) {
        // Update entities
        for (auto& entity : entities) {
            entity->update(deltaTime);
        }
        removeInactiveEntities();
        
        // Update buttons
        bool mousePressed = input.isMousePressed(MOUSE_LEFT);
        for (auto& button : buttons) {
            button->update(input.mouseX, input.mouseY, mousePressed);
        }
        
        // Game-specific update
        updateGame(deltaTime);
    }
    
    // Call base class update for physics and scene rendering
    Game::update();
    
    // Render game-specific elements (entities, UI)
    if (gameRunning || gameOver) {
        renderGame();
    }
}
