// Conway's Game of Life for Umbra Engine
#include "Engine/Core/Game2D.h"
#include <vector>

class GameOfLife : public Game2D
{
    std::vector<std::vector<int>> grid;
    std::vector<std::vector<int>> nextGrid;
    int cols, rows;
    uint32_t lastUpdate = 0;
    bool paused = false;

    // Bounds-safe accessors (rows/cols are int, vector indices are size_t).
    // Named cellAt/nextCellAt (y,x order) to avoid confusion with Grid::cell(x,y).
    int& cellAt(int y, int x) { return grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]; }
    int& nextCellAt(int y, int x) { return nextGrid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]; }

public:
    GameOfLife() : Game2D("Umbra Game of Life", 700, 700, 10) {}

    void initGame() override {
        cols = 70;
        rows = 70;
        grid.resize(static_cast<std::size_t>(rows), std::vector<int>(static_cast<std::size_t>(cols), 0));
        nextGrid.resize(static_cast<std::size_t>(rows), std::vector<int>(static_cast<std::size_t>(cols), 0));

        // Initial pattern: Glider
        grid[10][10] = 1; grid[11][11] = 1; grid[12][9] = 1; grid[12][10] = 1; grid[12][11] = 1;
        
        bindKey(KEY_SPACE).onPress([this](){ paused = !paused; });
        bindKey(KEY_R).onPress([this](){
            for(auto& r : grid) std::fill(r.begin(), r.end(), 0);
        });
    }

    int countNeighbors(int x, int y) {
        int sum = 0;
        for (int i = -1; i < 2; i++) {
            for (int j = -1; j < 2; j++) {
                int col = (x + i + cols) % cols;
                int row = (y + j + rows) % rows;
                sum += cellAt(row, col);
            }
        }
        sum -= cellAt(y, x);
        return sum;
    }

    void update() override {
        Game2D::update();

        int mx, my;
        if (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(1)) {
            int gx = mx / 10;
            int gy = my / 10;
            if (gx >= 0 && gx < cols && gy >= 0 && gy < rows) cellAt(gy, gx) = 1;
        }

        if (!paused && SDL_GetTicks() - lastUpdate > 100) {
            for (int y = 0; y < rows; y++) {
                for (int x = 0; x < cols; x++) {
                    int neighbors = countNeighbors(x, y);
                    int state = cellAt(y, x);
                    if (state == 0 && neighbors == 3) nextCellAt(y, x) = 1;
                    else if (state == 1 && (neighbors < 2 || neighbors > 3)) nextCellAt(y, x) = 0;
                    else nextCellAt(y, x) = state;
                }
            }
            grid = nextGrid;
            lastUpdate = SDL_GetTicks();
        }

        Renderer* renderer = getRenderer();
        renderer->clearScreen(5, 5, 10, 255);

        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                if (cellAt(y, x) == 1) {
                    SDL_SetRenderDrawColor(renderer->renderer, 0, 255, 100, 255);
                    SDL_Rect r = {x * 10, y * 10, 9, 9};
                    SDL_RenderFillRect(renderer->renderer, &r);
                }
            }
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    GameOfLife app;
    app.run();
    return 0;
}
