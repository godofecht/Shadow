#ifndef SPRITE_H
#define SPRITE_H

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
#include "Animation.h"

class Scene;

class Part : public Object
{
public:

    Part (Renderer* renderer) : Object (renderer)
    {
        
    }

    void renderAndRunScripts () override
    {

    };
};

class Component
{

public:
    Component()
    {

    }
};
class PhysicsComponent : public Component
{
    PhysicsManager* physicsManager;
    std::unique_ptr<Body> body;
    public:

    PhysicsComponent (PhysicsManager* _physicsManager) : physicsManager (_physicsManager)
    {
        body = std::make_unique<RigidBody>(physicsManager->getWorld());
    }

    Body* getBody()
    {
        return body.get();
    }
};



class AnimationFrameSet
{
public:
    std::vector<SDL_Rect> frames;

    void loadFramesFromImage (const Image& image, int frameWidth, int frameHeight, int frameCount)
    {
        int x = 0;
        for (int i = 0; i < frameCount; ++i)
        {
            SDL_Rect frame;
            frame.x = x;
            frame.y = 0;
            frame.w = frameWidth;
            frame.h = frameHeight;
            frames.push_back (frame);
            x += frameWidth;
        }
    }
};

class SpriteSheet
{
public:
    SpriteSheet (const Image& image, int frameWidth, int frameHeight, int frameCount)
        : image (image)
    {
        loadAnimations (frameWidth, frameHeight, frameCount);
    }

    void loadAnimations (int frameWidth, int frameHeight, int frameCount)
    {
        AnimationFrameSet frameSet;
        frameSet.loadFramesFromImage (image, frameWidth, frameHeight, frameCount);
        animations.push_back (frameSet);
    }

    const AnimationFrameSet& getAnimation (int index) const
    {
        if (index >= animations.size())
        {
            throw std::out_of_range ("Invalid animation index");
        }
        return animations[index];
    }

private:
    Image image;
    std::vector<AnimationFrameSet> animations;
};

class Sprite : public Object
{
public:
    Sprite (Renderer* renderer, const std::string& _id)
        : Object (renderer), renderer (renderer), texture (nullptr), isActive (false), currentFrameIndex (0)
    {
        setId (_id);
        std::cout << "Creating sprite object: " << _id << std::endl;
    }

    virtual ~Sprite()
    {
        if (texture)
        {
            SDL_DestroyTexture (texture);
        }
    }

    virtual void renderAndRunScripts()
    {
        if (!isActive || !texture)
            return;

        const SDL_Rect* frameToDraw = getCurrentImageToDraw();
        if (frameToDraw)
        {
            Rect<int> rect (getPosition().x, getPosition().y, frameToDraw->w, frameToDraw->h); // Example position
            SDL_Rect destRect = rect.getSDLRect();
            // Determine flip mode based on isReverse
            SDL_RendererFlip flipMode = isReverse ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

            SDL_RenderCopyEx (
                renderer->renderer, // The SDL renderer
                texture,            // The texture to render
                frameToDraw,        // The source rectangle (clipping region)
                &destRect,          // The destination rectangle
                0.0,                // Rotation angle (degrees)
                nullptr,            // Center point of rotation (nullptr means the center of destRect)
                flipMode            // Flip mode: horizontal flip if isReverse is true
            );
        }

        for (const auto& script : scripts)
        {
            script->update();
        }
    }


    bool isReverse = false;

    void setReverse (bool _isReverse) { isReverse = _isReverse; }
    void setActive (bool state) { isActive = state; }

    void attachScript (std::shared_ptr<Script> script) { scripts.push_back(script); }
    std::vector<std::shared_ptr<Script>>& getScripts() { return scripts; }

    virtual const SDL_Rect* getCurrentImageToDraw() const { return nullptr; }

    virtual void setScene (Scene* _scene) { scene = _scene; }

    friend class Scene;

protected:
    Renderer* renderer;
    SDL_Texture* texture;
    std::vector<std::shared_ptr<Script>> scripts;
    bool isActive;
    size_t currentFrameIndex;

    Scene* scene;
};

// class AnimatedSprite : public Sprite
// {
// public:
//     AnimatedSprite (Renderer* renderer, const std::string& _id)
//         : Sprite (renderer, _id)
//     {
//     }

//     void loadSpriteSheet (Image* image, int frameWidth, int frameHeight, int frameCount)
//     {
//         if (!image)
//         {
//             throw std::runtime_error ("Image not initialized");
//         }

//         // Load the sprite sheet into an SDL_Texture
//         SDL_Surface* surface = IMG_Load (image->getFilePath().c_str());
//         if (!surface)
//         {
//             throw std::runtime_error ("Failed to load image: " + std::string(IMG_GetError()));
//         }

//         texture = SDL_CreateTextureFromSurface (renderer->renderer, surface);
//         if (!texture)
//         {
//             SDL_FreeSurface (surface);
//             throw std::runtime_error ("Failed to create texture: " + std::string(SDL_GetError()));
//         }

//         SDL_FreeSurface (surface);

//         // Create frames
//         int x = 0;
//         for (int i = 0; i < frameCount; ++i)
//         {
//             SDL_Rect frame;
//             frame.x = x;
//             frame.y = 0;
//             frame.w = frameWidth;
//             frame.h = frameHeight;
//             spriteFrames.push_back (frame);
//             x += frameWidth;
//         }

