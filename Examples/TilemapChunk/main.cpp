// Tilemap Chunk Showcase for Umbra Engine
// Renders a tilemap chunk
#include "Engine/Core/Game2D.h"
#include <vector>

class TilemapChunkGame : public Game2D
{
    struct TilemapChunk {
        int chunkX, chunkY;
        int tileSize;
        int chunkSize; // tiles per side
        std::vector<float> tiles;
        float offsetX, offsetY;
    };
    
    std::vector<TilemapChunk> chunks;
    int cameraX, cameraY;

    float getTileValue(int x, int y) {
        // Simple procedural terrain generation
        float nx = x * 0.1f;
        float ny = y * 0.1f;
        
        // Combine multiple noise-like functions
        float value = sin(nx) * cos(ny) * 0.5f + 0.5f;
        value += sin(nx * 2.3f + ny * 1.7f) * 0.25f + 0.25f;
        value += sin(nx * 5.1f - ny * 3.2f) * 0.125f + 0.125f;
        
        return value / 1.875f; // Normalize to ~[0, 1]
    }

    SDL_Color getTileColor(float value) {
        if (value < 0.3f) {
            // Water
            return {50, 100, 200, 255};
        } else if (value < 0.4f) {
            // Sand
            return {200, 180, 100, 255};
        } else if (value < 0.6f) {
            // Grass
            return {100, 180, 80, 255};
        } else if (value < 0.8f) {
            // Forest
            return {50, 120, 50, 255};
        } else {
            // Mountain
            return {150, 150, 150, 255};
        }
    }

public:
    TilemapChunkGame() : Game2D("Umbra Tilemap Chunk", 800, 600, 20) {
        cameraX = 0;
        cameraY = 0;
    }

    void initGame() override {
        int tileSize = 32;
        int chunkSize = 8;
        
        // Create multiple chunks
        for (int cx = -1; cx <= 1; cx++) {
            for (int cy = -1; cy <= 1; cy++) {
                TilemapChunk chunk;
                chunk.chunkX = cx;
                chunk.chunkY = cy;
                chunk.tileSize = tileSize;
                chunk.chunkSize = chunkSize;
                chunk.offsetX = cx * chunkSize * tileSize;
                chunk.offsetY = cy * chunkSize * tileSize;
                
                // Generate tile values
                for (int y = 0; y < chunkSize; y++) {
                    for (int x = 0; x < chunkSize; x++) {
                        float value = getTileValue(
                            cx * chunkSize + x,
                            cy * chunkSize + y
                        );
                        chunk.tiles.push_back(value);
                    }
                }
                
                chunks.push_back(chunk);
            }
        }
    }

    void updateGame(float dt) override {
        // Camera movement
        if (input.isKeyPressed(KEY_LEFT) || input.isKeyPressed(KEY_A)) {
            cameraX -= static_cast<int>(200 * dt);
        }
        if (input.isKeyPressed(KEY_RIGHT) || input.isKeyPressed(KEY_D)) {
            cameraX += static_cast<int>(200 * dt);
        }
        if (input.isKeyPressed(KEY_UP) || input.isKeyPressed(KEY_W)) {
            cameraY -= static_cast<int>(200 * dt);
        }
        if (input.isKeyPressed(KEY_DOWN) || input.isKeyPressed(KEY_S)) {
            cameraY += static_cast<int>(200 * dt);
        }
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        renderer->clearScreen(30, 60, 90, 255); // Water background
        
        // Render all chunks
        for (const auto& chunk : chunks) {
            for (int y = 0; y < chunk.chunkSize; y++) {
                for (int x = 0; x < chunk.chunkSize; x++) {
                    float value = chunk.tiles[static_cast<std::size_t>(y) * static_cast<std::size_t>(chunk.chunkSize) + static_cast<std::size_t>(x)];
                    SDL_Color color = getTileColor(value);
                    
                    float tileX = chunk.offsetX + x * chunk.tileSize - cameraX;
                    float tileY = chunk.offsetY + y * chunk.tileSize - cameraY;
                    
                    // Only render if visible
                    if (tileX + chunk.tileSize > 0 && tileX < 800 &&
                        tileY + chunk.tileSize > 0 && tileY < 600) {
                        
                        SDL_SetRenderDrawColor(renderer->renderer, color.r, color.g, color.b, 255);
                        SDL_Rect tileRect = {(int)tileX, (int)tileY, chunk.tileSize, chunk.tileSize};
                        SDL_RenderFillRect(renderer->renderer, &tileRect);
                        
                        // Draw tile border
                        SDL_SetRenderDrawColor(renderer->renderer, 0, 0, 0, 50);
                        SDL_RenderDrawRect(renderer->renderer, &tileRect);
                    }
                }
            }
            
            // Draw chunk border
            SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 100);
            SDL_Rect chunkRect = {
                (int)(chunk.offsetX - cameraX),
                (int)(chunk.offsetY - cameraY),
                chunk.chunkSize * chunk.tileSize,
                chunk.chunkSize * chunk.tileSize
            };
            SDL_RenderDrawRect(renderer->renderer, &chunkRect);
        }
        
        // Draw info without hard dependency on default.ttf.
        drawSimpleText(renderer->renderer, 10, 10, "Tilemap Chunk - Procedural terrain with chunked rendering", {255, 255, 255, 255}, 0.8f);
        drawSimpleText(renderer->renderer, 10, 30, "Camera: (" + std::to_string(cameraX) + ", " + std::to_string(cameraY) + ") - Use WASD/Arrows to move", {255, 255, 255, 255}, 0.8f);
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static TilemapChunkGame app;
#else
    TilemapChunkGame app;
#endif
    app.run();
    return 0;
}
