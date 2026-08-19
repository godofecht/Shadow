// Spatial Audio Showcase for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include <cmath>

class SpatialAudioGame : public Game
{
    float soundX = 350, soundY = 350;
    float time = 0;
    bool audioStarted = false;

public:
    SpatialAudioGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {}

    void onStart() override {
        std::cout << "Spatial Audio Demo: Click 'Start Audio' to begin." << '\n';
    }

    void update() override {
        time += 0.02f;
        soundX = 350 + cos(time) * 250;
        soundY = 350 + sin(time * 0.5f) * 150;

        Renderer* renderer = getRenderer();
        renderer->clearScreen(10, 20, 15, 255);

        // Draw Listener
        SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
        SDL_Rect listener = {340, 340, 20, 20};
        SDL_RenderFillRect(renderer->renderer, &listener);
        
        // Draw Sound Source
        SDL_SetRenderDrawColor(renderer->renderer, 255, 100, 50, 255);
        SDL_Rect source = {(int)soundX - 15, (int)soundY - 15, 30, 30};
        SDL_RenderFillRect(renderer->renderer, &source);
        
        // Draw UI Button if audio not started
        if (!audioStarted) {
            SDL_SetRenderDrawColor(renderer->renderer, 50, 150, 255, 255);
            SDL_Rect btn = {275, 300, 150, 40};
            SDL_RenderFillRect(renderer->renderer, &btn);
            // Label simulated by color change on hover
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            if (mx > 275 && mx < 425 && my > 300 && my < 340) {
                SDL_SetRenderDrawColor(renderer->renderer, 100, 200, 255, 255);
                SDL_RenderDrawRect(renderer->renderer, &btn);
                if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(1)) {
                    audioStarted = true;
                    // Actual audio trigger
                    getAudioEngine()->getAudioMediaGroupById("ambient").addAudioPlayer("looptheme.wav", "theme");
                    getAudioEngine()->playAudioInGroup("ambient", "theme", true);
                }
            }
        }

        // Visualize waves
        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_BLEND);
        for(int i=1; i<4; i++) {
            float r = (fmod(time * 50.0f, 100.0f)) + i * 30.0f;
            SDL_SetRenderDrawColor(renderer->renderer, 255, 150, 100, (Uint8)(255 * (1.0f - r/200.0f)));
            SDL_Rect wave = {(int)(soundX - r), (int)(soundY - r), (int)r*2, (int)r*2};
            SDL_RenderDrawRect(renderer->renderer, &wave);
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    SpatialAudioGame app("Umbra Spatial Audio", 700, 700);
    app.run();
    return 0;
}
