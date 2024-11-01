#include <SDL.h>
#include <SDL_image.h>
#include <iostream>

class Renderer
{
public:
    Renderer(){}

    bool initialize (SDL_Window* window)
    {
        renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_ACCELERATED);

        if (renderer == nullptr)
        {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }

        //SDL_Image is needed for PNG and JPEG support
        if (!(IMG_Init (IMG_INIT_PNG) & IMG_INIT_PNG))
        {
            std::cerr << "SDL_image could not initialize! SDL_image Error: " << IMG_GetError() << std::endl;
            return false;
        }

        return true;
    }

    void drawLine (int x1, int y1, int x2, int y2)
    {
        SDL_SetRenderDrawColor (renderer, 255, 0, 0, 255);
        SDL_RenderDrawLine (renderer, x1, y1, x2, y2);
    }

    void clearScreen (Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        SDL_RenderClear(renderer);        
    }

    void destroy() { SDL_DestroyRenderer (renderer); }
    void present() { SDL_RenderPresent (renderer); }

    SDL_Renderer* renderer;
};