#include "Sprite.h"
#include <SDL_image.h>
#include <iostream>
#include <cassert>

void Sprite::destroy()
{
    //remove object from assetmanager
    std::cout << "Destroying object: " << getId() << std::endl;
    setActive (false);
}

void Sprite::setImage (const std::string& path)
{
    loadTexture (path);
}

void Sprite::renderAndRunScripts (Renderer* renderer) // This not only renders but also runs the script
{
    if (!isActive) return;
    renderer->copyTexture (texture.texture, bounds, rotation);

    if (texture.texture == nullptr)
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

bool Sprite::loadTexture (const std::string& path)
{
    texture.destroy();

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

    texture.createFromSurface (renderer->renderer, loadedSurface, path);

    setSize (loadedSurface->w, loadedSurface->h);
    return true;
}
