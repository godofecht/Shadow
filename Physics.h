// #include <box2d/box2d.h>
// #include <SDL_image.h>
// #include <SDL.h>
// #include <Box2D/Box2D.h>
// #include <vector>
// #include <cmath>
// #include <string>
// #include <iostream>
// #include "SDLManager.h"
// #include <functional>
// #include "Geometry.h"

// class PhysicsManager; // Forward declaration

// // Helper class for collision-related calculations
// class CollisionHelpers
// {
// public:
//     const int EDGE_THRESHOLD = 100; // Adjust based on the color threshold for edges
//     const float SIMPLIFY_THRESHOLD = 5.0f; // Threshold for simplification

//     std::vector<b2Vec2> extractContours(SDL_Surface* surface)
//     {
//         std::vector<b2Vec2> points;

//         SDLManager::lockSurface(surface);
//         Uint32* pixels = (Uint32*)surface->pixels;
//         int width = surface->w;
//         int height = surface->h;

//         // Edge detection by scanning pixels
//         for (int y = 1; y < height - 1; ++y)
//         {
//             for (int x = 1; x < width - 1; ++x)
//             {
//                 Uint32 currentPixel = pixels[y * width + x];
//                 Uint32 rightPixel = pixels[y * width + (x + 1)];
//                 Uint32 belowPixel = pixels[(y + 1) * width + x];

//                 if (abs((int)(currentPixel - rightPixel)) > EDGE_THRESHOLD ||
//                     abs((int)(currentPixel - belowPixel)) > EDGE_THRESHOLD)
//                 {
//                     points.emplace_back(b2Vec2((float)x, (float)y));
//                 }
//             }
//         }

//         SDL_UnlockSurface(surface);
//         return points;
//     }

//     void simplifySegment(const std::vector<b2Vec2>& points, std::vector<b2Vec2>& simplified, float epsilon, int start, int end)
//     {
//         float maxDist = 0.0f;
//         int index = start;

//         for (int i = start + 1; i < end; i++)
//         {
//             float dist = abs((points[end].y - points[start].y) * points[i].x -
//                              (points[end].x - points[start].x) * points[i].y +
//                              points[end].x * points[start].y - points[end].y * points[start].x) /
//                              sqrt(pow(points[end].y - points[start].y, 2) +
//                                   pow(points[end].x - points[start].x, 2));
//             if (dist > maxDist)
//             {
//                 maxDist = dist;
//                 index = i;
//             }
//         }

//         if (maxDist > epsilon)
//         {
//             if (start < index) simplifySegment(points, simplified, epsilon, start, index);
//             simplified.push_back(points[index]);
//             if (index < end) simplifySegment(points, simplified, epsilon, index, end);
//         }
//     }

//     std::vector<b2Vec2> simplifyContour(const std::vector<b2Vec2>& contourPoints)
//     {
//         if (contourPoints.size() < 3) return contourPoints;

//         std::vector<b2Vec2> simplified;
//         simplified.push_back(contourPoints[0]);
//         simplifySegment(contourPoints, simplified, SIMPLIFY_THRESHOLD, 0, contourPoints.size() - 1);
//         simplified.push_back(contourPoints.back());

//         return simplified;
//     }
// };

// // World and Physics Manager combined
// class World
// {
// public:
//     World(b2Vec2 gravity = b2Vec2(0.0f, 0.0f)) : world(gravity) {}

//     b2World& getInternalWorld() { return world; }

//     void step(float timeStep, int32 velocityIterations, int32 positionIterations)
//     {
//         world.Step(timeStep, velocityIterations, positionIterations);
//     }

// private:
//     b2World world;
// };

// class PhysicsManager
// {
// public:
//     PhysicsManager(b2Vec2 gravity = b2Vec2(0.0f, 0.0f)) : world(gravity) {}

//     b2Body* createBody(const b2BodyDef& bodyDef)
//     {
//         return world.getInternalWorld().CreateBody(&bodyDef);
//     }

//     void step(float timeStep, int32 velocityIterations, int32 positionIterations)
//     {
//         world.step(timeStep, velocityIterations, positionIterations);
//     }

//     World& getWorld() { return world; }

//     void setGravity(const b2Vec2& newGravity)
//     {
//         world.getInternalWorld().SetGravity(newGravity);
//     }

//     b2PolygonShape calculateCollisionPolygonFromImage(const char* imagePath)
//     {
//         // Load image with SDL
//         SDL_Surface* surface = IMG_Load(imagePath);
//         if (!surface)
//         {
//             std::cerr << "Error loading image: " << IMG_GetError() << std::endl;
//             return b2PolygonShape();
//         }

//         // Utilize CollisionHelpers for contour extraction and simplification
//         CollisionHelpers collisionHelpers;
//         std::vector<b2Vec2> contourPoints = collisionHelpers.extractContours(surface);
//         std::vector<b2Vec2> polygonPoints = collisionHelpers.simplifyContour(contourPoints);

//         // Create Box2D polygon shape
//         b2PolygonShape polygonShape;
//         polygonShape.Set(polygonPoints.data(), polygonPoints.size());

//         // Cleanup
//         SDL_FreeSurface(surface);

//         return polygonShape;
//     }

// private:
//     World world;
// };

// class Body
// {
// public:
//     Body(PhysicsManager* physicsManager, b2BodyType bodyType, float posX, float posY)
//     {
//         // Box2D Body Definition
//         b2BodyDef bodyDef;
//         bodyDef.type = bodyType;
//         bodyDef.position.Set(posX, posY);
//         body = physicsManager->createBody(bodyDef);
//         body->SetUserData(this);
//     }

//     void createFixture(const b2Shape& shape, const PhysicsProperties& properties)
//     {
//         b2FixtureDef fixtureDef;
//         fixtureDef.shape = &shape;
//         fixtureDef.density = properties.density;
//         fixtureDef.friction = properties.friction;
//         fixtureDef.restitution = properties.restitution;
//         body->CreateFixture(&fixtureDef);
//     }

//     void setCollisionCallback(std::function<void(Body*, Body*)> callback)
//     {
//         collisionCallback = callback;
//     }

//     void onCollision(Body* other)
//     {
//         if (collisionCallback)
//         {
//             collisionCallback(this, other);
//         }
//     }

//     Vector2D getPosition() const { return Vector2D(body->GetPosition().x, body->GetPosition().y); }
//     float getAngle() const { return body->GetAngle(); }
//     b2Body* getInternalBody() const { return body; }

// private:
//     b2Body* body;
//     std::function<void(Body*, Body*)> collisionCallback;
// };

// class PhysicsProperties
// {
// public:
//     PhysicsProperties(float density = 1.0f, float friction = 0.3f, float restitution = 0.0f)
//         : density(density), friction(friction), restitution(restitution) {}

//     float density;
//     float friction;
//     float restitution;
// };

// class Collider
// {
// public:
//     Collider(Body* body, PhysicsManager* physicsManager) : body(body), physicsManager(physicsManager) {}

//     void setCollisionCallback(std::function<void(Body*, Body*)> callback)
//     {
//         body->setCollisionCallback(callback);
//     }

// private:
//     Body* body;
//     PhysicsManager* physicsManager;
// };

#pragma once

#include <box2d/box2d.h>
#include <SDL_image.h>
#include <SDL.h>
#include <Box2D/Box2D.h>
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
                bodies.erase(it);
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
