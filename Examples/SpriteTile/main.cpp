// Sprite Tile Showcase for Umbra Engine
// Renders a sprite tiled in a grid
#include "Engine/Core/SDLApp.h"
#include <vector>

class SpriteTileGame : public Game
{
    struct TiledSprite {
        float x, y;
        float tileWidth, tileHeight;
        int tileCountX, tileCountY;
        SDL_Color baseColor;
        std::string name;
    };
    
    std::vector<TiledSprite> tiledSprites;
    float time;

    void drawTiledSprite(const TiledSprite& ts, Renderer* ren) {
        float totalWidth = ts.tileWidth * ts.tileCountX;
        float totalHeight = ts.tileHeight * ts.tileCountY;
        
        for (int row = 0; row < ts.tileCountY; row++) {
            for (int col = 0; col < ts.tileCountX; col++) {
                float tx = ts.x + col * ts.tileWidth;
                float ty = ts.y + row * ts.tileHeight;
                
                // Create checkerboard pattern
                bool isEven = (row + col) % 2 == 0;
                SDL_Color color = ts.baseColor;
                
                if (!isEven) {
                    color.r = (Uint8)(color.r * 0.7f);
                    color.g = (Uint8)(color.g * 0.7f);
                    color.b = (Uint8)(color.b * 0.7f);
                }
                
                // Animate color
                float anim = sin(time + row * 0.5f + col * 0.3f) * 0.3f + 0.7f;
                color.r = (Uint8)(color.r * anim);
                color.g = (Uint8)(color.g * anim);
                color.b = (Uint8)(color.b * anim);
                
                SDL_SetRenderDrawColor(ren->renderer, color.r, color.g, color.b, 255);
                
                SDL_Rect tileRect = {(int)tx, (int)ty, (int)ts.tileWidth, (int)ts.tileHeight};
                SDL_RenderFillRect(ren->renderer, &tileRect);
                
                // Draw tile border
                SDL_SetRenderDrawColor(ren->renderer, 0, 0, 0, 100);
                SDL_RenderDrawRect(ren->renderer, &tileRect);
            }
        }
        
        // Draw bounding box
        SDL_SetRenderDrawColor(ren->renderer, 255, 255, 255, 255);
        SDL_Rect bounds = {(int)ts.x, (int)ts.y, (int)totalWidth, (int)totalHeight};
        SDL_RenderDrawRect(ren->renderer, &bounds);
    }

public:
    SpriteTileGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {
        time = 0;
    }

    void onStart() override {
        // Create tiled sprites with different configurations
        TiledSprite ts1;
        ts1.x = 50; ts1.y = 50;
        ts1.tileWidth = 40; ts1.tileHeight = 40;
        ts1.tileCountX = 5; ts1.tileCountY = 4;
        ts1.baseColor = {255, 150, 100, 255};
        ts1.name = "5x4 Tiles";
        tiledSprites.push_back(ts1);
        
        TiledSprite ts2;
        ts2.x = 350; ts2.y = 50;
        ts2.tileWidth = 30; ts2.tileHeight = 30;
        ts2.tileCountX = 8; ts2.tileCountY = 6;
        ts2.baseColor = {100, 255, 150, 255};
        ts2.name = "8x6 Tiles";
        tiledSprites.push_back(ts2);
        
        TiledSprite ts3;
        ts3.x = 100; ts3.y = 350;
        ts3.tileWidth = 50; ts3.tileHeight = 25;
        ts3.tileCountX = 6; ts3.tileCountY = 8;
        ts3.baseColor = {100, 150, 255, 255};
        ts3.name = "6x8 Rectangular";
        tiledSprites.push_back(ts3);
        
        TiledSprite ts4;
        ts4.x = 450; ts4.y = 350;
        ts4.tileWidth = 25; ts4.tileHeight = 25;
        ts4.tileCountX = 10; ts4.tileCountY = 8;
        ts4.baseColor = {255, 100, 200, 255};
        ts4.name = "10x8 Fine";
        tiledSprites.push_back(ts4);
    }

    void update() override {
        time += 0.03f;
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(25, 25, 35, 255);
        
        // Draw all tiled sprites
        for (const auto& ts : tiledSprites) {
            drawTiledSprite(ts, renderer);
            
            // Draw label
            wchar_t label[64];
            swprintf(label, 64, L"%hs (%.0fx%.0f each)", ts.name.c_str(), ts.tileWidth, ts.tileHeight);
            Rect<float> textBounds(ts.x, ts.y + ts.tileHeight * ts.tileCountY + 5, 200, 30);
            renderer->getTextWriter()->drawTextToRenderer(label, renderer->renderer, textBounds, "/default.ttf");
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        std::wstring info = L"Sprite Tile - Tiled sprite grid with animated colors";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    SpriteTileGame app("Umbra Sprite Tile", 800, 600);
    app.run();
    return 0;
}
