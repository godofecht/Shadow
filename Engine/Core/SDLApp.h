// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#ifndef SDLAPP_H
#define SDLAPP_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <memory>
#include <string>
#include "Engine/Core/AssetManager.h"
#include "Engine/Text/TextWriter.h"
#include "Engine/Core/SDLManager.h"
#include "Engine/Core/FPSCounter.h"
#include "Engine/Audio/AudioEngine.h"
#include "Engine/Core/Physics.h"
#include "Engine/EntityAndScene/Scene.h"
#include "Engine/EntityAndScene/TileMap.h"
class Window
{
public:
    SDL_Window* window;

    float screenWidth;
    float screenHeight;
    const char* title;

    Window(){}

    bool initialize (const char* _title, int width, int height)
    {
        window = SDL_CreateWindow (_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
        if (window == nullptr)
        {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << '\n';
            return false;
        }
        screenWidth = (float) width;
        screenHeight = (float) height;

        std::cout << "Window created." << '\n';
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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

class Game
{
public:
    Game(const char* title, int width, int height);
    virtual ~Game();
    void run();

    // Loop callback for Emscripten
    static void loop(void* arg);
    void mainLoop();

    void init();
    virtual void update(); //Called repeatedly
    virtual void renderPostFX() {} // Called after update for full-screen effects

    Renderer* getRenderer()             {return renderer.get();}
    AssetManager* getAssetManager() {return assetManager.get();}
    AudioEngine* getAudioEngine()    {return audioEngine.get();}

    void addScene (std::unique_ptr<Scene>&& scene)
    {
        scenes.push_back (std::move (scene));
    }
    
    void limitFPS (Uint32 _fpsLimit)
    {
#ifdef __EMSCRIPTEN__
        (void)_fpsLimit;
        return;
#else
        static Uint32 frameStart;
        static Uint32 frameTime;

        frameStart = SDL_GetTicks();

        // Calculate frame time
        frameTime = SDL_GetTicks() - frameStart;

        // If frame time is less than the desired frame time, delay the difference
        if (frameTime < (1000 / _fpsLimit))
        {
            SDL_Delay ((1000 / _fpsLimit) - frameTime);
        }
#endif
    }

    virtual void initializeComponents() {}

    void drawFPS()
    {
        float fps = fpsCounter.getFPS();
        (void)fps;
    }
    
    const char* title; //Window title

    PhysicsManager* getPhysicsManager() { return physicsManager.get(); }

    virtual void onStart() {}

private:
    bool start();
    void clearScreen (Uint8 r, Uint8 g, Uint8 b, Uint8 a);
    void presentScreen();
    void quit();

    Window window;
    std::unique_ptr<AudioEngine> audioEngine;
    std::unique_ptr<Renderer> renderer;
    int screenWidth;
    int screenHeight;
    bool isRunning = false;
    std::unique_ptr<AssetManager> assetManager;
    std::unique_ptr<PhysicsManager> physicsManager;
    std::vector<std::unique_ptr<Scene>> scenes;

    FPSCounter fpsCounter;
    bool isInitialized = false;

    Uint32 fpsLimit = 60;
    [[maybe_unused]] SDLManager sdlManager;
};

#endif
