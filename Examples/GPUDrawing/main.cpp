// GPU Drawing Showcase for Umbra Engine
// Demonstrates GPU-accelerated batched rendering with instancing
#include "Engine/Core/SDLApp.h"
#include "Engine/Core/UI.h"
#include <vector>
#include <cmath>

class GPUDrawingGame : public Game
{
    struct GPUInstance {
        float x, y;
        float size;
        Uint8 r, g, b, a;
        float rotation;
    };
    
    std::vector<GPUInstance> instances;
    SDL_Texture* gpuTexture;
    int textureSize;
    float time;
    int batchSize;
    
    // FPS counter
    int frameCount;
    float fpsTimer;
    float currentFPS;
    
    // CPU/GPU mode toggle
    int renderMode; // 0 = CPU, 1 = GPU

public:
    GPUDrawingGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {
        textureSize = 32;
        time = 0;
        batchSize = 0;
        gpuTexture = nullptr;
        frameCount = 0;
        fpsTimer = 0;
        currentFPS = 0;
        renderMode = 1; // Start in GPU mode
    }

    void onStart() override {
        Renderer* renderer = getRenderer();
        
        // Create a GPU texture for batched rendering
        gpuTexture = SDL_CreateTexture(
            renderer->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_TARGET,
            textureSize, textureSize
        );
        
        // Render to texture (GPU operation)
        SDL_SetRenderTarget(renderer->renderer, gpuTexture);
        SDL_SetRenderDrawColor(renderer->renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer->renderer);
        
        // Draw a solid circle to the texture
        for (int y = 0; y < textureSize; y++) {
            for (int x = 0; x < textureSize; x++) {
                float dx = x - textureSize/2.0f;
                float dy = y - textureSize/2.0f;
                float dist = sqrt(dx*dx + dy*dy);
                if (dist < textureSize/2.0f - 1) {
                    SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
                    SDL_RenderDrawPoint(renderer->renderer, x, y);
                }
            }
        }
        
        SDL_SetRenderTarget(renderer->renderer, nullptr);
        
        // Create many instances for GPU batched rendering demo
        for (int i = 0; i < 500; i++) {
            GPUInstance inst;              inst.x = static_cast<float>(100 + (rand() % 600));
              inst.y = static_cast<float>(100 + (rand() % 500));
            inst.size = 16; // Fixed size
            inst.r = (Uint8)(100 + rand() % 155);
            inst.g = (Uint8)(100 + rand() % 155);
            inst.b = (Uint8)(100 + rand() % 155);
            inst.a = 220;
            inst.rotation = (rand() % 360) * 3.14159f / 180.0f;
            instances.push_back(inst);
        }
        
        batchSize = static_cast<int>(instances.size());
    }

    void update() override {
        time += 0.016f;
        
        // FPS calculation
        frameCount++;
        fpsTimer += 0.016f;
        if (fpsTimer >= 1.0f) {              currentFPS = static_cast<float>(frameCount);
            frameCount = 0;
            fpsTimer = 0;
        }
        
        Renderer* renderer = getRenderer();
        
        // Clear with FULLY TRANSPARENT background
        SDL_SetRenderDrawColor(renderer->renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer->renderer);
        
        // Handle mouse click to toggle CPU/GPU mode
        int mx, my;
        Uint32 mouseState = SDL_GetMouseState(&mx, &my);
        static Uint32 lastClick = 0;
        
        // Check if click is on the toggle button area (top-right corner: 650-790, 10-40)
        if ((mouseState & SDL_BUTTON(1)) && (SDL_GetTicks() - lastClick > 300)) {
            if (mx > 650 && mx < 790 && my > 10 && my < 40) {
                renderMode = 1 - renderMode; // Toggle between 0 and 1
                lastClick = SDL_GetTicks();
            }
        }
        
        // Update and render all instances
        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_BLEND);
        
