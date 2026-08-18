// Sprite Scale Showcase for Umbra Engine
// Shows how a sprite can be scaled into a rectangle while keeping the aspect ratio
#include "Engine/Core/Game2D.h"
#include <cmath>

class SpriteScaleGame : public Game2D
{
    struct ScalableSprite {
        std::shared_ptr<SimpleSprite> sprite;
        float originalWidth, originalHeight;
        float x, y;
        float targetWidth, targetHeight;
        float scale;
        bool keepAspectRatio;
        std::string name;
    };
    
    std::vector<ScalableSprite> sprites;
    float time;

public:
    SpriteScaleGame() : Game2D("Umbra Sprite Scale", 800, 600, 20) {
        time = 0;
    }

    void initGame() override {
        // Create sprites with different scale modes
        struct SpriteConfig {
            float x, y;
            float targetW, targetH;
            bool keepAR;
            const char* name;
        };
        
        SpriteConfig configs[] = {
            {100, 100, 80, 80, true, "1:1 (AR)"},
            {300, 100, 120, 60, true, "2:1 (AR)"},
            {550, 100, 60, 120, true, "1:2 (AR)"},
            {150, 300, 150, 100, false, "Stretch"},
            {450, 300, 100, 150, false, "Stretch"},
            {350, 480, 100, 100, true, "Animated Scale"}
        };
        
        for (int i = 0; i < 6; i++) {
            ScalableSprite ss;
            ss.x = configs[i].x;
            ss.y = configs[i].y;
            ss.targetWidth = configs[i].targetW;
            ss.targetHeight = configs[i].targetH;
            ss.keepAspectRatio = configs[i].keepAR;
            ss.name = configs[i].name;
            ss.originalWidth = 100;
            ss.originalHeight = 100;
            ss.scale = 1.0f;
            
            ss.sprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "scale" + std::to_string(i));
            ss.sprite->setPosition(Point2D(ss.x, ss.y));
            ss.sprite->setSize(ss.originalWidth, ss.originalHeight);
            
            sprites.push_back(ss);
        }
    }

    void updateGame(float dt) override {
        time += dt;
        
        // Animate the last sprite
        sprites[5].scale = 0.8f + 0.4f * sin(time * 3.0f);
        
        for (auto& ss : sprites) {
            float newWidth, newHeight;
            
            if (ss.keepAspectRatio) {
                // Calculate scale to fit within target while keeping aspect ratio
                float scaleX = ss.targetWidth / ss.originalWidth;
                float scaleY = ss.targetHeight / ss.originalHeight;
                float uniformScale = std::min(scaleX, scaleY);
                
                if (&ss == &sprites[5]) {
                    uniformScale *= ss.scale;
                }
                
                newWidth = ss.originalWidth * uniformScale;
                newHeight = ss.originalHeight * uniformScale;
            } else {
                // Stretch to fit
                newWidth = ss.targetWidth;
                newHeight = ss.targetHeight;
            }
            
            ss.sprite->setSize(newWidth, newHeight);
            ss.sprite->update(dt);
        }
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        for (auto& ss : sprites) {
            // Draw target rectangle (dashed outline)
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            SDL_Rect targetRect = {(int)ss.x, (int)ss.y, (int)ss.targetWidth, (int)ss.targetHeight};
            SDL_RenderDrawRect(renderer->renderer, &targetRect);
            
            // Draw actual sprite bounds
            SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 0, 255);
            float width, height;
            // Get current sprite size (would need getter in SimpleSprite)
            width = ss.sprite->getWidth();
            height = ss.sprite->getHeight();
            SDL_Rect actualRect = {(int)ss.x, (int)ss.y, (int)width, (int)height};
            SDL_RenderDrawRect(renderer->renderer, &actualRect);
            
            // Draw label
            wchar_t label[64];
            swprintf(label, 64, L"%s (%.0fx%.0f)", ss.name.c_str(), width, height);
            Rect<float> textBounds(ss.x, ss.y + ss.targetHeight + 5, 150, 30);
            renderer->getTextWriter()->drawTextToRenderer(label, renderer->renderer, textBounds, "/default.ttf");
            
            ss.sprite->renderAndRunScripts(renderer);
        }
        
        // Draw info text
        wchar_t info[256];
        swprintf(info, 256, 
            L"Sprite Scale - Aspect ratio preservation demonstration\n"
            L"Gray = Target bounds, Yellow = Actual sprite size");
        Rect<float> textBounds(10, 10, 500, 50);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    SpriteScaleGame app;
    app.run();
    return 0;
}
