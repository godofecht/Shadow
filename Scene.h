#pragma once
#include <vector>
#include <memory>
#include <cassert>
#include "AssetManager.h"
#include "Asset.h"
class Sprite;
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
    int getAssetCount() { return sprites.size(); }

    void initialize() 
    {
        isInitialized = true;
        sprites.clear();
    }

    void addItem (std::shared_ptr<Sprite> sprite);
    void render (Renderer* renderer);
    void initializePendingSprites();
};
