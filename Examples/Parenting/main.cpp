// Parenting Example for Umbra Engine
// Demonstrates hierarchical transforms (parent-child relationships)
#include "Engine/Core/SDLApp.h"
#include <vector>
#include <cmath>
#include <memory>

// Simple transform component
struct Transform {
    float x, y;
    float rotation; // in radians
    float scale;
    Transform* parent = nullptr;

    // Get world position by traversing up the hierarchy
    void getWorldTransform(float& worldX, float& worldY, float& worldRot, float& worldScale) {
        if (parent) {
            float px, py, prot, pscale;
            parent->getWorldTransform(px, py, prot, pscale);
            
            // Apply parent's transform
            worldScale = pscale * scale;
            worldRot = prot + rotation;
            
            // Rotate local position by parent's rotation
            float localX = x * pscale;
            float localY = y * pscale;
            worldX = px + (localX * cos(prot) - localY * sin(prot));
            worldY = py + (localX * sin(prot) + localY * cos(prot));
        } else {
            worldX = x;
            worldY = y;
            worldRot = rotation;
            worldScale = scale;
        }
    }
};

struct Entity {
    Transform transform;
    SDL_Color color;
    float size = 20.0f;
    std::vector<std::shared_ptr<Entity>> children;

    void addChild(std::shared_ptr<Entity> child) {
        child->transform.parent = &this->transform;
        children.push_back(std::move(child));
    }

    void render(Renderer* renderer) {
        float wx, wy, wrot, wscale;
        transform.getWorldTransform(wx, wy, wrot, wscale);

        SDL_SetRenderDrawColor(renderer->renderer, color.r, color.g, color.b, color.a);
        
        // Draw a simple rotated rect
        float s = size * wscale;
        
        // Manual rotation for rendering (SDL_RenderCopyEx equivalent for shapes)
        // For this demo, we'll draw lines connecting parent to child to visualize hierarchy
        
        // Draw the entity itself (as a small circle/rect)
        SDL_Rect r = { static_cast<int>(wx - s/2), static_cast<int>(wy - s/2), static_cast<int>(s), static_cast<int>(s) };
        SDL_RenderFillRect(renderer->renderer, &r);

        // Render children
        for (auto& child : children) {
            float cx, cy, crot, cscale;
            child->transform.getWorldTransform(cx, cy, crot, cscale);
            
            // Draw line to child
            SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
            SDL_RenderDrawLine(renderer->renderer, static_cast<int>(wx), static_cast<int>(wy), static_cast<int>(cx), static_cast<int>(cy));
            
            child->render(renderer);
        }
    }
};

class ParentingGame : public Game
{
public:
    ParentingGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {}

    void onStart() override {
        // Create root (Sun)
        root = std::make_shared<Entity>();
        root->transform = {350, 350, 0, 1.0f, nullptr};
        root->color = {255, 200, 50, 255}; // Yellow Sun
        root->size = 40.0f;

        // Create child (Earth)
        auto earth = std::make_shared<Entity>();
        earth->transform = {150, 0, 0, 0.8f, nullptr}; // Local pos relative to Sun
        earth->color = {50, 100, 255, 255}; // Blue Earth
        earth->size = 20.0f;
        root->addChild(earth);

        // Create grandchild (Moon)
        auto moon = std::make_shared<Entity>();
        moon->transform = {40, 0, 0, 0.5f, nullptr}; // Local pos relative to Earth
        moon->color = {200, 200, 200, 255}; // Grey Moon
        moon->size = 10.0f;
        earth->addChild(moon);
        
        // Create another grandchild (Satellite)
        auto sat = std::make_shared<Entity>();
        sat->transform = {0, 25, 0, 0.3f, nullptr}; // Local pos relative to Earth
        sat->color = {255, 50, 50, 255}; // Red Satellite
        sat->size = 8.0f;
        earth->addChild(sat);
    }

    void update() override {
        // Rotate Sun (affects everything)
        root->transform.rotation += 0.01f;

        // Rotate Earth around Sun (by changing Earth's local position?? No, parent rotation handles it!)
        // Actually, if we just rotate the parent, the child orbits it IF the child has an offset.
        // But to make Earth spin on its own axis, we change earth->rotation.
        
        if (!root->children.empty()) {
            auto earth = root->children[0];
            earth->transform.rotation += 0.03f; // Earth spins faster
            
            // Moon orbits Earth (automatically handled by hierarchy if Earth rotates? 
            // Yes, if Earth rotates, Moon orbits. But if we want Moon to rotate too...)
             if (!earth->children.empty()) {
                 earth->children[0]->transform.rotation -= 0.05f; // Moon spins opposite
             }
        }

        Renderer* renderer = getRenderer();
        renderer->clearScreen(20, 20, 30, 255);
        
        root->render(renderer);
    }

private:
    std::shared_ptr<Entity> root;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static ParentingGame app("Umbra Parenting Example", 700, 700);
#else
    ParentingGame app("Umbra Parenting Example", 700, 700);
#endif
    app.run();
    return 0;
}
