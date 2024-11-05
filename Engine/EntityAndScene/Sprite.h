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
class SimpleSprite : public Object
{
public:

    SimpleSprite (Renderer* renderer, const std::string& path, const std::string& _id)
        : renderer (renderer), bounds (0, 0, 0, 0) // Initialize bounds at (0,0) with zero width and height
    {
        setId (_id);
        std::cout << "Creating object: " << _id << std::endl;

        loadTexture (path);
        isInitialized = true;
    }

    ~SimpleSprite()
    {
        backgroundTexture.destroy();
    }

    void setSize (float width, float height)
    {
        bounds.width = width;
        bounds.height = height;
    }

    void setBounds (float x, float y, float width, float height)
    {
        bounds = Rect<float>(x, y, width, height);
    }

    Rect<float> getBounds() const { return bounds; }

    void setPosition (float x, float y)
    {
        bounds.x = x;
        bounds.y = y;
    }

    void getPosition (float& xOut, float& yOut) const
    {
        xOut = bounds.x;
        yOut = bounds.y;
    }

    void setRotation (float angle) { rotation = angle; }
    float getRotation() const { return rotation; }
    void setImage (const std::string& path);
    void attachScript (std::shared_ptr<Script> script) { scripts.push_back(script); }
    std::vector<std::shared_ptr<Script>>& getScripts() { return scripts; }

    float getX() const { return bounds.x; }
    float getY() const { return bounds.y; }
    void setX (float x) { bounds.x = x; }
    void setY (float y) { bounds.y = y; }

    virtual void renderAndRunScripts (Renderer* renderer) override;
    void setActive (bool state) { isActive = state; }
    void destroy();
    Renderer* getRenderer() { return renderer; }

    float getWidth() const { return bounds.width; }
    float getHeight() const { return bounds.height; }
    Scene* getScene() const { return scene; }
    void setScene (Scene* _scene) { scene = _scene; }

    void addPart (const std::string& path, const std::string& id)
    {
        auto part = std::make_shared<SimpleSprite>(renderer, path, id);
        part->setScene (scene);
        parts.push_back (*part);
    }

    Part* getPart (const std::string& id)
    {
        for (auto& part : parts)
        {
            if (part.getId() == id)
            {
                return &part;
            }
        }
        return nullptr;
    }

    virtual void update (float deltaTime) {}

    bool isInitialized = false;
    bool isActive = true;

private:
    Renderer* renderer;
    Texture backgroundTexture;
    Scene* scene; // Pointer to the parent Scene


    float rotation = 0.0f;
    Rect<float> bounds; // Holds position (x, y), width, and height
    std::vector<std::shared_ptr<Script>> scripts;

    bool loadTexture (const std::string& path);
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
