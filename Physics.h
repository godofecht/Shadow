#include <box2d/box2d.h>
#include <SDL_image.h>
#include <SDL.h>
#include <Box2D/Box2D.h>
#include <vector>
#include <cmath>
#include <string>
#include <iostream>
#include "SDLManager.h"

// Helper class for collision-related calculations
class CollisionHelpers
{
public:
    const int EDGE_THRESHOLD = 100; // Adjust based on the color threshold for edges
    const float SIMPLIFY_THRESHOLD = 5.0f; // Threshold for simplification

    // Edge detection and contour extraction
    std::vector<b2Vec2> extractContours (SDL_Surface* surface)
    {
        std::vector<b2Vec2> points;


        SDLManager::lockSurface (surface);
        Uint32* pixels = (Uint32*) surface->pixels;
        int width = surface->w;
        int height = surface->h;

        // Edge detection by scanning pixels
        for (int y = 1; y < height - 1; ++y)
        {
            for (int x = 1; x < width - 1; ++x)
            {
                Uint32 currentPixel = pixels[y * width + x];
                Uint32 rightPixel = pixels[y * width + (x + 1)];
                Uint32 belowPixel = pixels[(y + 1) * width + x];

                if (abs ((int)(currentPixel - rightPixel)) > EDGE_THRESHOLD || 
                    abs ((int)(currentPixel - belowPixel)) > EDGE_THRESHOLD) 
                {
                    points.emplace_back (b2Vec2 ((float)x, (float)y));
                }
            }
        }

        SDL_UnlockSurface(surface);
        return points;
    }

    // Douglas-Peucker simplification algorithm (recursive)
    void simplifySegment (const std::vector<b2Vec2>& points, std::vector<b2Vec2>& simplified, float epsilon, int start, int end)
    {
        float maxDist = 0.0f;
        int index = start;

        for (int i = start + 1; i < end; i++)
        {
            float dist = abs((points[end].y - points[start].y) * points[i].x - 
                             (points[end].x - points[start].x) * points[i].y + 
                              points[end].x * points[start].y - points[end].y * points[start].x) / 
                              sqrt (pow(points[end].y - points[start].y, 2) + 
                              pow (points[end].x - points[start].x, 2));
            if (dist > maxDist)
            {
                maxDist = dist;
                index = i;
            }
        }

        if (maxDist > epsilon)
        {
            if (start < index) simplifySegment (points, simplified, epsilon, start, index);
            simplified.push_back (points[index]);
            if (index < end) simplifySegment (points, simplified, epsilon, index, end);
        }
    }

    std::vector<b2Vec2> simplifyContour (const std::vector<b2Vec2>& contourPoints)
    {
        if (contourPoints.size() < 3) return contourPoints;

        std::vector<b2Vec2> simplified;
        simplified.push_back (contourPoints[0]);
        simplifySegment (contourPoints, simplified, SIMPLIFY_THRESHOLD, 0, contourPoints.size() - 1);
        simplified.push_back (contourPoints.back());

        return simplified;
    }
};

//TODO: fix this fuckin physics manager...
// link frickin box 2 dingus
class PhysicsManager
{
public:
    PhysicsManager()
    {
        b2WorldDef worldDef = b2DefaultWorldDef();
        worldDef.gravity = (b2Vec2){ 0.0f, -10.0f };
        b2WorldId worldId = b2CreateWorld (&worldDef);
    }

    b2Polygon calculateCollisionPolygonFromImage(char* imagePath)
    {
        // Load image with SDL
        SDL_Surface* surface = IMG_Load (imagePath);

        // Utilize CollisionHelpers for contour extraction and simplification
        CollisionHelpers collisionHelpers;
        std::vector<b2Vec2> contourPoints = collisionHelpers.extractContours (surface);
        std::vector<b2Vec2> polygonPoints = collisionHelpers.simplifyContour (contourPoints);

        // Create Box2D polygon shape
        b2Polygon polygonShape;

        // Apply polygon points to polygon shape

        // Cleanup
        SDL_FreeSurface (surface);

        return polygonShape;
    }
};
