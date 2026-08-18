// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include <box2d/box2d.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL.h>
#include <vector>
#include <iostream>
#include <functional>
#include <string>
#include "Engine/Core/Geometry.h"
enum BodyType
{
    Static,
    Dynamic,
    Kinematic
};

class Body
{
    std::string uid;

public:
    b2BodyId bodyId;
    b2BodyDef bodyDef;
    std::function<void()> onCollision;
    
    void setUid (const std::string& _uid) { uid = _uid; }
    std::string getUid() { return uid; }
    b2BodyId getId() { return bodyId; }
    b2BodyDef getDef() { return bodyDef; }
    void setId (b2BodyId id) { bodyId = id; }
    void setDef (b2BodyDef def) { bodyDef = def; }
};

class World
{
    b2WorldDef def;
    b2WorldId id;

    float fps = 60.0f;
    float timeStep = 1.0f / fps;
    int subStepCount = 4;

    std::vector<Body*> bodies;
public:

    b2WorldId getId() { return id; }
    b2WorldDef getDef() { return def; }

    World()
    {
        def = b2DefaultWorldDef();
        def.gravity.y = 10.0f;  // Set gravity
        id = b2CreateWorld(&def);
        
        // Validate world creation (b2_maxWorlds = 128 in Box2D v3)
        if (id.index1 == 0 || id.index1 > 128) {
            std::cerr << "Box2D ERROR: World creation failed!" << '\n';
            std::cerr << "  id.index1 = " << id.index1 << '\n';
        }
    }

    void simulateStep()
    {
        // Only step if world is valid
        if (id.index1 == 0 || id.index1 > 128) {
            return;  // Skip simulation if world is invalid
        }
        
        b2World_Step (id, timeStep, subStepCount);

        // Basic Box2D v3 contact processing
        b2ContactEvents events = b2World_GetContactEvents(id);
        for (int i = 0; i < events.beginCount; ++i) {
            b2ContactBeginTouchEvent* event = &events.beginEvents[i];

            b2BodyId bodyA = b2Shape_GetBody(event->shapeIdA);
            b2BodyId bodyB = b2Shape_GetBody(event->shapeIdB);

            // Find bodies and trigger collision callbacks if they exist
            for (auto body : bodies) {
                if (body->bodyId.index1 == bodyA.index1 || 
                    body->bodyId.index1 == bodyB.index1) {
                    if (body->onCollision) body->onCollision();
                }
            }
        }

    }

    void addBody (Body* body) { bodies.push_back (body); }

    void deleteBodyWithId (const std::string& _id)
    {
        for (auto it = bodies.begin(); it != bodies.end(); ++it)
        {
            if ((*it)->getUid() == _id)
            {
                bodies.erase (it);
                break;
            }
        }
    }
};

class RigidBody : public Body
{
    World& world;
public:
    RigidBody (World& _world) : world (_world)
    {
        b2Vec2 origin = {0.0f, 0.0f};

        bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = origin;

        setDef (bodyDef);
        bodyId = b2CreateBody (world.getId(), &bodyDef);

        b2Polygon dynamicBox = b2MakeBox (1.0f, 1.0f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 1.0f;
        shapeDef.friction = 0.3f;
        shapeDef.enableContactEvents = true;

        b2CreatePolygonShape (bodyId, &shapeDef, &dynamicBox);
        world.addBody(this);
    }
};

class PhysicsManager
{
    World world;
public:

    World& getWorld() { return world; }

    PhysicsManager()
    {
        std::cout << "Creating PhysicsManager" << '\n';
    }
};

// For the showcase
class PhysicsObject : public Body {
public:
    PhysicsObject(World& world, float x, float y, float w, float h, bool isDynamic) {
        bodyDef = b2DefaultBodyDef();
        bodyDef.type = isDynamic ? b2_dynamicBody : b2_staticBody;
        bodyDef.position = { x, y };
        bodyId = b2CreateBody(world.getId(), &bodyDef);

        b2Polygon box = b2MakeBox(w / 2.0f, h / 2.0f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.enableContactEvents = true;
        b2CreatePolygonShape(bodyId, &shapeDef, &box);
        world.addBody(this);
    }
};
