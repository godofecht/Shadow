#ifndef FLYCONTROLLERSCRIPT_H
#define FLYCONTROLLERSCRIPT_H

#include "Script.h"
#include "Sprite.h"
#include <SDL.h>
#include "InputManagement.h"

class FlyControllerScript : public Script
{
public:
    FlyControllerScript (SimpleSprite* sprite, int speed) : 
        sprite (sprite), speed (speed)
    {
    }

    void start() override {}
    
    void update() override
    {
        handleLifeSupport();
        handleMovement();
        handleRotation();
    }

    float health = 100.0f;
    float healthDepletionRate = 2.0f;
    float gravity = 1.0f;
    float jumpSpeed = 2.0f;
    float moveSpeed = 1.0f;

    void handleMovement()
    {
        Point2D position = sprite->getPosition();
        float& x = position.x;
        float& y = position.y;

        // Generate random movement
        int randomDirection = rand() % 4; // 0: up, 1: down, 2: left, 3: right
        switch (randomDirection)
        {
        case 0: // up
            y -= moveSpeed;
            break;
        case 1: // down
            y += moveSpeed;
            break;
        case 2: // left
            x -= moveSpeed;
            break;
        case 3: // right
            x += moveSpeed;
            break;
        }

        sprite->setPosition (position);

        //apply gravity
        // sprite->setPosition (sprite->getX(), sprite->getY() + 1);
    }

    void handleRotation()
    {
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
    SimpleSprite* sprite;
    int speed;
};

#endif
