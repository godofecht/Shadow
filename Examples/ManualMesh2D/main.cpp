// Manual Mesh 2D Showcase for Umbra Engine
// Renders a custom mesh 'manually' with 'mid-level' renderer APIs
#include "Engine/Core/SDLApp.h"
#include <vector>

class ManualMesh2DGame : public Game
{
    struct Vertex {
        float x, y;      // Position
        float u, v;      // UV coordinates
        Uint8 r, g, b, a; // Vertex color
    };
    
    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<int> indices;
        float x, y;
        float rotation;
    };
    
    Mesh customMesh;
    float time;

public:
    ManualMesh2DGame(const char* title, int width, int height) : Game(title, width, height) {
        time = 0;
    }

    void onStart() override {
        // Create a custom star-shaped mesh
        customMesh.x = 400;
        customMesh.y = 300;
        customMesh.rotation = 0;
        
        float outerRadius = 100;
        float innerRadius = 50;
        int points = 5;
        
        // Create star vertices
        for (int i = 0; i < points * 2; i++) {
            Vertex v;
            float angle = (3.14159f * 2.0f / (points * 2)) * i - 3.14159f / 2.0f;
            float radius = (i % 2 == 0) ? outerRadius : innerRadius;
            
            v.x = cos(angle) * radius;
            v.y = sin(angle) * radius;
            
            // UV coordinates
            v.u = 0.5f + v.x / (outerRadius * 2);
            v.v = 0.5f - v.y / (outerRadius * 2);
            
            // Vertex colors (gradient)
            v.r = 255;
            v.g = (Uint8)(128 + 127 * sin(i * 0.5f));
            v.b = (Uint8)(128 + 127 * cos(i * 0.5f));
            v.a = 255;
            
            customMesh.vertices.push_back(v);
        }
        
        // Center vertex
        Vertex center;
        center.x = 0; center.y = 0;
        center.u = 0.5f; center.v = 0.5f;
        center.r = 255; center.g = 255; center.b = 255; center.a = 255;
        customMesh.vertices.push_back(center);
        int centerIndex = static_cast<int>(customMesh.vertices.size()) - 1;
        
        // Create triangle fan indices
        for (size_t i = 0; i < customMesh.vertices.size() - 2; i++) {
            customMesh.indices.push_back(centerIndex);
            customMesh.indices.push_back(static_cast<int>(i));
            customMesh.indices.push_back(static_cast<int>(i + 1));
        }
    }

    void update() override {
        time += 0.016f;
        customMesh.rotation += 0.5f * 0.016f;
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(20, 20, 35, 255);
        
        // Render the custom mesh manually
        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_BLEND);
        
        float cosR = cos(customMesh.rotation);
        float sinR = sin(customMesh.rotation);
        
        // Draw triangles
        for (size_t i = 0; i < customMesh.indices.size(); i += 3) {
            const Vertex& v0 = customMesh.vertices[static_cast<std::size_t>(customMesh.indices[i])];
            const Vertex& v1 = customMesh.vertices[static_cast<std::size_t>(customMesh.indices[i + 1])];
            const Vertex& v2 = customMesh.vertices[static_cast<std::size_t>(customMesh.indices[i + 2])];
            
            // Apply rotation and translation
            float x0 = customMesh.x + v0.x * cosR - v0.y * sinR;
            float y0 = customMesh.y + v0.x * sinR + v0.y * cosR;
            float x1 = customMesh.x + v1.x * cosR - v1.y * sinR;
            float y1 = customMesh.y + v1.x * sinR + v1.y * cosR;
            float x2 = customMesh.x + v2.x * cosR - v2.y * sinR;
            float y2 = customMesh.y + v2.x * sinR + v2.y * cosR;
            
            // Set color (use average of vertex colors)
            Uint8 avgR = (v0.r + v1.r + v2.r) / 3;
            Uint8 avgG = (v0.g + v1.g + v2.g) / 3;
            Uint8 avgB = (v0.b + v1.b + v2.b) / 3;
            Uint8 avgA = (v0.a + v1.a + v2.a) / 3;
            
            SDL_SetRenderDrawColor(renderer->renderer, avgR, avgG, avgB, avgA);
            
            // Draw triangle outline
            SDL_RenderDrawLine(renderer->renderer, (int)x0, (int)y0, (int)x1, (int)y1);
            SDL_RenderDrawLine(renderer->renderer, (int)x1, (int)y1, (int)x2, (int)y2);
            SDL_RenderDrawLine(renderer->renderer, (int)x2, (int)y2, (int)x0, (int)y0);
        }
        
        // Draw vertex points
        SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
        for (const auto& v : customMesh.vertices) {
            float vx = customMesh.x + v.x * cosR - v.y * sinR;
            float vy = customMesh.y + v.x * sinR + v.y * cosR;
            SDL_Rect pt = {(int)vx - 3, (int)vy - 3, 6, 6};
            SDL_RenderFillRect(renderer->renderer, &pt);
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        std::wstring info = L"Manual Mesh 2D - Custom mesh with mid-level renderer APIs";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    ManualMesh2DGame app("Umbra Manual Mesh 2D", 800, 600);
    app.run();
    return 0;
}
