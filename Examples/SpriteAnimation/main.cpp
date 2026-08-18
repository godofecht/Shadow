// Sprite Animation Showcase for Umbra Engine
// Animates a sprite in response to an event
#include "Engine/Core/Game2D.h"
class SpriteAnimationGame : public Game2D
{
    struct AnimatedSprite {
        std::shared_ptr<SimpleSprite> sprite;
        int currentFrame;
        int totalFrames;
        float frameTime;
        float animationTimer;
        bool isAnimating;
        float x, y;
    };
    
    std::vector<AnimatedSprite> animatedSprites;
    std::shared_ptr<SimpleSprite> triggerSprite;
    float triggerX, triggerY;
    bool triggerActive;

public:
    SpriteAnimationGame() : Game2D("Umbra Sprite Animation", 800, 600, 20) {
        triggerX = 400;
        triggerY = 300;
        triggerActive = false;
    }

    void initGame() override {
        // Create animated sprites
        float positions[][2] = {
            {200, 200}, {400, 200}, {600, 200},
            {200, 400}, {400, 400}, {600, 400}
        };
        
        for (int i = 0; i < 6; i++) {
            AnimatedSprite animSprite;
            animSprite.x = positions[i][0];
            animSprite.y = positions[i][1];
            animSprite.currentFrame = 0;
            animSprite.totalFrames = 4;
            animSprite.frameTime = 0.15f;
            animSprite.animationTimer = 0;
            animSprite.isAnimating = false;
            
            animSprite.sprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "anim" + std::to_string(i));
            animSprite.sprite->setPosition(Point2D(animSprite.x, animSprite.y));
            animSprite.sprite->setSize(70, 70);
            
            animatedSprites.push_back(animSprite);
        }
        
        // Create trigger sprite
        triggerSprite = std::make_shared<SimpleSprite>(getRenderer(), "player.png", "trigger");
        triggerSprite->setPosition(Point2D(triggerX, triggerY));
        triggerSprite->setSize(100, 100);
    }

    void updateGame(float dt) override {
        float dtLocal = dt;   // local copy (avoids shadowing the base member)
        
        // Check for collision with trigger
        int mx, my;
        Uint32 mouseState = SDL_GetMouseState(&mx, &my);
        
        // Trigger animation on click
        if (mouseState & SDL_BUTTON(1)) {
            if (!triggerActive) {
                triggerActive = true;
                // Start animation on all sprites
                for (auto& animSprite : animatedSprites) {
                    animSprite.isAnimating = true;
                    animSprite.currentFrame = 0;
                    animSprite.animationTimer = 0;
                }
            }
        } else {
            triggerActive = false;
        }
        
        // Update animations
        for (auto& animSprite : animatedSprites) {
            if (animSprite.isAnimating) {
                animSprite.animationTimer += dtLocal;
                
                if (animSprite.animationTimer >= animSprite.frameTime) {
                    animSprite.animationTimer -= animSprite.frameTime;
                    animSprite.currentFrame++;
                    
                    if (animSprite.currentFrame >= animSprite.totalFrames) {
                        animSprite.currentFrame = 0;
                        animSprite.isAnimating = false;
                    }
                    
                    // Apply frame-based transformation
                    float scale = 1.0f + 0.2f * sin(animSprite.currentFrame * 3.14159f / 2.0f);
                    animSprite.sprite->setSize(70 * scale, 70 * scale);
                    
                    // Rotate based on frame
                    float rotation = animSprite.currentFrame * 90.0f;
                    animSprite.sprite->setAngle(rotation);
                }
            }
            
            animSprite.sprite->update(dtLocal);
        }
        
        triggerSprite->update(dtLocal);
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        // Render trigger sprite
        triggerSprite->renderAndRunScripts(renderer);
        
        // Render animated sprites
        for (auto& animSprite : animatedSprites) {
            animSprite.sprite->renderAndRunScripts(renderer);
            
            // Draw frame indicator
            SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
            for (int i = 0; i < animSprite.totalFrames; i++) {
                int dotX = (int)animSprite.x + i * 15 - (animSprite.totalFrames * 15) / 2;
                int dotY = (int)animSprite.y + 45;
                SDL_Rect dot = {dotX, dotY, 10, 10};
                if (i == animSprite.currentFrame) {
                    SDL_RenderFillRect(renderer->renderer, &dot);
                } else {
                    SDL_RenderDrawRect(renderer->renderer, &dot);
                }
            }
        }
        
        // Draw info text
        wchar_t info[256];
        swprintf(info, 256, 
            L"Sprite Animation - Click to trigger animation\n"
            L"Frame-based sprite animation with rotation and scale");
        Rect<float> textBounds(10, 10, 500, 50);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    SpriteAnimationGame app;
    app.run();
    return 0;
}
