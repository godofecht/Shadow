#include "Sprite.h"
#include <SDL_image.h>
#include <iostream>

Sprite::Sprite(SDL_Renderer* renderer, const std::string& path, const std::string& _id)
    : renderer(renderer), texture(nullptr), x(0), y(0)
{
    id = _id;
    std::cout << "Creating object: " << id << std::endl;
    loadTexture(path);
}

Sprite::~Sprite()
{
    if (texture != nullptr)
    {
        SDL_DestroyTexture(texture);
    }
}

void Sprite::setBounds(int x, int y, int width, int height)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
}

void Sprite::destroy()
{
    //remove object from assetmanager
    std::cout << "Destroying object: " << id << std::endl;
    setActive (false);
}

void Sprite::setPosition(int x, int y)
{
    this->x = x;
    this->y = y;
}

void Sprite::setSize(int width, int height)
{
    this->width = width;
    this->height = height;
}

void Sprite::getPosition(int& xOut, int& yOut)
{
    xOut = x;
    yOut = y;
}

void Sprite::setImage(const std::string& path)
{
    loadTexture(path);
}

void Sprite::attachScript(std::shared_ptr<Script> script)
{
    scripts.push_back(script);
    script->start();
}

void Sprite::renderAndRunScripts(SDL_Renderer* renderer) // This not only renders but also runs the script
{
    SDL_Point* center = nullptr;
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    SDL_Point calculatedCenter = { width / 2, height / 2 };
    if (center == nullptr) {
        center = &calculatedCenter;
    }
    if (!isActive) return;

    SDL_Rect renderQuad = { x, y, width, height };
    SDL_RenderCopyEx(renderer, texture, nullptr, &renderQuad, rotation, center, flip);

    for (auto& script : scripts)
    {
        script->update();
    }
}

bool Sprite::loadTexture(const std::string& path)
{
    if (texture != nullptr)
    {
        SDL_DestroyTexture(texture);
    }

    SDL_Surface* loadedSurface = IMG_Load(path.c_str());
    if (loadedSurface == nullptr)
    {
        std::cerr << "Unable to load image " << path << "! SDL_image Error: " << IMG_GetError() << std::endl;
        return false;
    }

    texture = SDL_CreateTextureFromSurface(renderer, loadedSurface);
    if (texture == nullptr)
    {
        std::cerr << "Unable to create texture from " << path << "! SDL Error: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(loadedSurface);
        return false;
    }

    width = loadedSurface->w;
    height = loadedSurface->h;

    SDL_FreeSurface(loadedSurface);
    return true;
}
