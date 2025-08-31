#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include "TextWriter.h"
#include "Geometry.h"
#include <cassert>
#include "SDLManager.h"
#include "Texture.h"

class TextWriter;
class Renderer
{
    std::unique_ptr<TextWriter> textWriter;

public:
    Renderer()
    {
    }

    bool initialize (SDL_Window* window)
    {
        renderer = SDLManager::createRenderer (window);
        textWriter = std::make_unique<TextWriter>(renderer);

        if (renderer == nullptr)
        {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }


        SDLManager::initSDLImage();

        return true;
    }

    void copyTexture (SDL_Texture* texture, Rect<float>& bounds, float rotation)
    {
        assert (renderer != nullptr);
        assert (texture != nullptr);

        SDL_Rect renderQuad = bounds.getSDLRect();
        SDL_Point* center = nullptr;

        SDL_RendererFlip flip = SDL_FLIP_NONE;
        int boundsWidthBy2 = bounds.width / 2;
        int boundsHeightBy2 = bounds.height / 2;
        SDL_Point calculatedCenter = { boundsWidthBy2, boundsHeightBy2 };
        if (center == nullptr) center = &calculatedCenter;
        SDL_RenderCopyEx (renderer, texture, nullptr, &renderQuad, rotation, center, flip);
    }

    TextWriter* getTextWriter() { return textWriter.get(); }

    void drawLine (int x1, int y1, int x2, int y2)
    {
        SDL_SetRenderDrawColor (renderer, 255, 0, 0, 255);
        SDL_RenderDrawLine (renderer, x1, y1, x2, y2);
    }

    void clearScreen (Uint8 r, Uint8 g, Uint8 b, Uint8 a)
    {
        SDL_SetRenderDrawColor (renderer, r, g, b, a);
        SDL_RenderClear (renderer);        
    }

    void destroy() { SDL_DestroyRenderer (renderer); }
    void present() { SDL_RenderPresent (renderer); }
    void reset()   { textWriter.reset(); }

    SDL_Renderer* renderer;
};

inline SDL_Surface* loadSurfaceFromRenderer (const std::string& path)
{
    return IMG_Load (path.c_str());
}
