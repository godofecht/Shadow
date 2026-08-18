// Fixed Timestep Showcase for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include <vector>

class FixedStepGame : public Game
{
    float variableX = 0;
    float fixedX = 0;
    float speed = 200.0f; // pixels per second

    float accumulator = 0;
    const float dt = 1.0f / 60.0f;

public:
    FixedStepGame(const char* title, int width, int height) : Game(title, width, height) {}

    void onStart() override {
        std::cout << "Top: Variable Update (dependent on framerate)" << '\n';
        std::cout << "Bottom: Fixed Update (1/60s steps)" << '\n';
    }

    void update() override {
        float frameTime = 1.0f / 60.0f; // Simulation of deltaTime
        
        // 1. Variable Update (Bad for physics)
        variableX += speed * frameTime;
        if (variableX > 700) variableX = 0;

        // 2. Fixed Timestep (Good for physics)
        accumulator += frameTime;
        while (accumulator >= dt) {
            fixedX += speed * dt;
            accumulator -= dt;
        }
        if (fixedX > 700) fixedX = 0;

        Renderer* renderer = getRenderer();
        renderer->clearScreen(15, 15, 20, 255);

        // Render Variable
        SDL_SetRenderDrawColor(renderer->renderer, 255, 100, 100, 255);
        SDL_Rect r1 = {(int)variableX - 20, 200, 40, 40};
        SDL_RenderFillRect(renderer->renderer, &r1);

        // Render Fixed
        SDL_SetRenderDrawColor(renderer->renderer, 100, 255, 100, 255);
        SDL_Rect r2 = {(int)fixedX - 20, 400, 40, 40};
        SDL_RenderFillRect(renderer->renderer, &r2);
        
        // Draw separation line
        SDL_SetRenderDrawColor(renderer->renderer, 50, 50, 60, 255);
        SDL_RenderDrawLine(renderer->renderer, 0, 350, 700, 350);
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    FixedStepGame app("Umbra Fixed Timestep", 700, 700);
    app.run();
    return 0;
}
