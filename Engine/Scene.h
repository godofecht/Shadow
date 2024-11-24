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
    std::vector<std::shared_ptr<Sprite>> sprites;
    std::vector<std::shared_ptr<Sprite>> pendingSprites;

    Scene() { initialize(); }

    void setAssetManager (AssetManager* assetManager) { this->assetManager = assetManager; }
    AssetManager* getAssetManager() { return assetManager; }
    int getAssetCount() { return (int) sprites.size(); }

    void initialize() 
    {
        isInitialized = true;
        sprites.clear();
    }

    std::shared_ptr<Sprite> getSpriteById (const std::string& id) 
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

    template <typename T>
    void addItem (const std::string id) 
    {
        auto sprite = assetManager->createAsset<T>(id);
        sprite->setScene (this);   // Associate the sprite with the current scene
        pendingSprites.push_back (sprite); // Add to pending, not main list
    }

    void addItem (std::shared_ptr<Sprite> sprite);
    void render (Renderer* renderer);
    void initializePendingSprites();
};