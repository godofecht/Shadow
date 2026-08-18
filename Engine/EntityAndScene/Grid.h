// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include "Engine/Core/Geometry.h"
#include <vector>
#include <functional>
#include <memory>
#include <SDL2/SDL.h>

// Forward declarations
class Renderer;

// Grid cell properties
struct GridCell {
    int value = 0;
    bool isSolid = false;
    bool isVisible = true;
    SDL_Color color = {255, 255, 255, 255};
    SDL_Color borderColor = {0, 0, 0, 255};
};

// 2D Grid for tile-based games
class Grid {
public:
    Grid(int width, int height, int tileSize = 20);
    
    // Grid access
    GridCell& cell(int x, int y);
    const GridCell& cell(int x, int y) const;
    int getValue(int x, int y) const { return cell(x, y).value; }
    void setValue(int x, int y, int value) { cell(x, y).value = value; }
    
    // Dimensions
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getTileSize() const { return tileSize; }
    
    // Coordinate conversion
    Rect<float> getCellRect(int x, int y) const;
    void screenToGrid(int screenX, int screenY, int& gridX, int& gridY) const;
    void gridToScreen(int gridX, int gridY, int& screenX, int& screenY) const;
    
    // Bounds checking
    bool isInBounds(int x, int y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }
    
    // Rendering
    void render(Renderer* renderer);
    void setCellColor(int x, int y, SDL_Color color);
    void setBorderColor(SDL_Color color);
    
    // Fill operations
    void fill(SDL_Color color);
    void clear();
    
    // Get underlying data for TileMap compatibility
    std::vector<std::vector<float>> toTileMapData() const;
    
private:
    int width, height, tileSize;
    std::vector<std::vector<GridCell>> cells;
    SDL_Color defaultBorderColor = {0, 0, 0, 255};
};

// Entity that lives on the grid
class GridEntity {
public:
    GridEntity(Grid* grid, int x, int y);
    virtual ~GridEntity() = default;
    
    // Position
    int getX() const { return gridX; }
    int getY() const { return gridY; }
    void setPosition(int x, int y);
    void move(int dx, int dy);
    
    // Movement with collision
    bool tryMove(int dx, int dy);
    bool canMoveTo(int x, int y) const;
    
    // Grid access
    Grid* getGrid() const { return grid; }
    
    // Visual properties
    SDL_Color getColor() const { return color; }
    void setColor(SDL_Color c) { color = c; }
    char getSymbol() const { return symbol; }
    void setSymbol(char s) { symbol = s; }
    
    // Update - override for entity logic
    virtual void update(float deltaTime) { (void)deltaTime; }
    virtual void render(Renderer* renderer);
    
    // State
    bool isActive() const { return active; }
    void setActive(bool a) { active = a; }
    
protected:
    Grid* grid;
    int gridX, gridY;
    SDL_Color color = {255, 255, 255, 255};
    char symbol = 0;
    bool active = true;
};