//         isInitialized = true;
//         isActive = true;
//     }

//     const SDL_Rect* getCurrentImageToDraw() const override
//     {
//         if (spriteFrames.empty())
//             return nullptr;
//         return &spriteFrames[currentFrameIndex];
//     }

//     void nextFrame()
//     {
//         if (!spriteFrames.empty())
//         {
//             currentFrameIndex = (currentFrameIndex + 1) % spriteFrames.size();
//         }
//     }


//     void renderAndRunScripts() override
//     {
//         Sprite::renderAndRunScripts();
//         nextFrame();
//     }

//     void setCurrentFrameIndex (int index)
//     {
//         if (index >= 0 && index < spriteFrames.size())
//         {
//             currentFrameIndex = index;
//         }
//     }

// private:
//     std::vector<SDL_Rect> spriteFrames;
// };

class AnimatedSprite : public Sprite
{
public:
    AnimatedSprite(Renderer* renderer, const std::string& _id)
        : Sprite(renderer, _id)
    {
        isInitialized = true;
        isActive = true;
    }

    void addAnimation (const std::string& state, const std::string imageFilePath, int animationWidth, int animationHeight, int numFrames) 
    { 
        Animation* animation = new Animation (renderer->renderer, new Image (imageFilePath), animationWidth, animationHeight, numFrames);
        animationController.addAnimation (state, animation); 
    }

    void renderAndRunScripts() override
    {
        const SDL_Rect* currentFrame = animationController.getCurrentFrame();
        if (currentFrame)
        {
            SDL_Rect destRect =  Rect<float> (getPosition().x, getPosition().y, getBounds().width, getBounds().height ).getSDLRect();

            SDL_Texture* texture = animationController.getCurrentTexture();

            // Determine flip mode based on isReverse
            SDL_RendererFlip flipMode = isReverse ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

            SDL_RenderCopyEx(
                renderer->renderer, // The SDL renderer
                texture,            // The texture to render
                currentFrame,       // The source rectangle (clipping region)
                &destRect,          // The destination rectangle
                0.0,                // Rotation angle (degrees)
                nullptr,            // Center point of rotation (nullptr means the center of destRect)
                flipMode            // Flip mode: horizontal flip if isReverse is true
            );
        }

        nextFrame();

        for (const auto& script : scripts)
        {
            script->update();
        }
    }

    void nextFrame() { animationController.nextFrame(); }
    void setAnimationState (const std::string& state) { animationController.setState (state); }


private:
    AnimationController animationController;
};


class SimpleSprite : public Sprite
{
public:
    SimpleSprite (Renderer* renderer, const std::string& path, const std::string& _id)
        : Sprite (renderer, _id)
    {
        loadBackgroundTexture (path);
        isInitialized = true;
    }

    SimpleSprite (Renderer* renderer, const std::string& _id)
        : Sprite (renderer, _id)
    {
        isInitialized = false;
    }

    ~SimpleSprite() { destroy(); }

    template <typename T>
    void addComponent(PhysicsManager* physicsManager)
    {
        components.push_back (new PhysicsComponent (physicsManager));
        Body* body = static_cast<PhysicsComponent*>(components.back())->getBody();
        physicsManager->getWorld().addBody (body);
    }

    void setImage (const std::string& path)
    {
        loadBackgroundTexture (path);
    }

    const SDL_Rect* getCurrentImageToDraw() const override
    {
        static SDL_Rect fullImage = { 0, 0, 0, 0 }; // Placeholder dimensions
        SDL_QueryTexture (texture, nullptr, nullptr, &fullImage.w, &fullImage.h);
        return &fullImage;
    }

    void renderAndRunScripts() override;

private:
    void loadBackgroundTexture (const std::string& path)
    {
        SDL_Surface* surface = IMG_Load (path.c_str());
        if (!surface)
        {
            throw std::runtime_error ("Failed to load image: " + std::string(IMG_GetError()));
        }

        texture = SDL_CreateTextureFromSurface(renderer->renderer, surface);
        SDL_FreeSurface(surface);

        if (!texture)
        {
            throw std::runtime_error ("Failed to create texture: " + std::string(SDL_GetError()));
        }
    }

    void destroy()
    {
        if (texture)
        {
            SDL_DestroyTexture (texture);
            texture = nullptr;
        }
    }

    std::vector<Component*> components;
    std::vector<Part> parts;
};

// Utility function to calculate normalized direction from origin to target
inline Vector2D calculateDirection (const Point2D& origin, const Point2D& target)
{
    float dx = target.x - origin.x;
    float dy = target.y - origin.y;
    float magnitude = std::sqrt (dx * dx + dy * dy);
    return magnitude > 0 ? Vector2D (dx / magnitude, dy / magnitude) : Vector2D(0, 0);
}

inline float calculateAngle (const Point2D& delta) { return atan2 (delta.y, delta.x) * 180.0 / M_PI; } //TODO: change atan2 to Point2D::calculateAngleFromOrigin()

inline Point2D getMousePosition()
{
    int x, y;
    SDL_GetMouseState (&x, &y);
    return Point2D (static_cast<float>(x), static_cast<float>(y));
}

inline Point2D getSpriteCenter (Sprite* sprite)
{
    Point2D position = sprite->getPosition();
    return Point2D (position.x + sprite->getWidth() / 2, position.y + sprite->getHeight() / 2);
}

#endif
