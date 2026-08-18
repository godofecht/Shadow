// State Machine AI Showcase for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include "Engine/Core/UI.h"
#include <vector>
#include <cmath>
#include <string>

enum class AIState { PATROL, CHASE, REST };

class AIGame : public Game
{
    AIState state = AIState::PATROL;
    float ax = 100, ay = 100;
    float targetX = 600, targetY = 600;
    float energy = 100.0f;
    std::unique_ptr<ExplanationOverlay> explanation;

public:
    AIGame(const char* title, int width, int height) : Game(title, width, height) {}

    void onStart() override {
        explanation = std::make_unique<ExplanationOverlay>(20, 20, 350);
        explanation->addLine("FEATURE: AI State Machines");
        explanation->addLine("TECH: Deterministic Finite Automata");
        explanation->addLine("LOGIC: Vision Cones & Energy Management");
        explanation->addLine("INPUT: Move Mouse to trigger CHASE");
    }

    void update() override {
        int mx, my;
        SDL_GetMouseState(&mx, &my);

        float dx = mx - ax;
        float dy = my - ay;
        float distToMouse = sqrt(dx*dx + dy*dy);

        // State Transitions
        if (state != AIState::REST && energy <= 0) state = AIState::REST;
        else if (state == AIState::REST && energy >= 100) state = AIState::PATROL;
        else if (state != AIState::REST) {
            if (distToMouse < 200) state = AIState::CHASE;
            else state = AIState::PATROL;
        }

        // State Logic
        if (state == AIState::PATROL) {
            float tx = targetX - ax, ty = targetY - ay;
            float d = sqrt(tx*tx + ty*ty);
            if(d < 10) {
                targetX = (rand() % 600) + 50;
                targetY = (rand() % 600) + 50;
            }
            ax += (tx/d) * 2.0f;
            ay += (ty/d) * 2.0f;
            energy -= 0.1f;
        } else if (state == AIState::CHASE) {
            ax += (dx/distToMouse) * 4.0f;
            ay += (dy/distToMouse) * 4.0f;
            energy -= 0.3f;
        } else if (state == AIState::REST) {
            energy += 0.5f;
        }

        Renderer* renderer = getRenderer();
        renderer->clearScreen(10, 15, 10, 255);

        // Draw Energy Bar
        SDL_SetRenderDrawColor(renderer->renderer, 50, 50, 50, 255);
        SDL_Rect barBg = {20, 650, 200, 20};
        SDL_RenderFillRect(renderer->renderer, &barBg);
        SDL_SetRenderDrawColor(renderer->renderer, 100, 255, 100, 255);
        SDL_Rect barFg = {20, 650, (int)(energy * 2), 20};
        SDL_RenderFillRect(renderer->renderer, &barFg);

        // Draw Agent
        SDL_Color agentColor;
        std::string stateName;
        if(state == AIState::PATROL) { agentColor = {100, 200, 255, 255}; stateName = "PATROLLING"; }
        else if(state == AIState::CHASE) { agentColor = {255, 50, 50, 255}; stateName = "CHASING TARGET"; }
        else { agentColor = {255, 255, 100, 255}; stateName = "RESTING (RECHARGING)"; }

        SDL_SetRenderDrawColor(renderer->renderer, agentColor.r, agentColor.g, agentColor.b, 255);
        SDL_Rect agent = {(int)ax - 15, (int)ay - 15, 30, 30};
        SDL_RenderFillRect(renderer->renderer, &agent);
        
        drawSimpleText(renderer->renderer, (int)ax - 40, (int)ay - 35, stateName, agentColor, 0.6f);

        explanation->render(renderer);
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    AIGame app("Umbra AI States", 700, 700);
    app.run();
    return 0;
}
