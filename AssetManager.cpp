#include "AssetManager.h"

AssetManager::AssetManager(SDL_Renderer* renderer)
{
    this->renderer = renderer;
    std::cout << "Created Asset Manager." << std::endl;
}