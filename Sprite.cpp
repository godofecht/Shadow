#include "Sprite.h"
#include <SDL_image.h>
#include <iostream>
#include <cassert>

void Sprite::destroy()
{
    //remove object from assetmanager
    std::cout << "Destroying object: " << id << std::endl;
    setActive (false);
}

void Sprite::setImage (const std::string& path)
{
    loadTexture (path);
}

void Sprite::renderAndRunScripts (Renderer* renderer) // This not only renders but also runs the script
{
    SDL_Point* center = nullptr;
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    SDL_Point calculatedCenter = { bounds.width / 2, bounds.height / 2 };
    
    if (center == nullptr) center = &calculatedCenter;

    if (!isActive) return;

    SDL_Rect renderQuad = { static_cast<int>(bounds.x), static_cast<int>(bounds.y), static_cast<int>(bounds.width), static_cast<int>(bounds.height) };
    SDL_RenderCopyEx (renderer->renderer, texture.texture, nullptr, &renderQuad, rotation, center, flip);

    if (texture.texture == nullptr)
    {
        std::cerr << "Texture is null for object: " << id << std::endl;
        return;
    } //Fixes issue with period of non initialization... but is it the right approach?

    assert (renderer != nullptr);
    assert (texture.texture != nullptr);

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

    SDL_FreeSurface (loadedSurface);
    return true;
}
