#ifndef SDLAPP_H
#define SDLAPP_H

#include <SDL.h>
#include <SDL_image.h>
#include <memory>
#include <string>
#include "AssetManager.h"
#include "CharacterControllerScript.h"
#include "FlyControllerScript.h"

class SDLApp
{
public:
    SDLApp(const char* title, int width, int height);
    ~SDLApp();
    void run();

    virtual void init() {} //Called once at the beginning of the Run Cycle
    virtual void update() {} //Called repeatedly

    SDL_Renderer* getRenderer() {return renderer;}
    AssetManager* getAssetManager() {return assetManager.get();}

private:
    bool start();
    void clearScreen(Uint8 r, Uint8 g, Uint8 b, Uint8 a);
    void presentScreen();
    void cleanup();

    SDL_Window* window;
    SDL_Renderer* renderer;
    int screenWidth;
    int screenHeight;
    bool isRunning;
    std::unique_ptr<AssetManager> assetManager;
};

#endif
