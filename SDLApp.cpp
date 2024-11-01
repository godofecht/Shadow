#include "SDLApp.h"
#include <iostream>

Game::Game(const char* _title, int width, int height)
    : renderer (nullptr), screenWidth (width), screenHeight (height), isRunning (true), assetManager (nullptr)
{
    renderer = std::make_unique<Renderer>();
    title = _title;
    isInitialized = start();
}

Game::~Game()
{
    quit();
}

bool Game::start()
{
    SDLManager::initVideo();

    std::cout << "SDL initialized." << std::endl;

    window.initialize (title, screenWidth, screenHeight);

    if (!renderer->initialize (window.window)) return false;
    std::cout << "Renderer and DirectX TextWriter created." << std::endl;

    assetManager = std::make_unique<AssetManager>(renderer.get());

    return true;
}

void Game::run()
{
    if (!isInitialized) return;

    init();

    SDL_Event event;
    FPSCounter fpsCounter;  // Create an instance of FPSCounter

    scenes[0]->getSpriteById ("enemy")->setPosition (100, 100);

    while (isRunning)
    {
        Uint32 frameStart = SDL_GetTicks();  // Start the frame timer

        while (SDL_PollEvent (&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                isRunning = false;
            }
        }

        clearScreen (0x00, 0x00, 0x00, 0xFF);

        update();

        // Get the current FPS and log it or display it if necessary
        float fps = fpsCounter.getFPS();
//        std::cout << "FPS: " << fps << std::endl;


        renderer->getTextWriter()->drawTextToRenderer (std::to_wstring (fps), renderer->renderer);

        presentScreen();

        // Limit FPS
        limitFPS (fpsLimit);
    }
}


void Game::init()
{
    //TODO: combine the two lines below into one
    auto scene = std::make_unique<Scene>();
    scene->setAssetManager (assetManager.get());

    scene->addItem (assetManager->createAsset<Player>("player"));
    scene->addItem (assetManager->createAsset<Enemy>("enemy"));

    scenes.push_back (std::move (scene));
}

void Game::clearScreen (Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    renderer->clearScreen (r, g, b, a);
}

void Game::presentScreen()
{
    renderer->present();
}

void Game::quit()
{
    renderer->reset();
    if (renderer != nullptr)
    {

        renderer->destroy();
    }
    window.exit();
    SDLManager::handleExit();
}

void Game::update()
{
    for (auto& scene : scenes)
    {
        scene->render (renderer.get());
    }
}