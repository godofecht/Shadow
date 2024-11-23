#pragma once
#include "Renderer.h"
#include "Helpers.h"

class Object
{
    std::string id;
    Texture texture;
    Rect<float> bounds {0, 0, 0, 0};
    float angle = 0.0f;
    Texture backgroundTexture;

    Renderer* renderer;
public:

    bool isInitialized = false;
    bool isActive = true;

    Object (Renderer* _renderer) : renderer (_renderer){}

    Renderer* getRenderer() { return renderer; }
    Texture& getBackgroundTexture() { return backgroundTexture; }
    Rect<float> getBounds() { return bounds; }
    float getAngle() { return angle; }
    void setTexture (Texture& texture) { this->texture = texture; }
    void setBounds (Rect<float> bounds) { this->bounds = bounds; }
    void setAngle (float angle) { this->angle = angle; } //rotation is really in degrees

    void setSize (float width, float height)
    {
        bounds.width = width;
        bounds.height = height;
    }

    bool loadBackgroundTexture (const std::string& path)
    {
        getBackgroundTexture().destroy();

        auto loadedSurface = loadSurfaceFromRenderer (path);
        if (loadedSurface == nullptr)
        {
            std::cerr << "Unable to load image " << path << "! SDL_image Error: " << IMG_GetError() << std::endl;
            return false;
        }

        getBackgroundTexture().createFromSurface (renderer->renderer, loadedSurface, path);
        return true;
    }

    void setBounds (float x, float y, float width, float height) { bounds = Rect<float>(x, y, width, height); }
    Rect<float> getBounds() const { return bounds; }

    void setPosition (Point2D position)
    {
        bounds.x = position.x;
        bounds.y = position.y;
    }

    void setCenter (Point2D position)
    {
        bounds.x = position.x - bounds.width / 2;
        bounds.y = position.y - bounds.height / 2;
    }

    Point2D getCenter() const
    {
        float xOut = bounds.x + bounds.width / 2;
        float yOut = bounds.y + bounds.height / 2;
        return {xOut, yOut};
    }

    float getX() const { return bounds.x; }
    float getY() const { return bounds.y; }
    void setX (float x) { bounds.x = x; }
    void setY (float y) { bounds.y = y; }
    Point2D getPosition() const { return {bounds.x, bounds.y}; }

    float getWidth() const { return bounds.width; }
    float getHeight() const { return bounds.height; }

    std::string getId() const { return id; }
    void setId (const std::string& id) { this->id = id; }

    virtual void renderAndRunScripts() = 0;
};
