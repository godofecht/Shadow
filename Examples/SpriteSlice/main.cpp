// Sprite Slice Showcase for Umbra Engine
// Showcases slicing sprites into sections that can be scaled independently via the 9-patch technique
#include "Engine/Core/SDLApp.h"
#include <vector>

class SpriteSliceGame : public Game
{
    struct NinePatchSprite {
        float x, y;
        float width, height;
        float cornerSize;
        SDL_Color color;
        std::string name;
    };
    
    std::vector<NinePatchSprite> ninePatchSprites;

    void drawNinePatch(const NinePatchSprite& np, Renderer* renderer) {
        float cw = np.cornerSize; // corner width/height
        float cx = np.x;
        float cy = np.y;
        float w = np.width;
        float h = np.height;
        
        SDL_SetRenderDrawColor(renderer->renderer, np.color.r, np.color.g, np.color.b, np.color.a);
        
        // Corner rectangles (not stretched)
        // Top-left
        SDL_Rect tl = {(int)cx, (int)cy, (int)cw, (int)cw};
        SDL_RenderFillRect(renderer->renderer, &tl);
        
        // Top-right
        SDL_Rect tr = {(int)(cx + w - cw), (int)cy, (int)cw, (int)cw};
        SDL_RenderFillRect(renderer->renderer, &tr);
        
        // Bottom-left
        SDL_Rect bl = {(int)cx, (int)(cy + h - cw), (int)cw, (int)cw};
        SDL_RenderFillRect(renderer->renderer, &bl);
        
        // Bottom-right
        SDL_Rect br = {(int)(cx + w - cw), (int)(cy + h - cw), (int)cw, (int)cw};
        SDL_RenderFillRect(renderer->renderer, &br);
        
        // Edge rectangles (stretched in one direction)
        // Top edge
        SDL_Rect top = {(int)(cx + cw), (int)cy, (int)(w - 2*cw), (int)cw};
        SDL_RenderFillRect(renderer->renderer, &top);
        
        // Bottom edge
        SDL_Rect bottom = {(int)(cx + cw), (int)(cy + h - cw), (int)(w - 2*cw), (int)cw};
        SDL_RenderFillRect(renderer->renderer, &bottom);
        
        // Left edge
        SDL_Rect left = {(int)cx, (int)(cy + cw), (int)cw, (int)(h - 2*cw)};
        SDL_RenderFillRect(renderer->renderer, &left);
        
        // Right edge
        SDL_Rect right = {(int)(cx + w - cw), (int)(cy + cw), (int)cw, (int)(h - 2*cw)};
        SDL_RenderFillRect(renderer->renderer, &right);
        
        // Center rectangle (stretched in both directions)
        SDL_Rect center = {(int)(cx + cw), (int)(cy + cw), (int)(w - 2*cw), (int)(h - 2*cw)};
        SDL_SetRenderDrawColor(renderer->renderer, 
            static_cast<Uint8>(np.color.r * 1.2), static_cast<Uint8>(np.color.g * 1.2), static_cast<Uint8>(np.color.b * 1.2), static_cast<Uint8>(np.color.a * 0.8));
        SDL_RenderFillRect(renderer->renderer, &center);
        
        // Draw 9-patch grid lines
        SDL_SetRenderDrawColor(renderer->renderer, 0, 0, 0, 100);
        SDL_RenderDrawLine(renderer->renderer, (int)(cx + cw), (int)cy, (int)(cx + cw), (int)(cy + h));
        SDL_RenderDrawLine(renderer->renderer, (int)(cx + w - cw), (int)cy, (int)(cx + w - cw), (int)(cy + h));
        SDL_RenderDrawLine(renderer->renderer, (int)cx, (int)(cy + cw), (int)(cx + w), (int)(cy + cw));
        SDL_RenderDrawLine(renderer->renderer, (int)cx, (int)(cy + h - cw), (int)(cx + w), (int)(cy + h - cw));
    }

public:
    SpriteSliceGame(const char* title, int width, int height) : Game(title, width, height) {}

    void onStart() override {
        // Create 9-patch sprites with different sizes
        NinePatchSprite np1;
        np1.x = 100; np1.y = 100;
        np1.width = 150; np1.height = 100;
        np1.cornerSize = 20;
        np1.color = {255, 150, 100, 255};
        np1.name = "Small";
        ninePatchSprites.push_back(np1);
        
        NinePatchSprite np2;
        np2.x = 350; np2.y = 100;
        np2.width = 300; np2.height = 150;
        np2.cornerSize = 30;
        np2.color = {100, 255, 150, 255};
        np2.name = "Wide";
        ninePatchSprites.push_back(np2);
        
        NinePatchSprite np3;
        np3.x = 100; np3.y = 350;
        np3.width = 120; np3.height = 200;
        np3.cornerSize = 25;
        np3.color = {100, 150, 255, 255};
        np3.name = "Tall";
        ninePatchSprites.push_back(np3);
        
        NinePatchSprite np4;
        np4.x = 400; np4.y = 350;
        np4.width = 350; np4.height = 180;
        np4.cornerSize = 35;
        np4.color = {255, 100, 200, 255};
        np4.name = "Large";
        ninePatchSprites.push_back(np4);
    }

    void update() override {
        Renderer* renderer = getRenderer();
        renderer->clearScreen(30, 30, 40, 255);
        
        // Draw all 9-patch sprites
        for (const auto& np : ninePatchSprites) {
            drawNinePatch(np, renderer);
            
            // Draw label
            wchar_t label[64];
            swprintf(label, 64, L"%s (%.0fx%.0f)", np.name.c_str(), np.width, np.height);
            Rect<float> textBounds(np.x, np.y + np.height + 5, 150, 30);
            renderer->getTextWriter()->drawTextToRenderer(label, renderer->renderer, textBounds, "/default.ttf");
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        std::wstring info = L"Sprite Slice (9-Patch) - Corners stay fixed, edges and center stretch";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    SpriteSliceGame app("Umbra Sprite Slice", 800, 600);
    app.run();
    return 0;
}
