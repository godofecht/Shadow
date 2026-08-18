// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details

#include "Engine/Core/SDLApp.h"
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

Game::Game(const char* _title, int width, int height)
    : renderer(nullptr), screenWidth(width), screenHeight(height), isRunning(true), assetManager(nullptr)
{
    renderer = std::make_unique<Renderer>();
    physicsManager = std::make_unique<PhysicsManager>();
    audioEngine = std::make_unique<AudioEngine>();

    title = _title;
    isInitialized = false;
}

Game::~Game()
{
    if (isInitialized)
    {
        quit();
    }
}

bool Game::start()
{
    SDLManager::initVideo();

    std::cout << "SDL initialized." << '\n';

    window.initialize (title, screenWidth, screenHeight);

    if (!renderer->initialize (window.window)) return false;
    std::cout << "Renderer and DirectX TextWriter created." << '\n';

    assetManager = std::make_unique<AssetManager>(renderer.get());

    if (!audioEngine->initialize()) return false;
    std::cout << "AudioEngine initialized." << '\n';

    return true;
}

void Game::run()
{
#ifdef __EMSCRIPTEN__
    // Keep control in Emscripten's RAF-driven main loop.
    emscripten_set_main_loop_arg(Game::loop, this, 0, 1);
#else
    if (!isInitialized)
    {
        isInitialized = start();
        if (!isInitialized) return;
        onStart();
        init();
    }

    while (isRunning)
    {
        mainLoop();
    }
#endif
}

void Game::loop(void* arg)
{
    Game* game = static_cast<Game*>(arg);

    if (!game->isInitialized)
    {
        game->isInitialized = game->start();
        if (!game->isInitialized)
        {
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
            return;
        }

        game->onStart();
        game->init();
    }

    game->mainLoop();
}

void Game::mainLoop()
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        if (event.type == SDL_QUIT)
        {
            isRunning = false;
        }
    }

    clearScreen(0x00, 0x00, 0x00, 0xFF);
    update();
    renderPostFX();

    drawFPS();
    
    presentScreen();
    limitFPS(fpsLimit);

#ifdef __EMSCRIPTEN__
    // Headless browser smoke marker: expose the completed-frame count to JS
    // so the CI browser step can assert the requestAnimationFrame loop is
    // live (SDL init + first update/render/present succeeded). Reaching this
    // point means the game reached a running frame.
    EM_ASM({
        if (typeof window !== 'undefined') {
            window.__umbraFrameCount = (window.__umbraFrameCount || 0) + 1;
        }
    });
#endif
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
