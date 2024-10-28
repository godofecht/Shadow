#ifndef SPRITE_H
#define SPRITE_H

#include <SDL.h>
#include <string>
#include <vector>
#include <memory>
#include "Asset.h"
#include "Script.h"
#include <iostream>

class Rect
{
public:
    Rect(int x, int y, int width, int height) : x(x), y(y), w(width), h(height) {}
    int x, y, w, h;
};

class Sprite : public Object
{
public:
    Sprite(SDL_Renderer* renderer, const std::string& path, const std::string& _id);
    ~Sprite();

    void setSize(int width, int height);
    void setBounds(int x, int y, int width, int height);
    Rect getBounds() {return Rect (x, y, width, height);}
    void setPosition(int x, int y);
    void getPosition(int& xOut, int& yOut);
    void setRotation (float angle) { rotation = angle; }
    float getRotation() {return rotation;}
    void setImage(const std::string& path);
    void attachScript(std::shared_ptr<Script> script);

    float rotation = 0.0f;

    int getX() {return x;}
    int getY() {return y;}

    void setX(int _x) {x = _x;}
    void setY(int _y) {y = _y;}

    std::string getId() {return id;}
 
    virtual void renderAndRunScripts(SDL_Renderer* renderer) override;

    void setActive (bool state) { isActive = state; }

    void destroy();

    SDL_Renderer* getRenderer() { return renderer; }

    int getWidth() {return width;}
    int getHeight() {return height;}

private:
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    bool isActive = true;
    std::string id;
    int x, y;
    int width, height;
    std::vector<std::shared_ptr<Script>> scripts;

    bool loadTexture(const std::string& path);
};

#endif