        for (auto& inst : instances) {
            // Animate positions with sine waves
            inst.x += sin(time + inst.y * 0.01f) * 0.3f;
            inst.y += cos(time + inst.x * 0.01f) * 0.3f;
            
            // Wrap around screen
            if (inst.x < 0) inst.x = 800;
            if (inst.x > 800) inst.x = 0;
            if (inst.y < 0) inst.y = 600;
            if (inst.y > 600) inst.y = 0;
            
            if (renderMode == 1) {
                // GPU MODE - Batched rendering with texture
                SDL_SetTextureColorMod(gpuTexture, inst.r, inst.g, inst.b);
                SDL_SetTextureAlphaMod(gpuTexture, inst.a);
                
                SDL_Rect destRect = {
                    (int)(inst.x - inst.size/2),
                    (int)(inst.y - inst.size/2),
                    (int)inst.size,
                    (int)inst.size
                };
                
                SDL_RenderCopy(renderer->renderer, gpuTexture, nullptr, &destRect);
            } else {
                // CPU MODE - Direct drawing (slower)
                SDL_SetRenderDrawColor(renderer->renderer, inst.r, inst.g, inst.b, inst.a);
                SDL_Rect rect = {
                    (int)(inst.x - inst.size/2),
                    (int)(inst.y - inst.size/2),
                    (int)inst.size,
                    (int)inst.size
                };
                SDL_RenderFillRect(renderer->renderer, &rect);
            }
        }
        
        // Draw UI overlay
        drawUI(renderer);
    }
    
    void drawUI(Renderer* ren) {
        // Draw FPS counter background (top-left)
        SDL_SetRenderDrawBlendMode(ren->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren->renderer, 0, 0, 0, 200);
        SDL_Rect fpsBg = {10, 10, 200, 35};
        SDL_RenderFillRect(ren->renderer, &fpsBg);
        
        // Draw FPS counter border
        SDL_SetRenderDrawColor(ren->renderer, 0, 255, 136, 255);
        SDL_RenderDrawRect(ren->renderer, &fpsBg);
        
        // Draw FPS text
        drawSimpleText(ren->renderer, 20, 15, "FPS: " + std::to_string((int)currentFPS), {0, 255, 136, 255}, 0.8f);
        
        // Draw mode button background (top-right)
        SDL_SetRenderDrawColor(ren->renderer, 0, 0, 0, 200);
        SDL_Rect modeBg = {650, 10, 140, 35};
        SDL_RenderFillRect(ren->renderer, &modeBg);
        
        // Draw mode button border
        if (renderMode == 1) {
            SDL_SetRenderDrawColor(ren->renderer, 0, 217, 255, 255); // Blue for GPU
        } else {
            SDL_SetRenderDrawColor(ren->renderer, 255, 100, 100, 255); // Red for CPU
        }
        SDL_RenderDrawRect(ren->renderer, &modeBg);
        
        // Draw mode text
        std::string modeText = (renderMode == 1) ? "GPU Mode" : "CPU Mode";
        drawSimpleText(ren->renderer, 660, 15, modeText, {255, 255, 255, 255}, 0.75f);
        
        // Draw info text (below FPS)
        SDL_SetRenderDrawColor(ren->renderer, 0, 0, 0, 180);
        SDL_Rect infoBg = {10, 50, 280, 30};
        SDL_RenderFillRect(ren->renderer, &infoBg);
        SDL_SetRenderDrawColor(ren->renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(ren->renderer, &infoBg);
        
        std::string infoText = "Particles: " + std::to_string(batchSize) + " | Click button to toggle";
        drawSimpleText(ren->renderer, 20, 55, infoText, {255, 255, 255, 255}, 0.7f);
    }

    ~GPUDrawingGame() override {
        if (gpuTexture) {
            SDL_DestroyTexture(gpuTexture);
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static GPUDrawingGame app("Umbra GPU Drawing", 800, 600);
#else
    GPUDrawingGame app("Umbra GPU Drawing", 800, 600);
#endif
    app.run();
    return 0;
}
