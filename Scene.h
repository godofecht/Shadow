#pragma once
#include <vector>
#include <memory>
#include <cassert>
#include "AssetManager.h"
#include "Object.h"
#include "Sprite.h"

class SimpleSprite;
class AssetManager;

class Scene 
{
private:
    AssetManager* assetManager;
public:
    bool isInitialized = false;
    std::vector<std::shared_ptr<SimpleSprite>> sprites;
    std::vector<std::shared_ptr<SimpleSprite>> pendingSprites;

    Scene() { initialize(); }

    void setAssetManager (AssetManager* assetManager) { this->assetManager = assetManager; }
    AssetManager* getAssetManager() { return assetManager; }
    int getAssetCount() { return (int) sprites.size(); }

    void initialize() 
    {
        isInitialized = true;
        sprites.clear();
    }

    std::shared_ptr<SimpleSprite> getSpriteById (const std::string& id) 
    {
        for (auto& sprite : sprites) 
        {
            if (sprite && sprite->getId() == id) 
            {
                return sprite;
            }
        }
        for (auto& sprite : pendingSprites) 
        {
            if (sprite && sprite->getId() == id) 
            {
                return sprite;
            }
        }
        return nullptr;
    }

    void addItem (std::shared_ptr<SimpleSprite> sprite);
    void render (Renderer* renderer);
    void initializePendingSprites();
};