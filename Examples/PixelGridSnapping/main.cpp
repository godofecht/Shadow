// Pixel Grid Snapping Showcase for Umbra Engine
// Shows how to create graphics that snap to the pixel grid by rendering to a texture in 2D
#include "Engine/Core/SDLApp.h"
#include <vector>

class PixelGridSnappingGame : public Game
{
    SDL_Texture* pixelTexture;
    int pixelArtWidth;
    int pixelArtHeight;
    float scale;
    float time;
    
    struct PixelSprite {
        int x, y;
        int vx, vy;
        SDL_Color color;
    };
    
    std::vector<PixelSprite> sprites;

    void drawPixelArt() {
        // Create pixel art on CPU (heap-allocated: the size is runtime, and
        // MSVC rejects variable-length arrays)
        std::vector<Uint32> pixels(static_cast<size_t>(pixelArtWidth) * static_cast<size_t>(pixelArtHeight), 0x00000000);
        
        // Draw pixel art sprites
        for (const auto& sprite : sprites) {
            for (int dy = 0; dy < 8; dy++) {
                for (int dx = 0; dx < 8; dx++) {
                    int px = sprite.x + dx;
                    int py = sprite.y + dy;
                    if (px >= 0 && px < pixelArtWidth && py >= 0 && py < pixelArtHeight) {
                        // Create a simple 8x8 pixel sprite
                        Uint32 color = (0xFFu << 24) | (static_cast<Uint32>(sprite.color.r) << 16) | (static_cast<Uint32>(sprite.color.g) << 8) | static_cast<Uint32>(sprite.color.b);
                        pixels[static_cast<size_t>(py) * static_cast<size_t>(pixelArtWidth) + static_cast<size_t>(px)] = color;
                    }
                }
            }
        }
        
        // Update texture
        SDL_UpdateTexture(pixelTexture, nullptr, pixels.data(), pixelArtWidth * static_cast<int>(sizeof(Uint32)));
    }

public:
    PixelGridSnappingGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {
        pixelArtWidth = 64;
        pixelArtHeight = 64;
        scale = 10.0f;
        time = 0;
        pixelTexture = nullptr;
    }

    void onStart() override {
        Renderer* renderer = getRenderer();
        
        // Create a render-target texture for pixel-perfect rendering
        pixelTexture = SDL_CreateTexture(
            renderer->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            pixelArtWidth,
            pixelArtHeight
        );
        
        // Initialize sprites
        sprites = {
            {10, 10, 1, 1, {255, 0, 0, 255}},
            {30, 20, -1, 1, {0, 255, 0, 255}},
            {50, 40, 1, -1, {0, 0, 255, 255}},
            {20, 50, -1, -1, {255, 255, 0, 255}}
        };
    }

    void update() override {
        time += 0.016f;
        
        // Update sprite positions
        for (auto& sprite : sprites) {
            sprite.x += sprite.vx;
            sprite.y += sprite.vy;
            
            // Bounce off walls
            if (sprite.x <= 0 || sprite.x >= pixelArtWidth - 8) {
                sprite.vx = -sprite.vx;
                sprite.x = std::max(0, std::min(sprite.x, pixelArtWidth - 8));
            }
            if (sprite.y <= 0 || sprite.y >= pixelArtHeight - 8) {
                sprite.vy = -sprite.vy;
                sprite.y = std::max(0, std::min(sprite.y, pixelArtHeight - 8));
            }
        }
        
        // Draw pixel art to texture
        drawPixelArt();
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(20, 20, 30, 255);
        
        // Get renderer dimensions
        int w, h;
        SDL_GetRendererOutputSize(renderer->renderer, &w, &h);
        
        // Render pixel texture scaled up
        SDL_Rect destRect = {
            (int)(w / 2.0f - (pixelArtWidth * scale) / 2),
            (int)(h / 2.0f - (pixelArtHeight * scale) / 2),
            (int)(pixelArtWidth * scale),
            (int)(pixelArtHeight * scale)
        };
        
        // Disable linear filtering for crisp pixels
        // Note: SDL_SetTextureScaleMode may not be available in all SDL2 versions
        // SDL_SetTextureScaleMode(pixelTexture, SDL_SCALEMODE_NEAREST);
        SDL_RenderCopy(renderer->renderer, pixelTexture, nullptr, &destRect);
        
        // Draw grid overlay
        SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 50);
        for (int i = 0; i <= pixelArtWidth; i++) {
            float x = destRect.x + i * scale;
            SDL_RenderDrawLine(renderer->renderer, (int)x, destRect.y, (int)x, destRect.y + destRect.h);
        }
        for (int i = 0; i <= pixelArtHeight; i++) {
            float y = destRect.y + i * scale;
            SDL_RenderDrawLine(renderer->renderer, destRect.x, (int)y, destRect.x + destRect.w, (int)y);
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        wchar_t info[256];
        swprintf(info, 256, 
            L"Pixel Grid Snapping - %.0fx%.0f pixel art scaled %.0fx\n"
            L"Nearest-neighbor filtering for crisp pixels",
            (float)pixelArtWidth, (float)pixelArtHeight, scale);
        Rect<float> textBounds(10, 10, 500, 50);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }

    ~PixelGridSnappingGame() override {
        if (pixelTexture) {
            SDL_DestroyTexture(pixelTexture);
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    PixelGridSnappingGame app("Umbra Pixel Grid Snapping", 800, 600);
    app.run();
    return 0;
}
