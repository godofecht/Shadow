#include "SDLApp.h"
#include <iostream>

SDLApp::SDLApp(const char* title, int width, int height)
    : window(nullptr), renderer(nullptr), screenWidth(width), screenHeight(height), isRunning(true), assetManager(nullptr)
{
}

SDLApp::~SDLApp()
{
    cleanup();
}

bool SDLApp::start()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "SDL initialized." << std::endl;

    window = SDL_CreateWindow("SDL Asset Management Example",
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

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr)
    {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "Renderer created." << std::endl;

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        std::cerr << "SDL_image could not initialize! SDL_image Error: " << IMG_GetError() << std::endl;
        return false;
    }

    assetManager = std::make_unique<AssetManager>(renderer);
    return true;
}

void SDLApp::run()
{
    if (!start()) return;

    init();

    SDL_Event event;
    while (isRunning)
    {
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                isRunning = false;
            }
        }

        clearScreen (0x00, 0x00, 0x00, 0xFF);
        update();
        presentScreen();
    }
}

void SDLApp::clearScreen(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderClear(renderer);
}

void SDLApp::presentScreen()
{
    SDL_RenderPresent(renderer);
}

void SDLApp::cleanup()
{
    if (renderer != nullptr)
    {
        SDL_DestroyRenderer(renderer);
    }
    if (window != nullptr)
    {
        SDL_DestroyWindow(window);
    }
    IMG_Quit();
    SDL_Quit();
}
