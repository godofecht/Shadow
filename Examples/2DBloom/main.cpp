// 2D Bloom Showcase for Umbra Engine
// Illustrates bloom post-processing in 2D
#include "Engine/Core/SDLApp.h"
#include <cmath>
#include <vector>

class Bloom2DGame : public Game
{
    struct LightSource {
        float x, y;
        float baseRadius;
        float radius;
        SDL_Color color;
        float phase;
        float speed;
    };
    std::vector<LightSource> lights;
    float time;

public:
    Bloom2DGame(const char* title, int width, int height) : Game(title, width, height) {
        time = 0;
    }

    void onStart() override {
        lights = {
            {200, 200, 40, 40, {255, 50, 50, 255}, 0, 1.5f},
            {500, 300, 60, 60, {50, 255, 50, 255}, 2.0f, 1.2f},
            {350, 500, 50, 50, {50, 50, 255, 255}, 4.0f, 1.8f},
            {600, 150, 35, 35, {255, 255, 50, 255}, 1.0f, 2.0f},
            {150, 450, 45, 45, {255, 50, 255, 255}, 3.0f, 1.3f}
        };
    }

    void update() override {
        time += 0.016f;
        
        // Animate lights
        for(auto& l : lights) {
            l.x += cos(time * l.speed + l.phase) * 1.5f;
            l.y += sin(time * l.speed * 0.7f + l.phase) * 1.5f;
            l.radius = l.baseRadius * (1.0f + 0.2f * sin(time * 2.0f + l.phase));
        }

        Renderer* renderer = getRenderer();
        renderer->clearScreen(5, 5, 15, 255);

        // Draw core shapes (bright centers)
        for(const auto& l : lights) {
            SDL_SetRenderDrawColor(renderer->renderer, l.color.r, l.color.g, l.color.b, 255);
            SDL_Rect r = { 
                (int)(l.x - l.radius/2), 
                (int)(l.y - l.radius/2), 
                (int)l.radius, 
                (int)l.radius 
            };
            SDL_RenderFillRect(renderer->renderer, &r);
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        // Bloom effect: draw larger, softer additive layers
        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_ADD);

        for(const auto& l : lights) {
            // Multiple bloom layers with decreasing alpha and increasing radius
            for(int i = 1; i <= 6; i++) {
                float glowRadius = l.radius * (1.0f + i * 0.5f);
                Uint8 alpha = (Uint8)(80 / (i * i));
                
                SDL_SetRenderDrawColor(renderer->renderer, 
                    l.color.r, l.color.g, l.color.b, alpha);

                SDL_Rect r = { 
                    (int)(l.x - glowRadius/2), 
                    (int)(l.y - glowRadius/2), 
                    (int)glowRadius, 
                    (int)glowRadius 
                };
                SDL_RenderFillRect(renderer->renderer, &r);
            }
        }
        
        // Reset blend mode
        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_BLEND);
        
        // Draw info text
        std::wstring info = L"2D Bloom Effect - Additive Glow Layers";
        Rect<float> textBounds(10, 10, 400, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Bloom2DGame app("Umbra 2D Bloom Showcase", 800, 600);
    app.run();
    return 0;
}
