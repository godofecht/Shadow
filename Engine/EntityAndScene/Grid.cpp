// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

#include "Engine/EntityAndScene/Grid.h"
#include "Engine/Rendering/Renderer.h"
Grid::Grid(int width, int height, int tileSize)
    : width(width), height(height), tileSize(tileSize) {
    cells.resize(static_cast<std::size_t>(height), std::vector<GridCell>(static_cast<std::size_t>(width)));
}

GridCell& Grid::cell(int x, int y) {
    return cells[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

const GridCell& Grid::cell(int x, int y) const {
    return cells[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

Rect<float> Grid::getCellRect(int x, int y) const {
    return Rect<float>(
        static_cast<float>(x * tileSize),
        static_cast<float>(y * tileSize),
        static_cast<float>(tileSize),
        static_cast<float>(tileSize)
    );
}

void Grid::screenToGrid(int screenX, int screenY, int& gridX, int& gridY) const {
    gridX = screenX / tileSize;
    gridY = screenY / tileSize;
}

void Grid::gridToScreen(int gridX, int gridY, int& screenX, int& screenY) const {
    screenX = gridX * tileSize;
    screenY = gridY * tileSize;
}

void Grid::render(Renderer* renderer) {
    SDL_Renderer* sdlRenderer = renderer->renderer;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const GridCell& gc = cell(x, y);
            if (!gc.isVisible) continue;
            
            SDL_Rect rect = {
                x * tileSize,
                y * tileSize,
                tileSize,
                tileSize
            };
            
            // Fill
            SDL_SetRenderDrawColor(sdlRenderer, gc.color.r, gc.color.g, gc.color.b, gc.color.a);
            SDL_RenderFillRect(sdlRenderer, &rect);
            
            // Border
            SDL_SetRenderDrawColor(sdlRenderer, gc.borderColor.r, gc.borderColor.g, gc.borderColor.b, gc.borderColor.a);
            SDL_RenderDrawRect(sdlRenderer, &rect);
        }
    }
}

void Grid::setCellColor(int x, int y, SDL_Color color) {
    if (isInBounds(x, y)) {
        cell(x, y).color = color;
    }
}

void Grid::setBorderColor(SDL_Color color) {
    defaultBorderColor = color;
    for (auto& row : cells) {
        for (auto& cell : row) {
            cell.borderColor = color;
        }
    }
}

void Grid::fill(SDL_Color color) {
    for (auto& row : cells) {
        for (auto& cell : row) {
            cell.color = color;
        }
    }
}

void Grid::clear() {
    for (auto& row : cells) {
        for (auto& cell : row) {
            cell = GridCell();
        }
    }
}

std::vector<std::vector<float>> Grid::toTileMapData() const {
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    std::vector<std::vector<float>> data(h, std::vector<float>(w));
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            data[y][x] = static_cast<float>(cell(static_cast<int>(x), static_cast<int>(y)).value);
        }
    }
    return data;
}

// GridEntity implementation
GridEntity::GridEntity(Grid* grid, int x, int y)
    : grid(grid), gridX(x), gridY(y) {
}

void GridEntity::setPosition(int x, int y) {
    if (grid->isInBounds(x, y)) {
        gridX = x;
        gridY = y;
    }
}

void GridEntity::move(int dx, int dy) {
    setPosition(gridX + dx, gridY + dy);
}

bool GridEntity::tryMove(int dx, int dy) {
    int newX = gridX + dx;
    int newY = gridY + dy;
    if (canMoveTo(newX, newY)) {
        gridX = newX;
        gridY = newY;
        return true;
    }
    return false;
}

bool GridEntity::canMoveTo(int x, int y) const {
    return grid->isInBounds(x, y) && !grid->cell(x, y).isSolid;
}

void GridEntity::render(Renderer* renderer) {
    if (!active) return;
    
    SDL_Renderer* sdlRenderer = renderer->renderer;
    Rect<float> rect = grid->getCellRect(gridX, gridY);
    
    SDL_Rect sdlRect = {
        static_cast<int>(rect.x),
        static_cast<int>(rect.y),
        static_cast<int>(rect.width),
        static_cast<int>(rect.height)
    };
    
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(sdlRenderer, &sdlRect);
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(sdlRenderer, &sdlRect);
}
