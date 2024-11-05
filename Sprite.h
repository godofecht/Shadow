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

class Scene;

class Part : public Object
{
public:

    Part (Renderer* renderer) : Object (renderer)
    {
        
    }

    void renderAndRunScripts (Renderer* renderer) override
    {

    };

};

class SimpleSprite : public Object
{
public:
    SimpleSprite (Renderer* renderer, const std::string& path, const std::string& _id)
        : Object (renderer)
    {
        setId (_id);
        std::cout << "Creating object: " << _id << std::endl;

        loadBackgroundTexture (path);
        isInitialized = true;
    }

    ~SimpleSprite()
    {
        getBackgroundTexture().destroy();
    }

    void setImage (const std::string& path);
    void attachScript (std::shared_ptr<Script> script) { scripts.push_back(script); }
    std::vector<std::shared_ptr<Script>>& getScripts() { return scripts; }

    virtual void renderAndRunScripts (Renderer* renderer) override;
    void setActive (bool state) { isActive = state; }
    void destroy();



    Scene* getScene() const { return scene; }
    void setScene (Scene* _scene) { scene = _scene; }


    virtual void update (float deltaTime) {}

    void addPart (const std::string& path, const std::string& id)
    {
        std::shared_ptr<Part> part = std::make_shared<Part>(getRenderer());
        part->loadBackgroundTexture (path);
        part->setId (id);
        // part->setScene (scene);
        parts.push_back (part);
    }

    Part* getPart (const std::string& id)
    {
        for (auto& part : parts)
        {
            if (part->getId() == id)
            {
                return part.get();
            }
        }
        return nullptr;
    }

private:

    std::vector<std::shared_ptr<Part>> parts;
    Scene* scene; // Pointer to the parent Scene

    std::vector<std::shared_ptr<Script>> scripts;
};

// Utility function to calculate normalized direction from origin to target
inline Vector2D calculateDirection (const Position<float>& origin, const Position<float>& target)
{
    float dx = target.x - origin.x;
    float dy = target.y - origin.y;
    float magnitude = std::sqrt (dx * dx + dy * dy);
    return magnitude > 0 ? Vector2D (dx / magnitude, dy / magnitude) : Vector2D(0, 0);
}

inline float calculateAngle (const Vector2D& delta)
{
    return atan2 (delta.y, delta.x) * 180 / M_PI;
}

inline Vector2D getMousePosition()
{
    int x, y;
    SDL_GetMouseState (&x, &y);
    return Vector2D (static_cast<float>(x), static_cast<float>(y));
}

inline Vector2D getSpriteCenter (SimpleSprite* sprite)
{
    float spriteX, spriteY;
    sprite->getPosition (spriteX, spriteY);
    return Vector2D (spriteX + sprite->getWidth() / 2, spriteY + sprite->getHeight() / 2);
}

#endif
