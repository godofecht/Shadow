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
    loadBackgroundTexture (path);
    isInitialized = true;
}

void SimpleSprite::renderAndRunScripts (Renderer* renderer) // This not only renders but also runs the script
{
    if (!isActive) return;

    renderer->copyTexture (getBackgroundTexture().texture, getBounds(), 0);

    for (auto& part : parts) // Render parts
    {
        auto& parentBounds = getBounds();
        auto& partBounds = part->getBounds();
        partBounds.x += parentBounds.x;
        partBounds.y += parentBounds.y;
        renderer->copyTexture (part->getBackgroundTexture().texture, partBounds, part->getAngle() + getAngle());
    }
    
    // if (texture.texture == nullptr)
    // {
    //     std::cerr << "Texture is null for object: " << getId() << std::endl;
    //     return;
    // } //Fixes issue with period of non initialization... but is it the right approach?

    for (auto& script : scripts)
    {
        assert (script != nullptr);
        script->update();
    }
}

