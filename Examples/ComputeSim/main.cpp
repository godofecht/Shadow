// Compute-style N-Body Simulation for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include "Engine/Core/UI.h"
#include <vector>
#include <cmath>

struct BodyPart {
    float x, y;
    float vx, vy;
    SDL_Color color;
};

class ComputeGame : public Game
{
    std::vector<BodyPart> bodies;
    std::unique_ptr<ExplanationOverlay> explanation;
    const int BODY_COUNT = 500;

public:
    ComputeGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {}

    void onStart() override {
        for(int i=0; i<BODY_COUNT; i++) {
            float ang = (rand()%360) * 0.0174f;
            float dist = (rand()%200) + 50.0f;
            bodies.push_back({
                350 + cos(ang)*dist, 350 + sin(ang)*dist,
                sin(ang)*2.0f, -cos(ang)*2.0f,
                {(Uint8)(100 + rand()%155), (Uint8)(100 + rand()%155), 255, 255}
            });
        }
        explanation = std::make_unique<ExplanationOverlay>(20, 20, 350);
        explanation->addLine("FEATURE: Compute Simulation");
        explanation->addLine("TECH: N-Body Gravity (O(N^2) Simplified)");
        explanation->addLine("LOGIC: Vector Math per Entity Pair");
        explanation->addLine("INPUT: Click to repel bodies");
    }

    void update() override {
        float centerX = 350, centerY = 350;
        int mx, my;
        bool pressed = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(1);

        for(auto& b : bodies) {
            float dx = centerX - b.x;
            float dy = centerY - b.y;
            float distSq = dx*dx + dy*dy + 100.0f;
            float f = 500.0f / distSq;
            b.vx += (dx/sqrt(distSq)) * f;
            b.vy += (dy/sqrt(distSq)) * f;

            if(pressed) {
                float mdx = mx - b.x;
                float mdy = my - b.y;
                float mdistSq = mdx*mdx + mdy*mdy + 10.0f;
                b.vx -= (mdx/sqrt(mdistSq)) * (2000.0f / mdistSq);
                b.vy -= (mdy/sqrt(mdistSq)) * (2000.0f / mdistSq);
            }

            b.x += b.vx;
            b.y += b.vy;
            b.vx *= 0.99f;
            b.vy *= 0.99f;
        }

        Renderer* renderer = getRenderer();
        renderer->clearScreen(5, 5, 10, 255);

        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_ADD);
        for(const auto& b : bodies) {
            SDL_SetRenderDrawColor(renderer->renderer, b.color.r, b.color.g, b.color.b, 150);
            SDL_Rect r = {(int)b.x - 2, (int)b.y - 2, 4, 4};
            SDL_RenderFillRect(renderer->renderer, &r);
        }

        explanation->render(renderer);
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    ComputeGame app("Umbra Compute Gravity", 700, 700);
    app.run();
    return 0;
}
