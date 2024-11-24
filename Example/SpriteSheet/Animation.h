#include <string>
#include <vector>
#include <memory>
#include "Object.h"
#include "Script.h"
#include <iostream>
#include "Geometry.h"
#include "Texture.h"
#include "Physics.h"
#include "Helpers.h"

class Animation
{
public:
    Animation(SDL_Renderer* renderer, Image* image, int frameWidth, int frameHeight, int frameCount)
        : renderer(renderer), frameWidth(frameWidth), frameHeight(frameHeight), frameCount(frameCount)
    {
        if (!image)
        {
            throw std::runtime_error("Image not initialized");
        }

        SDL_Surface* surface = IMG_Load(image->getFilePath().c_str());
        if (!surface)
        {
            throw std::runtime_error("Failed to load image: " + std::string(IMG_GetError()));
        }

        texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture)
        {
            SDL_FreeSurface(surface);
            throw std::runtime_error("Failed to create texture: " + std::string(SDL_GetError()));
        }

        SDL_FreeSurface(surface);

        // Generate frames
        for (int i = 0; i < frameCount; ++i)
        {
            SDL_Rect frame;
            frame.x = i * frameWidth;
            frame.y = 0;
            frame.w = frameWidth;
            frame.h = frameHeight;
            frames.push_back(frame);
        }
    }

    ~Animation()
    {
        if (texture)
        {
            SDL_DestroyTexture(texture);
        }
    }

    SDL_Texture* getTexture() const
    {
        return texture;
    }

    const SDL_Rect* getCurrentFrame() const
    {
        if (frames.empty())
            return nullptr;
        return &frames[currentFrameIndex];
    }

    void nextFrame()
    {
        currentFrameIndex = (currentFrameIndex + 1) % frames.size();
    }

    void setCurrentFrame(int index)
    {
        if (index >= 0 && index < frames.size())
        {
            currentFrameIndex = index;
        }
    }

private:
    SDL_Renderer* renderer;
    SDL_Texture* texture = nullptr;
    int frameWidth, frameHeight, frameCount;
    int currentFrameIndex = 0;
    std::vector<SDL_Rect> frames;
};

class AnimationController
{
public:
    void addAnimation(const std::string& state, Animation* animation)
    {
        if (!animation)
        {
            throw std::runtime_error("Animation not initialized");
        }
        animations[state] = animation;
    }

    void setAnimationState(const std::string& state)
    {
        if (animations.find(state) == animations.end())
        {
            throw std::runtime_error("State not found: " + state);
        }
        currentState = state;
    }

    Animation* getCurrentAnimation() const
    {
        if (currentState.empty() || animations.find(currentState) == animations.end())
        {
            return nullptr;
        }
        return animations.at(currentState);
    }

    const SDL_Rect* getCurrentFrame() const
    {
        Animation* animation = getCurrentAnimation();
        if (animation)
        {
            return animation->getCurrentFrame();
        }
        return nullptr;
    }

    void nextFrame()
    {
        Animation* animation = getCurrentAnimation();
        if (animation)
        {
            animation->nextFrame();
        }
    }

    SDL_Texture* getCurrentTexture() const
    {
        Animation* animation = getCurrentAnimation();
        if (animation)
        {
            return animation->getTexture();
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, Animation*> animations;
    std::string currentState;
};

