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
#include "SDLManager.h"
#include "Player.h"
#include "Enemy.h"
#include "FPSCounter.h"



class Window
{
public:

    SDL_Window* window;

    float screenWidth;
    float screenHeight;
    const char* title;

    Window ()
    {}

    bool initialize (const char* title, int width, int height)
    {
        window = SDL_CreateWindow (title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
        if (window == nullptr)
        {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        screenWidth = width;
        screenHeight = height;

        std::cout << "Window created." << std::endl;
        return true;
    }

    void exit()
    {
        if (window != nullptr)
        {
            SDL_DestroyWindow (window);
        }
    }
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
    void quit();

    Window window;
    std::unique_ptr<Renderer> renderer;
    int screenWidth;
    int screenHeight;
    bool isRunning = false;
    std::unique_ptr<AssetManager> assetManager;
    std::vector<std::unique_ptr<Scene>> scenes;

    FPSCounter fpsCounter;
    bool isInitialized = false;

    Uint32 fpsLimit = 60;
    SDLManager sdlManager;
};

#endif
