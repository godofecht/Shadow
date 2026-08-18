// Skeletal Animation with Inverse Kinematics (FABRIK) for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include <vector>
#include <cmath>

struct Joint {
    float x, y;
};

class SkeletalIKGame : public Game
{
    std::vector<Joint> joints;
    std::vector<float> lengths;
    int jointCount = 5;
    float tolerance = 0.1f;

public:
    SkeletalIKGame(const char* title, int width, int height) : Game(title, width, height) {}

    void onStart() override {
        float startX = 350, startY = 600;
        float segmentLen = 60.0f;
        
        for (int i = 0; i < jointCount; i++) {
            joints.push_back({startX, startY - i * segmentLen});
            if (i > 0) lengths.push_back(segmentLen);
        }
    }

    void solveIK(float targetX, float targetY) {
        if (jointCount < 2) return;
        Joint origin = joints[0];
        const std::size_t n = static_cast<std::size_t>(jointCount);
        
        // FABRIK Algorithm
        int maxIterations = 10;
        while (maxIterations--) {
            // Forward pass
            joints[n - 1] = {targetX, targetY};
            // Countdown loop: i runs from jointCount-2 down to 0
            std::size_t i = n - 1;
            while (i > 0) {
                --i;
                const std::size_t next = i + 1;
                float dx = joints[i].x - joints[next].x;
                float dy = joints[i].y - joints[next].y;
                float d = sqrt(dx*dx + dy*dy);
                float r = lengths[i] / d;
                joints[i].x = joints[next].x + dx * r;
                joints[i].y = joints[next].y + dy * r;
            }

            // Backward pass
            joints[0] = origin;
            for (std::size_t i = 0; i + 1 < n; i++) {
                const std::size_t next = i + 1;
                float dx = joints[next].x - joints[i].x;
                float dy = joints[next].y - joints[i].y;
                float d = sqrt(dx*dx + dy*dy);
                float r = lengths[i] / d;
                joints[next].x = joints[i].x + dx * r;
                joints[next].y = joints[i].y + dy * r;
            }

            float lastDx = joints[n-1].x - targetX;
            float lastDy = joints[n-1].y - targetY;
            if (sqrt(lastDx*lastDx + lastDy*lastDy) < tolerance) break;
        }
    }

    void update() override {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        
        solveIK((float)mx, (float)my);

        Renderer* renderer = getRenderer();
        renderer->clearScreen(10, 12, 20, 255);

        // Draw segments
        const std::size_t n = static_cast<std::size_t>(jointCount);
        for (std::size_t i = 0; i + 1 < n; i++) {
            const std::size_t next = i + 1;
            SDL_SetRenderDrawColor(renderer->renderer, static_cast<Uint8>(100 + (int)i*30), static_cast<Uint8>(200 - (int)i*20), 255, 255);
            // Thick lines
            for(int j=-3; j<=3; j++) {
                SDL_RenderDrawLine(renderer->renderer, (int)joints[i].x + j, (int)joints[i].y, 
                                   (int)joints[next].x + j, (int)joints[next].y);
            }
        }

        // Draw joints
        for (std::size_t i = 0; i < n; i++) {
            SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
            SDL_Rect r = {(int)joints[i].x - 6, (int)joints[i].y - 6, 12, 12};
            SDL_RenderFillRect(renderer->renderer, &r);
        }
        
        // Target indicator
        SDL_SetRenderDrawColor(renderer->renderer, 255, 50, 50, 255);
        SDL_Rect t = {mx - 4, my - 4, 8, 8};
        SDL_RenderFillRect(renderer->renderer, &t);
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static SkeletalIKGame app("Umbra Inverse Kinematics", 700, 700);
#else
    SkeletalIKGame app("Umbra Inverse Kinematics", 700, 700);
#endif
    app.run();
    return 0;
}
