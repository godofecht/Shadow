// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#ifndef SPRITE_H
#define SPRITE_H

#include <string>
#include <vector>
#include <memory>
#include "Engine/Core/Object.h"
#include "Engine/Core/Script.h"
#include <iostream>
#include "Engine/Core/Geometry.h"
#include "Engine/ResourceHandling/Texture.h"
#include "Engine/Core/Physics.h"
#include "Engine/Core/Helpers.h"
class Scene;

class Part : public Object
{
public:

    Part (Renderer* renderer) : Object (renderer)
    {
        
    }

    ~Part() override = default;

    void renderAndRunScripts (Renderer* renderer) override
    {
        (void)renderer;
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

    PhysicsComponent(PhysicsManager* _physicsManager) : physicsManager(_physicsManager)
    {
        body = std::make_unique<RigidBody>(physicsManager->getWorld());
    }

    Body* getBody()
    {
        return body.get();
    }
};

class SimpleSprite : public Object
{
public:
    SimpleSprite (Renderer* renderer, const std::string& path, const std::string& _id)
        : Object (renderer)
    {
        setId (_id);
        std::cout << "Creating object: " << _id << '\n';

        loadBackgroundTexture (path);
        isInitialized = true;
    }

    SimpleSprite (Renderer* renderer, const std::string& _id)
        : Object (renderer)
    {
        setId (_id);
        std::cout << "Creating object: " << _id << '\n';

        isInitialized = false;
    }

    ~SimpleSprite() override { getBackgroundTexture().destroy(); }

    std::vector<Component*> components;

    template <typename T>
    void addComponent (PhysicsManager* physicsManager) 
    {
        components.push_back (new PhysicsComponent (physicsManager));
        Body* body = static_cast<PhysicsComponent*>(components.back())->getBody();
        physicsManager->getWorld().addBody (body);
    }

    void setImage (const std::string& path);
    void attachScript (std::shared_ptr<Script> script) { scripts.push_back (std::move(script)); }
    std::vector<std::shared_ptr<Script>>& getScripts() { return scripts; }

    void renderAndRunScripts (Renderer* renderer) override;
    void setActive (bool state) { isActive = state; }
    void destroy();

    Scene* getScene() const { return scene; }
    void setScene (Scene* _scene) { scene = _scene; }

    virtual void update (float deltaTime) { (void)deltaTime; }

    Part* addPart (const std::string& path, const std::string& id)
    {
        std::shared_ptr<Part> part = std::make_shared<Part>(getRenderer());
        part->loadBackgroundTexture (path);
        part->setId (id);
        // part->setScene (scene);
        parts.push_back (part);
        return parts.back().get();
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

    Scene* scene; // Pointer to the parent Scene
    std::vector<std::shared_ptr<Part>> parts;
    std::vector<std::shared_ptr<Script>> scripts;
};

// Utility function to calculate normalized direction from origin to target
inline Vector2D calculateDirection (const Point2D& origin, const Point2D& target)
{
    float dx = target.x - origin.x;
    float dy = target.y - origin.y;
    float magnitude = std::sqrt (dx * dx + dy * dy);
    return magnitude > 0 ? Vector2D (dx / magnitude, dy / magnitude) : Vector2D(0, 0);
}

inline float calculateAngle (const Point2D& delta) { return static_cast<float>(atan2 (delta.y, delta.x) * 180.0 / M_PI); } //TODO: change atan2 to Point2D::calculateAngleFromOrigin()

inline Point2D getMousePosition()
{
    int x, y;
    SDL_GetMouseState (&x, &y);
    return Point2D (static_cast<float>(x), static_cast<float>(y));
}

inline Point2D getSpriteCenter (SimpleSprite* sprite)
{
    Point2D position = sprite->getPosition();
    return Point2D (position.x + sprite->getWidth() / 2, position.y + sprite->getHeight() / 2);
}

#endif
