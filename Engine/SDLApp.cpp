#include "SDLApp.h"
#include <iostream>

Game::Game(const char* _title, int width, int height)
    : renderer(nullptr), screenWidth(width), screenHeight(height), isRunning(true)
{
    renderer = std::make_unique<Renderer>();
    physicsManager = std::make_unique<PhysicsManager>();
    audioEngine = std::make_unique<AudioEngine>();

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

    if (!audioEngine->initialize()) return false;
    std::cout << "AudioEngine initialized." << std::endl;

    onStart();

    return true;
}

void Game::run()
{
    if (!isInitialized) return;

    init();

    SDL_Event event;
    FPSCounter fpsCounter;

    while (isRunning)
    {
        Uint32 frameStart = SDL_GetTicks();

        while (SDL_PollEvent (&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                isRunning = false;
            }
        }

        clearScreen (0x00, 0x00, 0x00, 0xFF);
        update();

        drawFPS();

        presentScreen();
        limitFPS (fpsLimit);
    }
}

void Game::init()
{
    initializeComponents();
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
    audioEngine->stopAudioInAllGroups();  // Stop main theme playback
    renderer->reset();
    if (renderer != nullptr) renderer->destroy();
    window.exit();
    SDLManager::handleExit();
}

void Game::update()
{
    physicsManager->getWorld().simulateStep();

    for (auto& scene : scenes)
        scene->render (renderer.get());
}
