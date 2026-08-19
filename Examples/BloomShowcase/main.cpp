// Bloom & Glow Showcase for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include <cmath>
#include <vector>

class BloomGame : public Game
{
    struct LightSource {
        float x, y;
        float radius;
        SDL_Color color;
        float phase;
    };
    std::vector<LightSource> lights;

public:
    BloomGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {}

    void onStart() override {
        lights = {
            {200, 200, 40, {255, 50, 50, 255}, 0},
            {500, 300, 60, {50, 255, 50, 255}, 2.0f},
            {350, 500, 50, {50, 50, 255, 255}, 4.0f}
        };
    }

    void update() override {
        float time = SDL_GetTicks() * 0.002f;
        for(auto& l : lights) {
            l.x += cos(time + l.phase) * 2.0f;
            l.y += sin(time * 0.7f + l.phase) * 2.0f;
        }

        Renderer* renderer = getRenderer();
        renderer->clearScreen(5, 5, 10, 255);

        // Core shapes (bright)
        for(const auto& l : lights) {
            SDL_SetRenderDrawColor(renderer->renderer, l.color.r, l.color.g, l.color.b, 255);
            SDL_Rect r = { (int)(l.x - l.radius/2), (int)(l.y - l.radius/2), (int)l.radius, (int)l.radius };
            SDL_RenderFillRect(renderer->renderer, &r);
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        // Emulate bloom by drawing larger, softer additive circles
        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_ADD);
        
        for(const auto& l : lights) {
            for(int i = 1; i <= 5; i++) {
                float glowRadius = l.radius * (1.0f + i * 0.4f);
                Uint8 alpha = (Uint8)(100 / (i * i));
                SDL_SetRenderDrawColor(renderer->renderer, l.color.r, l.color.g, l.color.b, alpha);
                
                // Draw glow "box" (simulated blur)
                SDL_Rect r = { (int)(l.x - glowRadius/2), (int)(l.y - glowRadius/2), (int)glowRadius, (int)glowRadius };
                SDL_RenderFillRect(renderer->renderer, &r);
            }
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    BloomGame app("Umbra Bloom Showcase", 700, 700);
    app.run();
    return 0;
}
