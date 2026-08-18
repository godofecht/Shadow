// Move Sprite Showcase for Umbra Engine
// Changes the transform of a sprite
#include "Engine/Core/Game2D.h"
class MoveSpriteGame : public Game2D
{
    struct MovingSprite {
        std::shared_ptr<SimpleSprite> sprite;
        float vx, vy;
        float x, y;
    };
    
    std::vector<MovingSprite> sprites;
    std::shared_ptr<SimpleSprite> playerSprite;
    float playerX, playerY;
    float playerSpeed;

public:
    MoveSpriteGame() : Game2D("Umbra Move Sprite", 800, 600, 20) {
        playerX = 400;
        playerY = 300;
        playerSpeed = 200;
    }

    void initGame() override {
        // Create bouncing sprites
        float positions[][2] = {
            {100, 100}, {300, 150}, {500, 200}, {200, 400}, {600, 350}
        };
        float velocities[][2] = {
            {100, 80}, {-120, 60}, {80, -100}, {-90, -70}, {110, -90}
        };
        
        for (int i = 0; i < 5; i++) {
            MovingSprite ms;
            ms.x = positions[i][0];
            ms.y = positions[i][1];
            ms.vx = velocities[i][0];
            ms.vy = velocities[i][1];
            
            // Create a simple colored rectangle sprite
            ms.sprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "sprite" + std::to_string(i));
            ms.sprite->setPosition(Point2D(ms.x, ms.y));
            ms.sprite->setSize(60, 60);
            
            sprites.push_back(ms);
        }
        
        // Create player sprite
        playerSprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "player");
        playerSprite->setPosition(Point2D(playerX, playerY));
        playerSprite->setSize(80, 80);
    }

    void updateGame(float dt) override {
        // Update bouncing sprites
        for (auto& ms : sprites) {
            ms.x += ms.vx * dt;
            ms.y += ms.vy * dt;
            
            // Bounce off walls
            if (ms.x < 0 || ms.x > 740) {
                ms.vx = -ms.vx;
                ms.x = std::max(0.0f, std::min(ms.x, 740.0f));
            }
            if (ms.y < 0 || ms.y > 540) {
                ms.vy = -ms.vy;
                ms.y = std::max(0.0f, std::min(ms.y, 540.0f));
            }
            
            ms.sprite->setPosition(Point2D(ms.x, ms.y));
        }
        
        // Player movement with keyboard
        if (input.isKeyPressed(KEY_LEFT) || input.isKeyPressed(KEY_A)) {
            playerX -= playerSpeed * dt;
        }
        if (input.isKeyPressed(KEY_RIGHT) || input.isKeyPressed(KEY_D)) {
            playerX += playerSpeed * dt;
        }
        if (input.isKeyPressed(KEY_UP) || input.isKeyPressed(KEY_W)) {
            playerY -= playerSpeed * dt;
        }
        if (input.isKeyPressed(KEY_DOWN) || input.isKeyPressed(KEY_S)) {
            playerY += playerSpeed * dt;
        }
        
        // Clamp player position
        playerX = std::max(0.0f, std::min(playerX, 720.0f));
        playerY = std::max(0.0f, std::min(playerY, 520.0f));
        
        playerSprite->setPosition(Point2D(playerX, playerY));
        
        // Mouse follow sprite
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        
        // Update all sprites
        for (auto& ms : sprites) {
            ms.sprite->update(dt);
        }
        playerSprite->update(dt);
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        // Render all sprites
        for (auto& ms : sprites) {
            ms.sprite->renderAndRunScripts(renderer);
        }
        playerSprite->renderAndRunScripts(renderer);
        
        // Draw info text
        wchar_t info[256];
        swprintf(info, 256, 
            L"Move Sprite - WASD/Arrow Keys to move player\n"
            L"Player Position: (%.1f, %.1f)",
            playerX, playerY);
        Rect<float> textBounds(10, 10, 400, 60);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    MoveSpriteGame app;
    app.run();
    return 0;
}
