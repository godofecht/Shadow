// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

class SDLManager
{
public:

    static bool lockSurface (SDL_Surface* surface)
    {
        if (SDL_LockSurface (surface) != 0)
        {
            std::cerr << "Failed to lock surface! SDL_Error: " << SDL_GetError() << '\n';
            return false;
        }
        return true;
    }

    static bool initSDLImage()
    {
        //SDL_Image is needed for PNG and JPEG support
        if (!(IMG_Init (IMG_INIT_PNG) & IMG_INIT_PNG))
        {
#ifndef __EMSCRIPTEN__
            std::cerr << "SDL_image PNG support unavailable: " << IMG_GetError() << '\n';
#endif
            return false;
        }
        return true;
    }

    static SDL_Renderer* createRenderer (SDL_Window* window)
    {
        auto renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_ACCELERATED);
        if (renderer == nullptr)
        {
            // No accelerated driver available (headless SDL_VIDEODRIVER=dummy
            // runs in CI, VMs without GPU support): fall back to the software
            // renderer so the app still runs. No behavior change on normal
            // desktops where the accelerated driver succeeds.
            renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_SOFTWARE);
        }
        if (renderer == nullptr)
        {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << '\n';
            return nullptr;
        }
        return renderer;
    }

    static bool initVideo()
    {
        if (SDL_Init (SDL_INIT_VIDEO) < 0)
        {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << '\n';
            return false;
        }

        std::cout << "SDL initialized." << '\n';
        return true;
    }

    static void handleExit()
    {
        IMG_Quit();
        SDL_Quit();
    }
};
