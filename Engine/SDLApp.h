#ifndef SDLAPP_H
#define SDLAPP_H

#include <SDL.h>
#include <SDL_image.h>
#include <memory>
#include <string>
#include "AssetManager.h"
#include "TextWriter.h"
#include "SDLManager.h"
#include "FPSCounter.h"
#include "AudioEngine.h"
#include "Physics.h"
#include "TileMap.h"
#include "Sprite.h"

class Window
{
public:
    SDL_Window* window;

    float screenWidth;
    float screenHeight;
    const char* title;

    Window(){}

    bool initialize (const char* title, int width, int height)
    {
        window = SDL_CreateWindow (title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
        if (window == nullptr)
        {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        screenWidth = (float) width;
        screenHeight = (float) height;

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

    Renderer* getRenderer()             {return renderer.get();}
    AssetManager* getAssetManager() {return assetManager.get();}
    AudioEngine* getAudioEngine()    {return audioEngine.get();}

    void addScene (std::unique_ptr<Scene>& scene)
    {
        scenes.push_back (std::move (scene));
    }
    
    void limitFPS (Uint32 _fpsLimit)
    {
        fpsLimit = _fpsLimit;
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

    virtual void initializeComponents() {}

    void drawFPS()
    {
        float fps = fpsCounter.getFPS();
        std::wstring fpsText = L"FPS: " + std::to_wstring(fps);

        int textHeight = 300;  // Adjust this value for the text box height

        // Get the width and height of the text box
        int textWidth = textHeight * 8.0f / 6.0f;  // Adjust this value as needed for the text box width

        // Calculate the position for the top-right corner
        float xPosition = screenWidth - textWidth; // 10-pixel margin from the right edge
        float yPosition = textHeight; // 10-pixel margin from the top edge

        // Set the bounds for the text
        Rect<float> bounds(0, 0, textWidth, textHeight);

        // Draw the text
        // renderer->getTextWriter()->drawTextToRenderer (fpsText, renderer->renderer, bounds, "default.ttf");
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
    SDLManager sdlManager;
};

#endif
