// Sprite Showcase for Umbra Engine
// Renders a sprite
#include "Engine/Core/Game2D.h"
class SpriteGame : public Game2D
{
    std::shared_ptr<SimpleSprite> mainSprite;
    std::vector<std::shared_ptr<SimpleSprite>> sprites;
    float spriteX, spriteY;

public:
    SpriteGame() : Game2D("Umbra Sprite", 800, 600, 20) {
        spriteX = 400;
        spriteY = 300;
    }

    void initGame() override {
        // Create main sprite
        mainSprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "main");
        mainSprite->setPosition(Point2D(spriteX, spriteY));
        mainSprite->setSize(100, 100);
        
        // Create surrounding sprites (angles is a member array)
        float radius = 200;
        
        for (int i = 0; i < 5; i++) {
            float angle = angles[i] * 3.14159f / 180.0f;
            float x = 400 + cos(angle) * radius;
            float y = 300 + sin(angle) * radius;
            
            auto sprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "satellite" + std::to_string(i));
            sprite->setPosition(Point2D(x, y));
            sprite->setSize(60, 60);
            
            sprites.push_back(sprite);
        }
    }

    void updateGame(float dt) override {
        float time = SDL_GetTicks() * 0.001f;
        
        // Rotate satellite sprites
        for (std::size_t i = 0; i < 5; i++) {
            float angle = (angles[i] + time * 50) * 3.14159f / 180.0f;
            float radius = 200;
            float x = 400 + cos(angle) * radius;
            float y = 300 + sin(angle) * radius;
            
            sprites[i]->setPosition(Point2D(x, y));
            sprites[i]->setAngle(angle * 180.0f / 3.14159f);
            sprites[i]->update(dt);
        }
        
        mainSprite->update(dt);
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        // Render main sprite
        mainSprite->renderAndRunScripts(renderer);
        
        // Render satellite sprites
        for (auto& sprite : sprites) {
            sprite->renderAndRunScripts(renderer);
        }
        
        // Draw orbit path (approximated with points)
        SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 50);
        for (int angle = 0; angle < 360; angle += 2) {
            float rad = angle * 3.14159f / 180.0f;
            int px = 400 + (int)(cos(rad) * 200);
            int py = 300 + (int)(sin(rad) * 200);
            SDL_RenderDrawPoint(renderer->renderer, px, py);
        }
        
        // Draw info text
        std::wstring info = L"Sprite - Basic sprite rendering with orbiting satellites";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }

private:
    float angles[5] = {0, 72, 144, 216, 288};
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    SpriteGame app;
    app.run();
    return 0;
}
