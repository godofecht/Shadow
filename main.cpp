#include "SDLApp.h"


// class Game : public SDLApp
// {
//     std::shared_ptr<Sprite> player;
// public:

//     Game (const char* title, int width, int height) : SDLApp (title, width, height)
//     {
//     }

//     void init() override
//     {
//         player = getAssetManager()->createAsset<Sprite>("knob1");
//         player->setBounds (200, 200, 50, 50);
//         player->setImage("/Users/abhishekshivakumar/gamedev/shadow/knob1.png");

//         std::shared_ptr<Script> characterController = std::make_shared<CharacterControllerScript>(player.get(), 5);
//         player->attachScript(characterController);
//     }

//     void update() override
//     {
//         player->renderAndRunScripts (getRenderer());
//     }
// };

// int main(int argc, char* args[])
// {
//     Game app("SDL Asset Management Example", 800, 600);
//     app.run();
//     return 0;
// }

class Player : public Sprite
{
public:

    Player(SDL_Renderer* renderer, const std::string& path, const std::string& _id) : Sprite (renderer, path, _id)
    {
        std::cout << "Creating object: " << getId() << std::endl;
        auto controller = std::make_shared<CharacterControllerScript>(this, 5);
        attachScript (controller);
    }
};

class Fly : public Sprite
{
public:

    Fly (SDL_Renderer* renderer, const std::string& path, const std::string& _id) : Sprite (renderer, path, _id)
    {
        std::cout << "Creating object: " << getId() << std::endl;
        auto controller = std::make_shared<FlyControllerScript>(this, 5);
        attachScript (controller);
    }
};

class Game : public SDLApp
{
    std::shared_ptr<Player> player;
    std::shared_ptr<Fly> fly;

    std::vector<std::shared_ptr<Sprite>> itemsToRender;
    std::vector<std::shared_ptr<Fly>> flies;
public:

    Game (const char* title, int width, int height) : SDLApp (title, width, height)
    {}

    //If you want to render the asset, you must call this function.
    template <typename T>
    std::shared_ptr<T> addAsset (std::string Id, std::string path = "") //TODO: seriously... wtf passing in path along with..
    {
        auto asset = getAssetManager()->createAsset<T>(Id, path);
        itemsToRender.push_back (asset);
        return asset;
    }

    void init() override
    {
        player = addAsset<Player>("player");
        player->setBounds (200, 200, 50, 50);

        // Create a swarm of flies and give them random locations
        std::srand(static_cast<unsigned int>(std::time(nullptr))); // Seed for random number generation

        for (int i = 0; i < 20; ++i) // Create 10 flies
        {
            auto fly = addAsset<Fly>("fly" + std::to_string(i), "/Users/abhishekshivakumar/gamedev/shadow/fly.png");
            int x = std::rand() % 800; // Random x position within the window width
            int y = std::rand() % 600; // Random y position within the window height
            fly->setBounds(x, y, 20, 20);
            flies.push_back(fly);
        }
    }

    void update() override
    {
        for (auto& item : itemsToRender)
        {
            item->renderAndRunScripts (getRenderer());
        }

        // Draw lines between all the flies
        for (size_t i = 0; i < flies.size(); ++i)
        {
            for (size_t j = i + 1; j < flies.size(); ++j)
            {
            SDL_SetRenderDrawColor(getRenderer(), 255, 255, 255, SDL_ALPHA_OPAQUE); // Set color to white
            SDL_RenderDrawLine(getRenderer(), 
                       flies[i]->getBounds().x + flies[i]->getBounds().w / 2, 
                       flies[i]->getBounds().y + flies[i]->getBounds().h / 2, 
                       flies[j]->getBounds().x + flies[j]->getBounds().w / 2, 
                       flies[j]->getBounds().y + flies[j]->getBounds().h / 2);
            }
        }
    }
};

int main(int argc, char* args[])
{
    Game app("SDL Asset Management Example", 1800, 1600);
    app.run();
    return 0;
}

