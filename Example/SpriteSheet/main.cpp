
#define SDL_MAIN_HANDLED

#pragma once
#include "SDLApp.h"

#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>

class Player : public AnimatedSprite
{
    public:

    Player (Renderer* renderer, const std::string& _id) : AnimatedSprite (renderer, "player")
    {
        auto s = "W:/Downloads/Free 3 Cyberpunk Sprites Pixel Art/2 Punk/Punk_run.png";
        loadSpriteSheet(new Image (s), 48, 48, 6);
    }
};

class SpriteSheetExample : public Game
{
    public:
    SpriteSheetExample (const char* title, int width, int height) : Game (title, width, height)
    {

    }

    void onStart() override
    {

    }

    void initializeComponents() override
    {
        auto scene = std::make_unique<Scene>();
        auto assetManager = getAssetManager();
        scene->setAssetManager(assetManager);
        scene->addItem(assetManager->createAsset<Player>("sprite"));
        getAssetManager()->getAsset<Player>("sprite")->setBounds(Rect<float>(50, 50, 50, 50));
        addScene(std::move(scene));
    }
};



int main (int argc, char* argv[])
{
    SpriteSheetExample app ("SpriteSheetExample", 700, 700);
    app.limitFPS(10);
    app.run();
    return 0;
}