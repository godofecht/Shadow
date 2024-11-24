#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "InputManagement.h"
#include "Helpers.h"

class SideScrollerCharacterControllerScript : public Script
{
public:
    SideScrollerCharacterControllerScript(Sprite* sprite, int speed)
        : sprite(sprite), moveSpeed(speed), gravity(1.0f), jumpSpeed(-15.0f), isJumping(false), maxHealth(100.0f)
    {}

    virtual void start() override
    {
        velocity = {0.0f, 0.0f};
        health = maxHealth;
    }

    virtual void update() override
    {
        handleMovement();
        applyGravity();
    }

    void handleMovement()
    {
        Point2D position = sprite->getPosition();
        velocity.origin.x = 0.0f; // Reset horizontal velocity each frame

        auto& inputManager = InputManager::getInstance();
        if (inputManager.isKeyPressed ("A"))
        {
            velocity.origin.x = -moveSpeed;
            sprite->setReverse (true);

            static_cast<AnimatedSprite*>(sprite)->setAnimationState ("run");
        }
        if (inputManager.isKeyPressed ("D")) 
        {
            velocity.origin.x = moveSpeed;
            sprite->setReverse (false);

            static_cast<AnimatedSprite*>(sprite)->setAnimationState ("run");
        }
        if (inputManager.isKeyPressed ("W"))
        {
            if (!isJumping)
            {
                velocity.origin.y = jumpSpeed; // Apply jump velocity
                isJumping = true;
            }

            static_cast<AnimatedSprite*>(sprite)->setAnimationState ("jump");
        }

        if (!inputManager.isKeyPressed ("A") && !inputManager.isKeyPressed ("D") && !inputManager.isKeyPressed ("S") && !inputManager.isKeyPressed ("W"))
        {
            static_cast<AnimatedSprite*>(sprite)->setAnimationState ("idle");
        }

        // Update position based on velocity
        position.x += velocity.origin.x;
        position.y += velocity.origin.y;

        // Simulate ground collision
        if (position.y >= groundY)
        {
            position.y = groundY;
            velocity.origin.y = 0.0f;
            isJumping = false;
        }

        sprite->setPosition (position);
    }

    void applyGravity() { velocity.origin.y += gravity; } // Apply gravity to vertical velocity

private:
    Sprite* sprite; // Changed from SimpleSprite to Sprite
    Vector2D velocity; // For handling movement and gravity
    float health;
    float maxHealth;
    float gravity;
    float jumpSpeed;
    float moveSpeed = 5.0f;
    bool isJumping;
    const float groundY = 400.0f; // Example ground level
};


class Player : public AnimatedSprite
{
    public:

    Player (Renderer* renderer, const std::string& _id) : AnimatedSprite (renderer, "player")
    {
        std::string s = "W:/Downloads/Free 3 Cyberpunk Sprites Pixel Art/2 Punk/";

        // Load animations
        // addAnimation ("run",  new Animation (renderer->renderer, new Image (s + "Punk_run.png"), 48, 48, 6));
        // addAnimation ("idle", new Animation (renderer->renderer, new Image (s + "Punk_idle.png"), 48, 48, 4));
        // addAnimation ("jump", new Animation (renderer->renderer, new Image (s + "Punk_jump.png"), 48, 48, 4));

        addAnimation ("run",  s + "Punk_run.png",  48, 48, 6);
        addAnimation ("idle", s + "Punk_idle.png", 48, 48, 4);
        addAnimation ("jump", s + "Punk_jump.png", 48, 48, 4);

        // Set initial state
        setAnimationState("run");

        auto controller = std::make_shared<SideScrollerCharacterControllerScript>(this, 20);
        attachScript (controller);        

        isInitialized = true;
    }
};

class HUDSprite : public Sprite
{
public:
    HUDSprite(Renderer* renderer, const std::string& _id)
        : Sprite(renderer, _id), bounds({0, 0, 0, 0}), alpha(255) {}

    virtual ~HUDSprite() = default;

    void setBounds(const Rect<float>& rect) { bounds = rect; }
    const Rect<float>& getBounds() const { return bounds; }

    virtual void renderAndRunScripts() override
    {
        if (!isActive || !texture)
            return;

        const SDL_Rect destRect = bounds.getSDLRect();

        SDL_SetTextureAlphaMod (texture, alpha);
        SDL_RenderCopy (renderer->renderer, texture, nullptr, &destRect);

        for (const auto& script : scripts)
            script->update();
    }

    void setTransparency (Uint8 _alpha) { alpha = _alpha; }

protected:
    Rect<float> bounds; // Position and size
    Uint8 alpha;
};

// HealthBar Derived from HUDSprite
class HealthBar : public HUDSprite
{
public:
    HealthBar(Renderer* renderer, const std::string& _id)
        : HUDSprite(renderer, _id), maxHealth(100), currentHealth(100) {}

    void setHealth(float health)
    {
        currentHealth = std::clamp(health, 0.0f, maxHealth);
    }

    void setMaxHealth(float health)
    {
        maxHealth = std::max(1.0f, health);
        currentHealth = std::min(currentHealth, maxHealth);
    }

    virtual void renderAndRunScripts() override
    {
        if (!isActive)
            return;

        // Render background bar
        SDL_Rect background = bounds.getSDLRect();

        SDL_SetRenderDrawColor(renderer->renderer, 50, 50, 50, alpha);
        SDL_RenderFillRect(renderer->renderer, &background);

        // Render health foreground
        SDL_Rect foreground = {
            static_cast<int>(bounds.x),
            static_cast<int>(bounds.y),
            static_cast<int>(bounds.width * (currentHealth / maxHealth)),
            static_cast<int>(bounds.height)
        };
        SDL_SetRenderDrawColor(renderer->renderer, 0, 255, 0, alpha);
        SDL_RenderFillRect(renderer->renderer, &foreground);
    }

private:
    float maxHealth;
    float currentHealth;
};