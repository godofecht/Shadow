#pragma once

#include <iostream>
#include <SDL.h>
#include <SDL_image.h>

class SDLManager
{
public:

    static bool lockSurface (SDL_Surface* surface)
    {
        if (SDL_LockSurface (surface) != 0)
        {
            std::cerr << "Failed to lock surface! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        return true;
    }

    static bool initSDLImage()
    {
        //SDL_Image is needed for PNG and JPEG support
        if (!(IMG_Init (IMG_INIT_PNG) & IMG_INIT_PNG))
        {
            std::cerr << "SDL_image could not initialize! SDL_image Error: " << IMG_GetError() << std::endl;
            return false;
        }
        return true;
    }

    static SDL_Renderer* createRenderer (SDL_Window* window)
    {
        auto renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_ACCELERATED);
        if (renderer == nullptr)
        {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return nullptr;
        }
        return renderer;
    }

    static bool initVideo()
    {
        if (SDL_Init (SDL_INIT_VIDEO) < 0)
        {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }

        std::cout << "SDL initialized." << std::endl;
        return true;
    }

    static void handleExit()
    {
        IMG_Quit();
        SDL_Quit();
    }
};