#pragma once

#include <box2d/box2d.h>
#include <SDL_image.h>
#include <SDL.h>
#include "Geometry.h"

enum BodyType
{
    Static,
    Dynamic,
    Kinematic
};

class Body
{
    const char* uid;

    b2BodyId bodyId;
    b2BodyDef bodyDef;    
public:
    void setUid (const char* _uid) { uid = _uid; }
    const char* getUid() { return uid; }
    b2BodyId getId() { return bodyId; }
    b2BodyDef& getDef() { return bodyDef; }
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
        id = b2CreateWorld (&def);  
    }

    void simulateStep()
    {
        for (auto body : bodies)
        {
            b2Vec2 position = b2Body_GetPosition (body->getId());
            b2Rot rotation = b2Body_GetRotation (body->getId());
            printf ("%4.2f %4.2f %4.2f\n", position.x, position.y, b2Rot_GetAngle (rotation));     
        }
        b2World_Step (id, timeStep, subStepCount);
    }

    void addBody (Body* body) { bodies.push_back (body); }

    void deleteBodyWithId (const char* id)
    {
        for (auto it = bodies.begin(); it != bodies.end(); ++it)
        {
            if ((*it)->getUid() == id)
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

        auto bodyDef = b2DefaultBodyDef();
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = origin;

        setDef (bodyDef);
        setId (b2CreateBody (world.getId(), &getDef()));

        b2Polygon dynamicBox = b2MakeBox (1.0f, 1.0f);
        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 1.0f;
        shapeDef.friction = 0.3f;

        b2CreatePolygonShape (getId(), &shapeDef, &dynamicBox);
    }
};

class PhysicsManager
{
    World world;
public:

    World& getWorld() { return world; }

    PhysicsManager()
    {
        std::cout << "Creating PhysicsManager" << std::endl;
    }
};
