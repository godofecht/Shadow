#include "SDLApp.h"
#include <iostream>

Game::Game(const char* _title, int width, int height)
    : window (nullptr), renderer (nullptr), screenWidth (width), screenHeight (height), isRunning (true), assetManager (nullptr)
{
    renderer = std::make_unique<Renderer>();
    title = _title;
    isInitialized = start();
}

Game::~Game()
{
    cleanup();
}

bool Game::start()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "SDL initialized." << std::endl;

    window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              screenWidth,
                              screenHeight,
                              SDL_WINDOW_SHOWN);

    if (window == nullptr)
    {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "Window created." << std::endl;

    if (!renderer->initialize (window)) return false;
    
    textWriter = std::make_unique<TextWriter>(renderer.get());

    std::cout << "Renderer created." << std::endl;


    assetManager = std::make_unique<AssetManager>(renderer.get());

    return true;
}

void Game::run()
{
    if (!isInitialized) return;

    init();

    SDL_Event event;
    FPSCounter fpsCounter;  // Create an instance of FPSCounter

    while (isRunning)
    {
        Uint32 frameStart = SDL_GetTicks();  // Start the frame timer

        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                isRunning = false;
            }
        }

        clearScreen(0x00, 0x00, 0x00, 0xFF);

        update();

        // Get the current FPS and log it or display it if necessary
        float fps = fpsCounter.getFPS();
//        std::cout << "FPS: " << fps << std::endl;

#ifdef _WIN32
        // Create the texture for text rendering
        SDL_Texture* textTexture = SDL_CreateTexture(renderer->renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, 800, 600);

        // Clear textTexture with a transparent color to avoid overdraw
        SDL_SetTextureBlendMode(textTexture, SDL_BLENDMODE_BLEND);
        void* pixels;
        int pitch;
        SDL_LockTexture(textTexture, nullptr, &pixels, &pitch);
        memset(pixels, 0, pitch * 600);  // Clear with transparent color
        SDL_UnlockTexture(textTexture);

        // Render text using DirectWrite on SDL window
        textWriter->RenderTextToTexture(L"FPS: " + std::to_wstring(fps), textTexture, Col::green());

        // Draw the texture to the SDL renderer
        SDL_SetTextureAlphaMod(textTexture, 128); // Set alpha to 50%
        SDL_RenderCopy(renderer->renderer, textTexture, NULL, NULL);

        // Clean up texture after rendering
        SDL_DestroyTexture(textTexture);
#endif

        presentScreen();

        // Limit FPS
        limitFPS(fpsLimit);
    }
}


void Game::init()
{
    auto scene = std::make_unique<Scene>();

    scene->addItem (assetManager->createAsset<Player>("player"));
    scene->setAssetManager (assetManager.get());

    scenes.push_back (std::move(scene));
}

void Game::clearScreen(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    renderer->clearScreen (r, g, b, a);
}

void Game::presentScreen()
{
    renderer->present();
}

void Game::cleanup()
{
    textWriter.reset();
    if (renderer != nullptr)
    {

        renderer->destroy();
    }
    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
    }
    IMG_Quit();
    SDL_Quit();
}

void Game::update()
{
    for (auto& scene : scenes)
    {
        scene->render (renderer.get());
    }
}