#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "InputManagement.h"
#include "Helpers.h"

class TopDownCharacterControllerScript : public Script
{
public:
    TopDownCharacterControllerScript(Sprite* sprite, float speed)
        : sprite(sprite), moveSpeed(speed), maxHealth(100.0f)
    {}

    virtual void start() override
    {
        velocity = {0.0f, 0.0f};
        health = maxHealth;
    }

    virtual void update() override
    {
        handleMovement();
    }

    void handleMovement()
    {
        Point2D position = sprite->getPosition();
        velocity.origin.x = 0.0f; // Reset horizontal velocity each frame
        velocity.origin.y = 0.0f; // Reset vertical velocity each frame

        auto& inputManager = InputManager::getInstance();
        bool isMoving = false;

        // Diagonal movement support
        if (inputManager.isKeyPressed("W") && inputManager.isKeyPressed("A"))
        {
            // Northwest movement
            velocity.origin.x = -moveSpeed * 0.707f;
            velocity.origin.y = -moveSpeed * 0.707f;
            sprite->setReverse(true);
            static_cast<AnimatedSprite*>(sprite)->setAnimationState("move");
            isMoving = true;
        }
        else if (inputManager.isKeyPressed("W") && inputManager.isKeyPressed("D"))
        {
            // Northeast movement
            velocity.origin.x = moveSpeed * 0.707f;
            velocity.origin.y = -moveSpeed * 0.707f;
            sprite->setReverse(false);
            static_cast<AnimatedSprite*>(sprite)->setAnimationState("move");
            isMoving = true;
        }
        else if (inputManager.isKeyPressed("S") && inputManager.isKeyPressed("A"))
        {
            // Southwest movement
            velocity.origin.x = -moveSpeed * 0.707f;
            velocity.origin.y = moveSpeed * 0.707f;
            sprite->setReverse(true);
            static_cast<AnimatedSprite*>(sprite)->setAnimationState("move");
            isMoving = true;
        }
        else if (inputManager.isKeyPressed("S") && inputManager.isKeyPressed("D"))
        {
            // Southeast movement
            velocity.origin.x = moveSpeed * 0.707f;
            velocity.origin.y = moveSpeed * 0.707f;
            sprite->setReverse(false);
            static_cast<AnimatedSprite*>(sprite)->setAnimationState("move");
            isMoving = true;
        }
        // Cardinal directions
        else if (inputManager.isKeyPressed("W"))
        {
            velocity.origin.y = -moveSpeed;
            static_cast<AnimatedSprite*>(sprite)->setAnimationState("move_north");
            isMoving = true;
        }
        else if (inputManager.isKeyPressed("S"))
        {
            velocity.origin.y = moveSpeed;
            static_cast<AnimatedSprite*>(sprite)->setAnimationState("move_south");
            isMoving = true;
        }
        else if (inputManager.isKeyPressed("A"))
        {
            velocity.origin.x = -moveSpeed;
            sprite->setReverse(true);
            static_cast<AnimatedSprite*>(sprite)->setAnimationState("move_west");
            isMoving = true;
        }
        else if (inputManager.isKeyPressed("D"))
        {
            velocity.origin.x = moveSpeed;
            sprite->setReverse(false);
            static_cast<AnimatedSprite*>(sprite)->setAnimationState("move_east");
            isMoving = true;
        }

        // If no movement keys are pressed, set to idle
        if (!isMoving)
        {
            static_cast<AnimatedSprite*>(sprite)->setAnimationState("idle");
        }

        // Update position based on velocity
        position.x += velocity.origin.x;
        position.y += velocity.origin.y;

        sprite->setPosition(position);
    }

private:
    Sprite* sprite;
    Vector2D velocity;
    float health;
    float maxHealth;
    float moveSpeed = 5.0f;
};

class Player : public AnimatedSprite
{
public:
    Player(Renderer* renderer, const std::string& _id) : AnimatedSprite(renderer, "player")
    {
        std::string s = "W:/Downloads/Free 3 Cyberpunk Sprites Pixel Art/2 Punk/";

        // Top-down animations for different directions
        addAnimation("idle", s + "Punk_idle.png", 48, 48, 4);
        addAnimation("move_north", s + "Punk_run.png", 48, 48, 6);
        addAnimation("move_south", s + "Punk_run.png", 48, 48, 6);
        addAnimation("move_east", s + "Punk_run.png", 48, 48, 6);
        addAnimation("move_west", s + "Punk_run.png", 48, 48, 6);
        addAnimation("move", s + "Punk_run.png", 48, 48, 6); // Diagonal movement

        // Set initial state
        setAnimationState("idle");

        auto controller = std::make_shared<TopDownCharacterControllerScript>(this, 5.0f);
        attachScript(controller);        

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