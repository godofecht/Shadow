// Transparency in 2D Showcase for Umbra Engine
// Demonstrates transparency in 2D
#include "Engine/Core/SDLApp.h"
#include <vector>

class Transparency2DGame : public Game
{
    struct TransparentShape {
        float x, y;
        float width, height;
        SDL_Color color;
        std::string name;
    };
    
    std::vector<TransparentShape> shapes;
    int currentBlendMode;
    float time;

public:
    Transparency2DGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {
        currentBlendMode = 0;
        time = 0;
    }

    void onStart() override {
        // Create overlapping transparent shapes
        TransparentShape s1;
        s1.x = 200; s1.y = 150;
        s1.width = 200; s1.height = 150;
        s1.color = {255, 0, 0, 150};
        s1.name = "Red (Alpha 150)";
        shapes.push_back(s1);
        
        TransparentShape s2;
        s2.x = 300; s2.y = 200;
        s2.width = 200; s2.height = 150;
        s2.color = {0, 255, 0, 150};
        s2.name = "Green (Alpha 150)";
        shapes.push_back(s2);
        
        TransparentShape s3;
        s3.x = 250; s3.y = 280;
        s3.width = 200; s3.height = 150;
        s3.color = {0, 0, 255, 150};
        s3.name = "Blue (Alpha 150)";
        shapes.push_back(s3);
        
        // Circles with gradient alpha
        for (int i = 0; i < 5; i++) {
            TransparentShape circle;
            circle.x = static_cast<float>(550 + i * 40);
            circle.y = 150.0f;
            circle.width = 30;
            circle.height = 30;
            circle.color = {255, 255, 0, (Uint8)(50 + i * 40)};
            circle.name = "Alpha " + std::to_string(50 + i * 40);
            shapes.push_back(circle);
        }
    }

    void update() override {
        time += 0.016f;
        
        // Animate shapes
        shapes[0].y = 150 + sin(time) * 20;
        shapes[1].y = 200 + sin(time * 0.8f) * 20;
        shapes[2].y = 280 + sin(time * 1.2f) * 20;
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(30, 30, 40, 255);
        
        // Draw background pattern to show transparency
        SDL_SetRenderDrawColor(renderer->renderer, 60, 60, 70, 255);
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 8; j++) {
                if ((i + j) % 2 == 0) {
                    SDL_Rect rect = {i * 80, j * 75, 80, 75};
                    SDL_RenderFillRect(renderer->renderer, &rect);
                }
            }
        }
        
        // Set blend mode
        SDL_BlendMode blendModes[] = {SDL_BLENDMODE_NONE, SDL_BLENDMODE_BLEND, SDL_BLENDMODE_ADD, SDL_BLENDMODE_MOD};
        SDL_SetRenderDrawBlendMode(renderer->renderer, blendModes[currentBlendMode]);
        
        // Render transparent shapes
        for (const auto& shape : shapes) {
            SDL_SetRenderDrawColor(renderer->renderer, shape.color.r, shape.color.g, shape.color.b, shape.color.a);
            
            // Draw filled rectangle
            SDL_Rect rect = {(int)shape.x, (int)shape.y, (int)shape.width, (int)shape.height};
            SDL_RenderFillRect(renderer->renderer, &rect);
            
            // Draw outline
            SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 200);
            SDL_RenderDrawRect(renderer->renderer, &rect);
        }
        
        // Draw blend mode indicator
        const wchar_t* modeNames[] = {L"NONE", L"BLEND", L"ADD", L"MOD"};
        wchar_t modeInfo[64];
        swprintf(modeInfo, 64, L"Blend Mode: %s (Press SPACE to change)", modeNames[currentBlendMode]);
        Rect<float> modeBounds(10, 500, 400, 30);
        renderer->getTextWriter()->drawTextToRenderer(modeInfo, renderer->renderer, modeBounds, "/default.ttf");
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        // Handle blend mode switching
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
                currentBlendMode = (currentBlendMode + 1) % 4;
            }
        }
        
        std::wstring info = L"Transparency in 2D - Overlapping transparent shapes with different blend modes";
        Rect<float> textBounds(10, 10, 600, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Transparency2DGame app("Umbra Transparency 2D", 800, 600);
    app.run();
    return 0;
}
