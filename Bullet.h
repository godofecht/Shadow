#ifndef BULLET_H
#define BULLET_H

#include "Sprite.h"
#include <iostream>
#include <cmath>
#include <memory>
#include "Script.h"
#include "Geometry.h"

class BulletControllerScript : public Script
{
public:
    BulletControllerScript(Sprite* sprite, float speed, const Vector2D& direction, float lifetime)
        : sprite(sprite), speed(speed), direction(direction.normalized()), lifetime(lifetime) 
    {}

    void setDirection (const Vector2D& newDirection) { direction = newDirection.normalized(); }
    void start() override { elapsedTime = 0; }

    void update() override
    {
        Vector2D velocity = direction * speed;

        sprite->setPosition (sprite->getBounds().x + velocity.x, sprite->getBounds().y + velocity.y);

        elapsedTime += deltaTime; // deltaTime should be the frame time
        if (elapsedTime >= lifetime)
        {
            destroy();
        }
    }

    virtual ~BulletControllerScript() {}

private:
    Sprite* sprite;
    Vector2D direction;
    float speed;
    float lifetime;
    float elapsedTime = 0;

    void destroy()
    {
        // Handle bullet destruction, like removing it from the game
        sprite->destroy();
    }
};

class Bullet : public Sprite
{
public:
    Bullet (Renderer* renderer, const std::string& _id)
        : Sprite (renderer, "C:/Users/abhis/gamedev/Shadow/fly.png", _id),
          direction (Vector2D (1.0f, 0.0f))
    {
        auto controller = std::make_shared<BulletControllerScript>(this, bulletSpeed, direction, 1.0f);
        attachScript (controller);
        isInitialized = true;
    }

private:
    Vector2D direction;
    float bulletSpeed = 20.0f;
};

#endif // BULLET_H