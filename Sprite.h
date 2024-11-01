#ifndef SPRITE_H
#define SPRITE_H


#include <string>
#include <vector>
#include <memory>
#include "Asset.h"
#include "Script.h"
#include <iostream>
#include "Geometry.h"
#include "Texture.h"




class Scene;
class Sprite : public Object
{
public:

    Sprite (Renderer* renderer, const std::string& path, const std::string& _id)
        : renderer (renderer), id (_id), bounds (0, 0, 0, 0) // Initialize bounds at (0,0) with zero width and height
    {
        std::cout << "Creating object: " << id << std::endl;

        loadTexture (path);
        isInitialized = true;
    }

    ~Sprite()
    {
        texture.destroy();
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
    void setX(float x) { bounds.x = x; }
    void setY(float y) { bounds.y = y; }

    std::string getId() const { return id; }
    virtual void renderAndRunScripts (Renderer* renderer) override;
    void setActive (bool state) { isActive = state; }
    void destroy();
    Renderer* getRenderer() { return renderer; }

    float getWidth() const { return bounds.width; }
    float getHeight() const { return bounds.height; }
    Scene* getScene() const { return scene; }
    void setScene (Scene* _scene) { scene = _scene; }

    virtual void update(float deltaTime) {}

    bool isInitialized = false;
    bool isActive = true;

private:
    Renderer* renderer;
    Texture texture;
    Scene* scene; // Pointer to the parent Scene

    std::string id;

    float rotation = 0.0f;
    Rect<float> bounds; // Holds position (x, y), width, and height
    std::vector<std::shared_ptr<Script>> scripts;

    bool loadTexture (const std::string& path);
};

#endif
