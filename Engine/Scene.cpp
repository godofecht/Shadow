#include "AssetManager.h"
#include "Scene.h"
#include "Sprite.h"  // Include any specific sprite types you'll use


void Scene::initializePendingSprites()
{
    for (auto it = pendingSprites.begin(); it != pendingSprites.end(); )
    {
        if ((*it)->isInitialized) // Only move fully initialized sprites
        {
            sprites.push_back (*it);    // Move to main list for rendering
            it = pendingSprites.erase (it); // Erase from pending list
        }
        else
        {
            ++it;
        }
    }
}

void Scene::render (Renderer* renderer)
{
    initializePendingSprites(); // Ensure that all sprites have been added, prior to running 'renderAndRunScripts.'

    for (auto& sprite : sprites)
    {
        if (!sprite->isActive || !sprite->isInitialized)
            continue;

        try 
        {
            sprite->renderAndRunScripts();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception caught while rendering sprite: " << e.what() << std::endl;
        }
    }
}