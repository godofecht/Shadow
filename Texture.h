#pragma once

#include <SDL.h>
#include <memory>
#include <string>
#include <vector>

class Texture 
{
    public:
    SDL_Texture* texture;

    Texture() : texture (nullptr) {}

    void destroy()
    {
        if (texture != nullptr)
        {
            SDL_DestroyTexture (texture);
        }
    }

    bool createFromSurface (SDL_Renderer* renderer, SDL_Surface* loadedSurface, const std::string& path)
    {
        texture = SDL_CreateTextureFromSurface (renderer, loadedSurface);
        if (texture == nullptr)
        {
            std::cerr << "Unable to create texture from " << path << "! SDL Error: " << SDL_GetError() << std::endl;
            SDL_FreeSurface (loadedSurface);
            return false;
        }

        SDL_FreeSurface (loadedSurface);
        return true;
    }
};
