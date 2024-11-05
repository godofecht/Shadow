#include "Sprite.h"
#include <SDL_image.h>
#include <iostream>
#include <cassert>

void SimpleSprite::destroy()
{
    //remove object from assetmanager
    std::cout << "Destroying object: " << getId() << std::endl;
    setActive (false);
}

void SimpleSprite::setImage (const std::string& path)
{
    loadTexture (path);
}

void SimpleSprite::renderAndRunScripts (Renderer* renderer) // This not only renders but also runs the script
{
    if (!isActive) return;
    renderer->copyTexture (backgroundTexture.texture, bounds, rotation);

    if (backgroundTexture.texture == nullptr)
    {
        std::cerr << "Texture is null for object: " << getId() << std::endl;
        return;
    } //Fixes issue with period of non initialization... but is it the right approach?

    for (auto& script : scripts)
    {
        assert (script != nullptr);
        script->update();
    }
}

inline SDL_Surface* loadSurfaceFromRenderer (const std::string& path)
{
    return IMG_Load (path.c_str());
}

bool SimpleSprite::loadTexture (const std::string& path)
{
    backgroundTexture.destroy();

    auto loadedSurface = loadSurfaceFromRenderer (path);
    if (loadedSurface == nullptr)
    {
        std::cerr << "Unable to load image " << path << "! SDL_image Error: " << IMG_GetError() << std::endl;
        return false;
    }

    // texture = SDL_CreateTextureFromSurface (renderer, loadedSurface);
    // if (texture == nullptr)
    // {
    //     std::cerr << "Unable to create texture from " << path << "! SDL Error: " << SDL_GetError() << std::endl;
    //     SDL_FreeSurface (loadedSurface);
    //     return false;
    // }

    backgroundTexture.createFromSurface (renderer->renderer, loadedSurface, path);

    setSize (loadedSurface->w, loadedSurface->h);
    return true;
}
