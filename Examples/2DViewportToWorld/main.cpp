// 2D Viewport To World Showcase for Umbra Engine
// Demonstrates how to use the Camera::viewport_to_world_2d method with a dynamic viewport and camera.
#include "Engine/Core/Game2D.h"
#include <cmath>

class Camera2D {
public:
    float x, y;           // Camera position
    float zoom;           // Camera zoom level
    float viewportWidth;  // Viewport width in screen space
    float viewportHeight; // Viewport height in screen space
    float screenWidth;    // Screen width
    float screenHeight;   // Screen height
    
    Camera2D(float screenWidth, float screenHeight) 
        : x(0), y(0), zoom(1.0f), 
          viewportWidth(screenWidth), viewportHeight(screenHeight),
          screenWidth(screenWidth), screenHeight(screenHeight) {}
    
    // Convert viewport (screen) coordinates to world coordinates
    Point2D viewportToWorld(float screenX, float screenY) const {
        float worldX = (screenX - screenWidth / 2.0f) / zoom + x;
        float worldY = (screenY - screenHeight / 2.0f) / zoom + y;
        return Point2D(worldX, worldY);
    }
    
    // Convert world coordinates to viewport (screen) coordinates
    Point2D worldToViewport(float worldX, float worldY) const {
        float screenX = (worldX - x) * zoom + screenWidth / 2.0f;
        float screenY = (worldY - y) * zoom + screenHeight / 2.0f;
        return Point2D(screenX, screenY);
    }
    
    void setViewport(float w, float h) {
        viewportWidth = w;
        viewportHeight = h;
        screenWidth = w;
        screenHeight = h;
    }
};

class ViewportToWorldGame : public Game2D
{
    Camera2D* camera;
    std::vector<Point2D> worldPoints;
    Point2D mouseWorldPos;
    float dynamicViewportWidth;
    float dynamicViewportHeight;
    
public:
    ViewportToWorldGame() : Game2D("Umbra 2D Viewport To World", 800, 600, 20) {
        camera = nullptr;
        dynamicViewportWidth = 800;
        dynamicViewportHeight = 600;
    }

    void initGame() override {
        camera = new Camera2D(800, 600);
        
        // Create some world space points (a grid)
        for (int x = -500; x <= 500; x += 100) {
            for (int y = -500; y <= 500; y += 100) {
                worldPoints.emplace_back(x, y);
            }
        }
    }

    void updateGame(float dt) override {
        // Camera controls
        if (input.isKeyPressed(KEY_LEFT)) camera->x -= 200 * dt;
        if (input.isKeyPressed(KEY_RIGHT)) camera->x += 200 * dt;
        if (input.isKeyPressed(KEY_UP)) camera->y -= 200 * dt;
        if (input.isKeyPressed(KEY_DOWN)) camera->y += 200 * dt;
        if (input.isKeyPressed(KEY_SPACE)) camera->zoom *= 1.01f;
        if (input.isKeyPressed(KEY_R)) camera->zoom /= 1.01f;
        
        // Dynamic viewport controls
        if (input.isKeyPressed(KEY_W)) dynamicViewportHeight -= 100 * dt;
        if (input.isKeyPressed(KEY_S)) dynamicViewportHeight += 100 * dt;
        if (input.isKeyPressed(KEY_A)) dynamicViewportWidth -= 100 * dt;
        if (input.isKeyPressed(KEY_D)) dynamicViewportWidth += 100 * dt;
        
        // Clamp viewport
        dynamicViewportWidth = std::max(100.0f, std::min(dynamicViewportWidth, 1600.0f));
        dynamicViewportHeight = std::max(100.0f, std::min(dynamicViewportHeight, 1200.0f));
        
        camera->setViewport(dynamicViewportWidth, dynamicViewportHeight);
        
        // Get mouse position and convert to world space
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        mouseWorldPos = camera->viewportToWorld(mx, my);
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        // Draw viewport boundary
        SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 0, 255);
        SDL_Rect viewportRect = {0, 0, (int)dynamicViewportWidth, (int)dynamicViewportHeight};
        SDL_RenderDrawRect(renderer->renderer, &viewportRect);
        
        // Draw world grid points
        SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
        for (const auto& point : worldPoints) {
            Point2D screenPos = camera->worldToViewport(point.x, point.y);
            if (screenPos.x >= 0 && screenPos.x <= dynamicViewportWidth &&
                screenPos.y >= 0 && screenPos.y <= dynamicViewportHeight) {
                SDL_Rect pt = {(int)screenPos.x - 2, (int)screenPos.y - 2, 4, 4};
                SDL_RenderFillRect(renderer->renderer, &pt);
            }
        }
        
        // Draw mouse position indicator in world space
        SDL_SetRenderDrawColor(renderer->renderer, 255, 0, 0, 255);
        Point2D mouseScreen = camera->worldToViewport(mouseWorldPos.x, mouseWorldPos.y);
        SDL_Rect mouseRect = {(int)mouseScreen.x - 5, (int)mouseScreen.y - 5, 10, 10};
        SDL_RenderFillRect(renderer->renderer, &mouseRect);
        
        // Draw info text
        wchar_t info[256];
        swprintf(info, 256, 
            L"Camera: (%.1f, %.1f) Zoom: %.2f\n"
            L"Viewport: %.0f x %.0f\n"
            L"Mouse Screen: (%.1f, %.1f)\n"
            L"Mouse World: (%.1f, %.1f)\n"
            L"Controls: Arrow=Move, +/-=Zoom, WASD=Resize Viewport",
            camera->x, camera->y, camera->zoom,
            dynamicViewportWidth, dynamicViewportHeight,
            mouseScreen.x, mouseScreen.y,
            mouseWorldPos.x, mouseWorldPos.y);
        
        Rect<float> textBounds(10, 10, 500, 120);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    ViewportToWorldGame app;
    app.run();
    return 0;
}
