#include "SDLApp.h"

class TankGame : public Game
{
public:
    TankGame (const char* title, int width, int height) : Game (title, width, height)
    {
    }

    void onStart() override
    {
        getAudioEngine()->addAudioMediaGroup ("main_theme");
        std::cout << "AssetManager and PhysicsManager created." << std::endl;
    }

    void initializeComponents() override
    {
        auto scene = std::make_unique<Scene>();
        auto assetManager = getAssetManager();
        scene->setAssetManager (assetManager);

        scene->addItem (assetManager->createAsset<Player>("player"));
        scene->addItem (assetManager->createAsset<Enemy>("enemy"));

        scene->getSpriteById ("player")->setSize (100, 100);
        // scene->getSpriteById ("enemy")->setSize (100, 100);

        // scene->getSpriteById ("enemy")->addComponent<PhysicsComponent> (getPhysicsManager());

        addScene (std::move (scene));

        //Always initialize audio in the init function.
        //We might ALSO have to do so after scenes... but I'm unsure.
        //TODO: Clear this up.

        // auto& a = audioEngine->getAudioMediaGroupByIndex (0);
        // // a.addAudioPlayer ("C:/Users/abhis/gamedev/Shadow/looptheme.wav", "main_theme");
        // // a.playAudio ("main_theme", true);  // Start main theme with looping
    }
};