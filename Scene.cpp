#include "Scene.h"

void Scene::addItem (std::shared_ptr<SimpleSprite> sprite) 
{
    sprite->setScene (this);   // Associate the sprite with the current scene
    pendingSprites.push_back (sprite); // Add to pending, not main list
}

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
    initializePendingSprites(); // Ensure pending sprites are initialized

    for (auto& sprite : sprites)
    {
        if (!sprite->isActive || !sprite->isInitialized)
            continue;

        try 
        {
            sprite->renderAndRunScripts (renderer);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception caught while rendering sprite: " << e.what() << std::endl;
        }
    }
}