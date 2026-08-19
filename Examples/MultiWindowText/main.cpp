// Multi-Window Text Showcase for Umbra Engine
// Renders text to multiple windows with different scale factors using both Text and Text2d
#include "Engine/Core/SDLApp.h"
#include <vector>

// Simple window wrapper for multi-window support
class GameWindow {
public:
    SDL_Window* window;
    SDL_Renderer* renderer;
    TextWriter* textWriter;
    float scale;
    int width, height;
    const char* title;
    
    GameWindow() : window(nullptr), renderer(nullptr), textWriter(nullptr), scale(1.0f), width(400), height(300) {}
    
    bool create(const char* _title, int x, int y, int w, int h, float _scale) {
        title = _title;
        width = w;
        height = h;
        scale = _scale;
        
        window = SDL_CreateWindow(title, x, y, w, h, SDL_WINDOW_SHOWN);
        if (!window) return false;
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) return false;
        
        textWriter = new TextWriter(renderer);
        return true;
    }
    
    void clear(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        SDL_RenderClear(renderer);
    }
    
    void present() {
        SDL_RenderPresent(renderer);
    }
    
    void destroy() {
        delete textWriter;
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
    }
    
    void drawText(const std::wstring& text, float x, float y, [[maybe_unused]] float size) {
        Rect<float> bounds(x, y, 300 * scale, 30 * scale);
        textWriter->drawTextToRenderer(text, renderer, bounds, "/default.ttf");
    }
};

class MultiWindowTextGame : public Game
{
    std::vector<GameWindow> windows;
    float time;

public:
    MultiWindowTextGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {
        time = 0;
    }

    void onStart() override {
        // Create multiple windows with different scale factors
        GameWindow w1;
        w1.create("Window 1 (Scale: 1.0)", 100, 100, 400, 300, 1.0f);
        windows.push_back(w1);
        
        GameWindow w2;
        w2.create("Window 2 (Scale: 1.5)", 550, 100, 600, 450, 1.5f);
        windows.push_back(w2);
        
        GameWindow w3;
        w3.create("Window 3 (Scale: 2.0)", 100, 450, 800, 600, 2.0f);
        windows.push_back(w3);
    }

    void update() override {
        time += 0.016f;
        
        // Render to each window
        for (size_t i = 0; i < windows.size(); i++) {
            GameWindow& win = windows[i];
            
            // Clear with different colors
            win.clear(
                (Uint8)(30 + i * 20), 
                (Uint8)(30 + i * 10), 
                (Uint8)(50 + i * 15), 
                255
            );
            
            // Draw text with different scales
            wchar_t titleText[128];
            swprintf(titleText, 128, L"Window %zu - Scale: %.1f", i + 1, win.scale);
            win.drawText(titleText, 10, 10, 24 * win.scale);
            
            wchar_t timeText[128];
            swprintf(timeText, 128, L"Time: %.2f", time);
            win.drawText(timeText, 10, 50 * win.scale, 18 * win.scale);
            
            wchar_t infoText[256];
            swprintf(infoText, 256, 
                L"Scale Factor: %.1fx\n"
                L"Window Size: %dx%d\n"
                L"Text scales with window",
                win.scale, win.width, win.height);
            win.drawText(infoText, 10, 90 * win.scale, 14 * win.scale);
            
            win.present();
        }
        
        // Also render to main window
        Renderer* renderer = getRenderer();
        renderer->clearScreen(20, 20, 30, 255);
        
        std::wstring mainText = L"Main Window - Check other windows!";
        Rect<float> bounds(10, 10, 500, 50);
        renderer->getTextWriter()->drawTextToRenderer(mainText, renderer->renderer, bounds, "/default.ttf");
    }

    void renderPostFX() override {
        // Info on main window
        Renderer* renderer = getRenderer();
        std::wstring info = L"Multi-Window Text - Multiple windows with different scale factors";
        Rect<float> textBounds(10, 50, 600, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
    
    ~MultiWindowTextGame() override {
        for (auto& win : windows) {
            win.destroy();
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    MultiWindowTextGame app("Umbra Multi-Window Text", 800, 600);
    app.run();
    return 0;
}
