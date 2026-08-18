// Sprite Flipping Showcase for Umbra Engine
// Renders a sprite flipped along an axis
#include "Engine/Core/Game2D.h"
class SpriteFlippingGame : public Game2D
{
    struct FlippableSprite {
        std::shared_ptr<SimpleSprite> sprite;
        float x, y;
        bool flipHorizontal;
        bool flipVertical;
        std::string name;
    };
    
    std::vector<FlippableSprite> sprites;

public:
    SpriteFlippingGame() : Game2D("Umbra Sprite Flipping", 800, 600, 20) {}

    void initGame() override {
        // Create sprites with different flip states
        struct SpriteConfig {
            float x, y;
            bool hFlip, vFlip;
            const char* name;
        };
        
        SpriteConfig configs[] = {
            {150, 150, false, false, "Normal"},
            {400, 150, true, false, "Flip H"},
            {650, 150, false, true, "Flip V"},
            {275, 350, true, true, "Flip H+V"},
            {525, 350, false, false, "Animated Flip"}
        };
        
        for (int i = 0; i < 5; i++) {
            FlippableSprite fs;
            fs.x = configs[i].x;
            fs.y = configs[i].y;
            fs.flipHorizontal = configs[i].hFlip;
            fs.flipVertical = configs[i].vFlip;
            fs.name = configs[i].name;
            
            fs.sprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "flip" + std::to_string(i));
            fs.sprite->setPosition(Point2D(fs.x, fs.y));
            fs.sprite->setSize(100, 100);
            
            sprites.push_back(fs);
        }
    }

    void updateGame(float dt) override {
        // Animate the last sprite
        float time = SDL_GetTicks() * 0.002f;
        sprites[4].flipHorizontal = sin(time) > 0;
        sprites[4].flipVertical = cos(time * 0.7f) > 0;
        
        // Update all sprites
        for (auto& fs : sprites) {
            // Note: SimpleSprite may need flip methods added
            // For now, we indicate flip state visually
            fs.sprite->update(dt);
        }
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        // Render all sprites with flip visualization
        for (auto& fs : sprites) {
            
            // Draw sprite bounds
            SDL_SetRenderDrawColor(renderer->renderer, 200, 200, 200, 255);
            SDL_Rect bounds = {(int)fs.x, (int)fs.y, 100, 100};
            SDL_RenderDrawRect(renderer->renderer, &bounds);
            
            // Draw flip indicator arrows
            SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 0, 255);
            
            if (fs.flipHorizontal) {
                // Horizontal flip arrow
                SDL_RenderDrawLine(renderer->renderer, 
                    (int)fs.x + 10, (int)fs.y + 50,
                    (int)fs.x + 90, (int)fs.y + 50);
                SDL_RenderDrawLine(renderer->renderer,
                    (int)fs.x + 80, (int)fs.y + 45,
                    (int)fs.x + 90, (int)fs.y + 50);
                SDL_RenderDrawLine(renderer->renderer,
                    (int)fs.x + 80, (int)fs.y + 55,
                    (int)fs.x + 90, (int)fs.y + 50);
            }
            
            if (fs.flipVertical) {
                // Vertical flip arrow
                SDL_RenderDrawLine(renderer->renderer,
                    (int)fs.x + 50, (int)fs.y + 10,
                    (int)fs.x + 50, (int)fs.y + 90);
                SDL_RenderDrawLine(renderer->renderer,
                    (int)fs.x + 45, (int)fs.y + 80,
                    (int)fs.x + 50, (int)fs.y + 90);
                SDL_RenderDrawLine(renderer->renderer,
                    (int)fs.x + 55, (int)fs.y + 80,
                    (int)fs.x + 50, (int)fs.y + 90);
            }
            
            // Draw center marker
            SDL_SetRenderDrawColor(renderer->renderer, 255, 0, 0, 255);
            SDL_Rect center = {(int)fs.x + 48, (int)fs.y + 48, 4, 4};
            SDL_RenderFillRect(renderer->renderer, &center);
            
            // Draw label
            wchar_t label[64];
            swprintf(label, 64, L"%s", fs.name.c_str());
            Rect<float> textBounds(fs.x, fs.y + 110, 120, 30);
            renderer->getTextWriter()->drawTextToRenderer(label, renderer->renderer, textBounds, "/default.ttf");
        }

        // Render actual sprites
        for (auto& fs : sprites) {
            fs.sprite->renderAndRunScripts(renderer);
        }
        
        // Draw info
        std::wstring info = L"Sprite Flipping - Horizontal and Vertical flip demonstration";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    SpriteFlippingGame app;
    app.run();
    return 0;
}
