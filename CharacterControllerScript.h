#ifndef CHARACTERCONTROLLERSCRIPT_H
#define CHARACTERCONTROLLERSCRIPT_H

#include "Script.h"
#include "Sprite.h"
#include <SDL.h>
#include "InputManagement.h"

class CharacterControllerScript : public Script
{
public:
    CharacterControllerScript(Sprite* sprite, int speed);
    virtual void start() override;
    virtual void update() override;

    float health = 100.0f;
    float healthDepletionRate = 2.0f;
    float gravity = 1.0f;
    float jumpSpeed = 2.0f;
    float moveSpeed = 1.0f;

    void handleMovement()
    {
        int x, y;
        sprite->getPosition(x, y);

        auto& inputManager = InputManager::getInstance();
        if (inputManager.isKeyPressed ("W")) y -= jumpSpeed;
        if (inputManager.isKeyPressed ("S")) y += gravity;
        if (inputManager.isKeyPressed ("A")) x -= moveSpeed;
        if (inputManager.isKeyPressed ("D")) x += moveSpeed;

        sprite->setPosition (x, y);

        //apply gravity
        // sprite->setPosition (sprite->getX(), sprite->getY() + 1);
    }

    void handleRotation()
    {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        int spriteX, spriteY;
        sprite->getPosition(spriteX, spriteY);

        int spriteCenterX = spriteX + sprite->getWidth() / 2;
        int spriteCenterY = spriteY + sprite->getHeight() / 2;

        float deltaX = mouseX - spriteCenterX;
        float deltaY = mouseY - spriteCenterY;

        float angle = atan2(deltaY, deltaX) * 180 / M_PI; // Convert radians to degrees

        sprite->setRotation(angle - 115);

        // Draw a line from the sprite to the mouse position
        SDL_Renderer* renderer = sprite->getRenderer();
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red color
        SDL_RenderDrawLine(renderer, spriteCenterX, spriteCenterY, mouseX, mouseY);
 //       SDL_RenderPresent(renderer);
    }

    void handleLifeSupport()
    {
        if (health <= 0.0f)
        {
            health = 0.0f; //just reset it for safety
 //           sprite->destroy();
        }

        //health naturally depletes over time
        health -= healthDepletionRate;
    }

private:
    Sprite* sprite;
    int speed;
};

#endif
