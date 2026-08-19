// Tweening & Easing Showcase for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include "Engine/Core/Helpers.h"
#include <vector>
#include <string>

struct TweeningObject {
    std::string name;
    float startY, endY;
    float (*easingFunc)(float);
    SDL_Color color;
};

class TweeningGame : public Game
{
    std::vector<TweeningObject> objects;
    float progress = 0;
    bool forward = true;

public:
    TweeningGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {}

    void onStart() override {
        objects = {
            {"Linear", 100, 600, [](float t){ return t; }, {255, 255, 255, 255}},
            {"InQuad", 100, 600, Easing::InQuad, {255, 100, 100, 255}},
            {"OutQuad", 100, 600, Easing::OutQuad, {100, 255, 100, 255}},
            {"InOutQuad", 100, 600, Easing::InOutQuad, {100, 100, 255, 255}},
            {"OutElastic", 100, 600, Easing::OutElastic, {255, 255, 100, 255}},
            {"OutBounce", 100, 600, Easing::OutBounce, {255, 100, 255, 255}}
        };
    }

    void update() override {
        float speed = 0.005f;
        if (forward) {
            progress += speed;
            if (progress >= 1.0f) { progress = 1.0f; forward = false; }
        } else {
            progress -= speed;
            if (progress <= 0.0f) { progress = 0.0f; forward = true; }
        }

        Renderer* renderer = getRenderer();
        renderer->clearScreen(15, 15, 20, 255);

        int spacing = 700 / static_cast<int>(objects.size() + 1);
        for (size_t i = 0; i < objects.size(); ++i) {
            float t = objects[i].easingFunc(progress);
            float currentY = Easing::Lerp(objects[i].startY, objects[i].endY, t);
            
            int x = static_cast<int>(i + 1) * spacing;
            
            // Draw track
            SDL_SetRenderDrawColor(renderer->renderer, 40, 40, 50, 255);
            SDL_RenderDrawLine(renderer->renderer, x, 100, x, 600);

            // Draw object
            SDL_SetRenderDrawColor(renderer->renderer, objects[i].color.r, objects[i].color.g, objects[i].color.b, 255);
            SDL_Rect r = { x - 15, (int)currentY - 15, 30, 30 };
            SDL_RenderFillRect(renderer->renderer, &r);
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static TweeningGame app("Umbra Tweening Showcase", 700, 700);
#else
    TweeningGame app("Umbra Tweening Showcase", 700, 700);
#endif
    app.run();
    return 0;
}
