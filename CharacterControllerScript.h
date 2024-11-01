#ifndef CHARACTERCONTROLLERSCRIPT_H
#define CHARACTERCONTROLLERSCRIPT_H

#include "Script.h"
#include "Sprite.h"
#include "Scene.h"
#include "Bullet.h"
#include <SDL.h>
#include "InputManagement.h"
#include <iostream>
#include <unordered_map>
#include "AssetManager.h"
#include <cmath>
#include "Helpers.h"



#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif



class TopDownCharacterControllerScript : public Script
{
public:
    TopDownCharacterControllerScript (Sprite* sprite, int speed)
        : sprite (sprite), speed (speed), lastShotTime (0), cooldownTime (500) // cooldownTime in milliseconds
    {}

    virtual void start() override {}
    
    virtual void update() override
    {
        handleMovement();
        handleRotation();
        handleLifeSupport();
    }

    void handleMovement()
    {
        float x, y;
        sprite->getPosition (x, y);

        auto& inputManager = InputManager::getInstance();
        if (inputManager.isKeyPressed ("W")) y -= moveSpeed;
        if (inputManager.isKeyPressed ("S")) y += moveSpeed;
        if (inputManager.isKeyPressed ("A")) x -= moveSpeed;
        if (inputManager.isKeyPressed ("D")) x += moveSpeed;

        // Check if cooldown has passed since last shot
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastShotTime >= cooldownTime)
        {
            isShooting = false; // Reset shooting state after cooldown
        }

        if (inputManager.isMouseButtonPressed (SDL_BUTTON_LEFT) && !isShooting)
        {
            Scene* scene = sprite->getScene();
            if (scene && scene->isInitialized)
            {
                int mouseX, mouseY;
                SDL_GetMouseState (&mouseX, &mouseY);

                float bulletStartX = x;
                float bulletStartY = y;

                Position origin (bulletStartX, bulletStartY);
                Position target (static_cast<float>(mouseX - sprite->getBounds().width / 2.0f), 
                                 static_cast<float>(mouseY - sprite->getBounds().height / 2.0f));
                
                Vector2D bulletDirection = calculateDirection (origin, target);
                
                auto bullet = (scene->getAssetManager())->createAsset<Bullet>("bullet" + std::to_string (rand()));
                bullet->setPosition (bulletStartX, bulletStartY);

                auto bulletControllerScript = std::dynamic_pointer_cast<BulletControllerScript>(bullet->getScripts()[0]);
                bulletControllerScript->setDirection (bulletDirection);

                scene->addItem (bullet);  // Add bullet to the scene
                isShooting = true;

                // Update last shot time to current time
                lastShotTime = currentTime;
            }
        }

        sprite->setPosition (x, y);
    }

    void handleRotation()
    {
        Vector2D mousePos = getMousePosition();
        Vector2D spriteCenter = getSpriteCenter(sprite);

        Vector2D delta = mousePos - spriteCenter;
        float angle = calculateAngle (delta);
        sprite->setRotation (angle - 115);

        Renderer* renderer = sprite->getRenderer();

        renderer->drawLine (static_cast<int>(spriteCenter.x), 
                            static_cast<int>(spriteCenter.y),
                            static_cast<int>(mousePos.x), 
                            static_cast<int>(mousePos.y));
        health = clamp (health, 0.0f, maxHealth);
    }

    void handleLifeSupport()
    {
        health = clamp (health, 0.0f, maxHealth);
        health -= healthDepletionRate;
    }

    float health = 100.0f;
    float healthDepletionRate = 2.0f;
    float gravity = 1.0f;
    float jumpSpeed = 2.0f;
    float moveSpeed = 5.0f;

    float maxHealth = 100.0f;

private:
    Sprite* sprite;
    int speed;
    Uint32 lastShotTime;
    const Uint32 cooldownTime;
    bool isShooting = false;
};

#endif // CHARACTERCONTROLLERSCRIPT_H
