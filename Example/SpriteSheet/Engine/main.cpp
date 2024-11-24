
#define SDL_MAIN_HANDLED

#pragma once
#include "SDLApp.h"

#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>

class Player : public Sprite
{
    public:

    Player()
    {

    }


};


class TopDownTileGame : public Game
{
    public:
    TopDownTileGame (const char* title, int width, int height) : Game (title, width, height)
    {

    }

    void onStart() override
    {

    }

    void initializeComponents() override
    {
        auto& scene = createScene();
        scene->addItem<Player>("sprite");
        addScene (scene);
    }
};


int main (int argc, char* argv[])
{
    TopDownTileGame app ("Tank Example", 700, 700);
    app.run();
    return 0;
}