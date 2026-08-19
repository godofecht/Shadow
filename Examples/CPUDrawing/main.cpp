// CPU Drawing Showcase for Umbra Engine
// Manually read/write the pixels of a texture
#include "Engine/Core/SDLApp.h"
#include <vector>
#include <cmath>

class CPUDrawingGame : public Game
{
    SDL_Texture* cpuTexture;
    int textureWidth;
    int textureHeight;
    float time;

    void updateTexture() {
        // Lock the texture for CPU access
        void* pixels;
        int pitch;
        
        if (SDL_LockTexture(cpuTexture, nullptr, &pixels, &pitch) != 0) {
            return;
        }
        
        Uint32* pixelData = (Uint32*)pixels;
        
        // Write pixels directly from CPU
        for (int y = 0; y < textureHeight; y++) {
            for (int x = 0; x < textureWidth; x++) {
                // Create animated pattern
                float nx = x / (float)textureWidth;
                float ny = y / (float)textureHeight;
                
                // Animated wave pattern
                float wave = sin(nx * 10.0f + time) * cos(ny * 8.0f + time * 0.7f);
                float spiral = sin(sqrt(nx*nx + ny*ny) * 20.0f - time * 2.0f);
                
                // Combine patterns
                float value = (wave + spiral) / 2.0f;
                
                // Convert to color (heatmap style)
                Uint8 r = (Uint8)(255 * (value + 1) / 2);
                Uint8 g = (Uint8)(255 * (1 - fabs(value)));
                Uint8 b = (Uint8)(255 * (1 - (value + 1) / 2));
                
                // Create pixel value (RGBA format)
                pixelData[y * (pitch / 4) + x] = (255u << 24) | (static_cast<Uint32>(r) << 16) | (static_cast<Uint32>(g) << 8) | static_cast<Uint32>(b);
            }
        }
        
        SDL_UnlockTexture(cpuTexture);
    }

public:
    CPUDrawingGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {
        textureWidth = 256;
        textureHeight = 256;
        cpuTexture = nullptr;
        time = 0;
    }

    void onStart() override {
        Renderer* renderer = getRenderer();
        
        // Create a streaming texture for CPU access
        cpuTexture = SDL_CreateTexture(
            renderer->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            textureWidth,
            textureHeight
        );
    }

    void update() override {
        time += 0.016f;
        updateTexture();
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(20, 20, 30, 255);
        
        // Get renderer dimensions
        int w, h;
        SDL_GetRendererOutputSize(renderer->renderer, &w, &h);
        
        // Render the CPU-drawn texture
        SDL_Rect destRect = {
            (w / 2 - textureWidth / 2),
            (h / 2 - textureHeight / 2),
            textureWidth,
            textureHeight
        };
        SDL_RenderCopy(renderer->renderer, cpuTexture, nullptr, &destRect);
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        // Draw info text
        std::wstring info = L"CPU Drawing - Direct Pixel Manipulation\nClick to read pixels back";
        Rect<float> textBounds(10, 10, 400, 50);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
        
        // Handle pixel readback on click
        int mx, my;
        if (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(1)) {
            static Uint32 lastClick = 0;
            if (SDL_GetTicks() - lastClick > 500) {
                // Read pixels from texture
                SDL_Rect readRect = {mx - 5, my - 5, 10, 10};
                Uint32 pixels[100];
                if (SDL_RenderReadPixels(renderer->renderer, &readRect, 
                    SDL_PIXELFORMAT_ARGB8888, pixels, 10 * sizeof(Uint32)) == 0) {
                    // Successfully read pixels
                    wchar_t debug[100];
                    swprintf(debug, 100, L"Pixel at (%d,%d): %08X", mx, my, pixels[0]);
                    Rect<float> debugBounds(10, 70, 300, 30);
                    renderer->getTextWriter()->drawTextToRenderer(debug, renderer->renderer, debugBounds, "/default.ttf");
                }
                lastClick = SDL_GetTicks();
            }
        }
    }

    ~CPUDrawingGame() override {
        if (cpuTexture) {
            SDL_DestroyTexture(cpuTexture);
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    CPUDrawingGame app("Umbra CPU Drawing Showcase", 800, 600);
    app.run();
    return 0;
}
