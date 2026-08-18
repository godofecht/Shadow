// Particle System Showcase for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include "Engine/Rendering/ParticleSystem.h"
#include "Engine/Core/UI.h"
#include <cmath>

class ParticleGame : public Game
{
    std::unique_ptr<ExplanationOverlay> explanation;
    std::unique_ptr<ParticleSystem> particleSystem;
    
public:
    ParticleGame(const char* title, int width, int height) : Game(title, width, height) {}

    void onStart() override {
        particleSystem = std::make_unique<ParticleSystem>(2000);
        explanation = std::make_unique<ExplanationOverlay>(20, 20, 300);
        explanation->addLine("FEATURE: Particle System");
        explanation->addLine("TECH: Additive Blending");
        explanation->addLine("CAPACITY: 2000 Particles");
        explanation->addLine("INPUT: Left Click to Emit");
    }

    void update() override {
        int mx, my;
        Uint32 mouseState = SDL_GetMouseState(&mx, &my);

        // Emit particles on mouse click
        if (mouseState & SDL_BUTTON(1)) {
            for (int i = 0; i < 10; i++) {
                float angle = (rand() % 360) * (3.14159f / 180.0f);
                float speed = (rand() % 100) / 20.0f;
                particleSystem->emit(mx, my, cos(angle) * speed, sin(angle) * speed, {255, 150, 50, 255}, 1.0f, 8.0f);
            }
        } else {
            // Auto-emit from center
            float time = SDL_GetTicks() * 0.005f;
            float px = 350 + cos(time) * 200;
            float py = 350 + sin(time * 1.5f) * 150;
            SDL_Color color = { 
                (Uint8)(150 + 100 * sin(time)), 
                (Uint8)(150 + 100 * cos(time * 0.8f)), 
                255, 255 
            };
            for (int i = 0; i < 5; i++) {
                float angle = (rand() % 360) * (3.14159f / 180.0f);
                float speed = (rand() % 50) / 25.0f;
                particleSystem->emit(px, py, cos(angle) * speed, sin(angle) * speed, color, 1.0f, 10.0f);
            }
        }

        particleSystem->update();

        Renderer* renderer = getRenderer();
        
        // Clear with dark background (NOT black)
        renderer->clearScreen(10, 10, 20, 255);
        
        // Render particles with additive blending
        particleSystem->render(renderer->renderer);
        
        // Render explanation overlay
        explanation->render(renderer);
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    ParticleGame app("Umbra Particle Showcase", 800, 600);
    app.run();
    return 0;
}
