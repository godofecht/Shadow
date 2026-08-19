// 2D Rotation to Cursor Showcase for Umbra Engine
// Demonstrates rotating entities in 2D to follow the cursor
#include "Engine/Core/Game2D.h"
#include <cmath>

class RotationToCursorGame : public Game2D
{
    struct RotatingEntity {
        std::shared_ptr<SimpleSprite> sprite;
        float x, y;
        float angle;
        float followSpeed;
    };
    
    std::vector<RotatingEntity> rotatingEntities;
    Point2D cursorPos;

public:
    RotationToCursorGame() : Game2D("Umbra 2D Rotation to Cursor", 800, 600, 20) {
        cursorPos = Point2D(400, 300);
    }

    void initGame() override {
        // Create entities that rotate to face cursor
        float positions[][2] = {
            {200, 150}, {400, 150}, {600, 150},
            {200, 300}, {600, 300},
            {200, 450}, {400, 450}, {600, 450}
        };
        
        for (int i = 0; i < 8; i++) {
            RotatingEntity entity;
            entity.x = positions[i][0];
            entity.y = positions[i][1];
            entity.angle = 0;
            entity.followSpeed = 5.0f + i * 0.5f;
            
            entity.sprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "entity" + std::to_string(i));
            entity.sprite->setPosition(Point2D(entity.x, entity.y));
            entity.sprite->setSize(60, 60);
            
            rotatingEntities.push_back(entity);
        }
    }

    void updateGame(float dt) override {
        // Get cursor position
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        cursorPos = Point2D((float)mx, (float)my);
        
        // Update each entity
        for (auto& entity : rotatingEntities) {
            // Calculate angle to cursor
            float dx = cursorPos.x - entity.x;
            float dy = cursorPos.y - entity.y;
            float targetAngle = atan2(dy, dx) * 180.0f / 3.14159f;
            
            // Smooth rotation
            float angleDiff = targetAngle - entity.angle;
            
            // Normalize angle difference to [-180, 180]
            while (angleDiff > 180) angleDiff -= 360;
            while (angleDiff < -180) angleDiff += 360;
            
            // Interpolate towards target angle
            entity.angle += angleDiff * entity.followSpeed * dt;
            
            entity.sprite->setAngle(entity.angle);
            entity.sprite->update(dt);
        }
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        // Draw cursor indicator
        SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 0, 255);
        SDL_Rect cursorRect = {(int)cursorPos.x - 5, (int)cursorPos.y - 5, 10, 10};
        SDL_RenderFillRect(renderer->renderer, &cursorRect);
        
        // Draw lines from rotatingEntities to cursor
        SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 100);
        for (const auto& entity : rotatingEntities) {
            SDL_RenderDrawLine(renderer->renderer,
                (int)entity.x, (int)entity.y,
                (int)cursorPos.x, (int)cursorPos.y);
        }
        
        // Render all rotatingEntities
        for (auto& entity : rotatingEntities) {
            entity.sprite->renderAndRunScripts(renderer);
        }
        
        // Draw info text
        wchar_t info[256];
        swprintf(info, 256, 
            L"2D Rotation to Cursor - Entities rotate to face the mouse\n"
            L"Cursor: (%.1f, %.1f)",
            cursorPos.x, cursorPos.y);
        Rect<float> textBounds(10, 10, 400, 60);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    RotationToCursorGame app;
    app.run();
    return 0;
}
