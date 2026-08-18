// Text 2D Showcase for Umbra Engine
// Generates text in 2D with transparency
#include "Engine/Core/Game2D.h"
class Text2DGame : public Game2D
{
    struct FloatingText {
        std::wstring text;
        float x, y;
        float vx, vy;
        float life;
        float maxLife;
        SDL_Color color;
        float size;
    };

    std::vector<FloatingText> floatingTexts;
    float spawnTimer;
    std::shared_ptr<TextDisplay> staticText;
    std::shared_ptr<TextDisplay> titleText;

public:
    Text2DGame() : Game2D("Umbra Text 2D", 800, 600, 20) {
        spawnTimer = 0;
    }

    void initGame() override {
        // Create static title text with transparency info
        titleText = createText(200, 30, "TEXT 2D SHOWCASE - With Transparency!");

        // Create static info text
        staticText = createText(200, 550, "Click to spawn floating text with alpha blending");
    }

    void updateGame(float dt) override {
        // Spawn new floating text periodically
        spawnTimer += dt;
        if (spawnTimer >= 0.5f) {
            spawnTimer = 0;

            FloatingText ft;
            ft.x = 100 + (rand() % 600);
            ft.y = 100 + (rand() % 400);
            ft.vx = (rand() % 100 - 50) * 0.5f;
            ft.vy = -50 - (rand() % 50);
            ft.maxLife = 2.0f;
            ft.life = ft.maxLife;
            ft.size = 16 + (rand() % 20);

            // Random colors with random alpha for transparency
            ft.color = {
                (Uint8)(100 + rand() % 155),
                (Uint8)(100 + rand() % 155),
                (Uint8)(100 + rand() % 155),
                (Uint8)(150 + rand() % 105)  // Alpha: 150-255
            };

            // Random text
            const wchar_t* texts[] = {L"Hello", L"World", L"Text 2D", L"Umbra", L"Alpha", L"Blend", L"2D"};
            ft.text = texts[rand() % 7];

            floatingTexts.push_back(ft);
        }

        // Spawn on click - with transparency effect
        int mx, my;
        if (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(1)) {
            static Uint32 lastClick = 0;
            if (SDL_GetTicks() - lastClick > 200) {
                FloatingText ft;
                ft.x = (float)mx;
                ft.y = (float)my;
                ft.vx = (rand() % 100 - 50) * 0.3f;
                ft.vy = -100 - (rand() % 50);
                ft.maxLife = 3.0f;
                ft.life = ft.maxLife;
                ft.size = 20 + (rand() % 15);
                // Bright yellow with varying alpha
                ft.color = {255, 255, 0, (Uint8)(200 + rand() % 55)};
                ft.text = L"Click!";
                floatingTexts.push_back(ft);
                lastClick = SDL_GetTicks();
            }
        }

        // Update floating texts
        for (auto it = floatingTexts.begin(); it != floatingTexts.end(); ) {
            it->x += it->vx * dt;
            it->y += it->vy * dt;
            it->vy += 20 * dt; // Gravity
            it->life -= dt;

            // Update alpha based on life (fade in/out effect)
            float alphaFactor = 1.0f;
            if (it->life < 0.5f) {
                // Fade out at end
                alphaFactor = it->life / 0.5f;
            } else if (it->life > it->maxLife - 0.5f) {
                // Fade in at start
                alphaFactor = (it->maxLife - it->life) / 0.5f;
            }
            it->color.a = (Uint8)(alphaFactor * 255);

            if (it->life <= 0) {
                it = floatingTexts.erase(it);
            } else {
                ++it;
            }
        }
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        // Enable alpha blending for text rendering
        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_BLEND);

        // Render floating texts with transparency
        for (const auto& ft : floatingTexts) {
            // Draw semi-transparent text background
            int textWidth = (int)(ft.text.length() * ft.size * 0.6f);
            int textHeight = (int)ft.size;
            SDL_Rect bgRect = {(int)ft.x - 5, (int)ft.y - 5, textWidth + 10, textHeight + 10};
            SDL_SetRenderDrawColor(renderer->renderer, 0, 0, 0, (Uint8)(100 * (ft.life / ft.maxLife)));
            SDL_RenderFillRect(renderer->renderer, &bgRect);

            // Draw text using TextWriter (SDL_ttf handles alpha in the texture)
            Rect<float> textBounds(ft.x, ft.y, textWidth, textHeight);
            renderer->getTextWriter()->drawTextToRenderer(ft.text, renderer->renderer, textBounds, "/default.ttf");
        }
        
        // Reset blend mode
        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_NONE);

        // Draw info with transparency demo
        wchar_t info[256];
        swprintf(info, 256, L"Active texts: %zu | Transparency: Alpha blending enabled", floatingTexts.size());
        Rect<float> infoBounds(10, 550, 400, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, infoBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Text2DGame app;
    app.run();
    return 0;
}
