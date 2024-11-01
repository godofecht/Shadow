#ifndef SDLAPP_H
#define SDLAPP_H

#include <SDL.h>
#include <SDL_image.h>
#include <memory>
#include <string>
#include "AssetManager.h"
#include "CharacterControllerScript.h"
#include "FlyControllerScript.h"
#include "TextWriter.h"


class Enemy : public Sprite
{
public:
    Enemy(Renderer* renderer, const std::string& id)
        : Sprite(renderer, "", id), health(health)
    {
        setPosition(startX, startY);
        setSize(40, 40); // Set default enemy size
        std::cout << "Enemy created: " << id << " at position (" << startX << ", " << startY << ")" << std::endl;
    }

    void setTarget(Sprite* target)
    {
        this->target = target;
    }

    // virtual void update() override
    // {
    //     if (health <= 0)
    //     {
    //         destroy();
    //         return;
    //     }

    //     // Basic movement: follow target if one is set
    //     if (target != nullptr)
    //     {
    //         moveTowardsTarget();
    //     }
    // }

    void takeDamage(int amount)
    {
        health -= amount;
        std::cout << "Enemy " << getId() << " took " << amount << " damage. Health: " << health << std::endl;
        if (health <= 0)
        {
            std::cout << "Enemy " << getId() << " has been destroyed." << std::endl;
            destroy();
        }
    }

    int getHealth() const { return health; }

private:
    int startX = 0;
    int startY = 0;
    int health = 100;
    Sprite* target = nullptr;

    void moveTowardsTarget()
    {
        if (target == nullptr)
        {
            return;
        }

        float targetX, targetY;
        target->getPosition(targetX, targetY);

        float enemyX, enemyY;
        getPosition(enemyX, enemyY);

        float deltaX = targetX - enemyX;
        float deltaY = targetY - enemyY;

        // Normalize the direction vector and move towards the target
        float length = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        if (length != 0)
        {
            deltaX /= length;
            deltaY /= length;
        }

        float moveSpeed = 1.0f; // Set movement speed
        setPosition(enemyX + deltaX * moveSpeed, enemyY + deltaY * moveSpeed);
    }
};

class Player : public Sprite
{
public:
    Player(Renderer* renderer, const std::string& id)
        : Sprite (renderer, "C:/Users/abhis/gamedev/Shadow/fly.png", id)
    {
        std::cout << "Creating object: " << getId() << std::endl;
        auto controller = std::make_shared<TopDownCharacterControllerScript>(this, 5);
        setImage ("C:/Users/abhis/gamedev/Shadow/fly.png");
        attachScript(controller);        
        isInitialized = true;
    }
};

class FPSCounter 
{
public:
    FPSCounter() : start_time(SDL_GetTicks()), frame_count(0), fps(0.0f) {}

    float getFPS() 
    {
        frame_count++;
        Uint32 current_time = SDL_GetTicks();

        if (current_time - start_time >= 500) {  // Update every 500 ms
            fps = (frame_count * 1000.0f) / (current_time - start_time);
            frame_count = 0;
            start_time = current_time;
        }
        return fps;
    }

private:
    Uint32 start_time;
    int frame_count;
    float fps;
};

class Game
{
public:
    Game(const char* title, int width, int height);
    ~Game();
    void run();

    void init();
    void update(); //Called repeatedly

    Renderer* getRenderer() {return renderer.get();}
    AssetManager* getAssetManager() {return assetManager.get();}

    void limitFPS(Uint32 fpsLimit)
    {
        static Uint32 frameStart;
        static Uint32 frameTime;

        frameStart = SDL_GetTicks();

        // Calculate frame time
        frameTime = SDL_GetTicks() - frameStart;

        // If frame time is less than the desired frame time, delay the difference
        if (frameTime < (1000 / fpsLimit))
        {
            SDL_Delay ((1000 / fpsLimit) - frameTime);
        }
    }
    
    const char* title; //Window title

private:
    bool start();
    void clearScreen(Uint8 r, Uint8 g, Uint8 b, Uint8 a);
    void presentScreen();
    void cleanup();

    SDL_Window* window;
    std::unique_ptr<Renderer> renderer;
    int screenWidth;
    int screenHeight;
    bool isRunning = false;
    std::unique_ptr<AssetManager> assetManager;
    std::vector<std::unique_ptr<Scene>> scenes;

    std::unique_ptr<TextWriter> textWriter;

    FPSCounter fpsCounter;
    bool isInitialized = false;

    Uint32 fpsLimit = 60;
};

#endif
