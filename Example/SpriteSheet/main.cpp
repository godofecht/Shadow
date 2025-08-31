
#define SDL_MAIN_HANDLED

#include "SDLApp.h"

#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "InputManagement.h"
#include "Helpers.h"
#include "SideScrollerCharacterControllerScript.h"

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
        auto scene = createScene();
        scene->addItem<Player>("sprite");

        getAssetManager()->getAsset<Player>("sprite")->setBounds (50, 50, 125, 125);
        addScene (scene);
    }
};

int main (int argc, char* argv[])
{
    SpriteSheetExample app ("SpriteSheetExample", 700, 700);
    app.limitFPS(10);
    app.run();
    return 0;
}