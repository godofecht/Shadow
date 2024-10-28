#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <SDL.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "Sprite.h"

class AssetManager
{
public:
    AssetManager(SDL_Renderer* renderer);

    template <typename T>
    std::shared_ptr<T> createAsset(const std::string& assetName, const std::string& assetPath = "")
    {
        std::shared_ptr<T> asset;
        if (assetPath == "")
            asset = std::make_shared<T>(renderer, "/Users/abhishekshivakumar/gamedev/shadow/" + assetName + ".png", assetName);
        else
            asset = std::make_shared<T>(renderer, assetPath, assetName);
        assets[assetName] = asset;
        return asset;
    }

private:
    SDL_Renderer* renderer;
    std::unordered_map<std::string, std::shared_ptr<Object>> assets;
};

#endif
