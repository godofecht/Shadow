// Bunnymark Performance Test for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include <vector>

struct Bunny {
    float x, y;
    float vx, vy;
};

class BunnymarkGame : public Game
{
    std::vector<Bunny> bunnies;
    SDL_Texture* bunnyTexture = nullptr;
    int bunnyCount = 0;

public:
    BunnymarkGame(const char* title, int width, int height) : Game(title, width, height) {}

    void onStart() override {
        // Load fly.png as our "bunny"
        SDL_Surface* surface = IMG_Load("fly.png");
        if (surface) {
            bunnyTexture = SDL_CreateTextureFromSurface(getRenderer()->renderer, surface);
            SDL_FreeSurface(surface);
        }
        addBunnies(100);
    }

    void addBunnies(int count) {
        for (int i = 0; i < count; i++) {
            bunnies.push_back({ 350.0f, 350.0f, (float)(rand()%10 - 5), (float)(rand()%10 - 5) });
        }
        bunnyCount = static_cast<int>(bunnies.size());
        std::cout << "Bunnies: " << bunnyCount << '\n';
    }

    void update() override {
        // Update bunnies
        for (auto& b : bunnies) {
            b.x += b.vx;
            b.y += b.vy;

            if (b.x < 0 || b.x > 700) b.vx *= -1;
            if (b.y < 0 || b.y > 700) b.vy *= -1;
        }

        Renderer* renderer = getRenderer();
        renderer->clearScreen(20, 25, 30, 255);

        // Render all bunnies
        if (bunnyTexture) {
            SDL_Rect dest = { 0, 0, 32, 32 };
            for (const auto& b : bunnies) {
                dest.x = (int)b.x - 16;
                dest.y = (int)b.y - 16;
                SDL_RenderCopy(renderer->renderer, bunnyTexture, nullptr, &dest);
            }
        }

        // Click to add more
        if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(1)) {
            addBunnies(100);
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static BunnymarkGame app("Umbra Bunnymark", 700, 700);
#else
    BunnymarkGame app("Umbra Bunnymark", 700, 700);
#endif
    app.run();
    return 0;
}
