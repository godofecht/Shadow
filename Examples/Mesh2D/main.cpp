// Mesh 2D Showcase for Umbra Engine
// Renders a 2D mesh
#include "Engine/Core/SDLApp.h"
#include <vector>

class Mesh2DGame : public Game
{
    struct Mesh {
        std::vector<float> vertices; // x, y pairs
        std::vector<int> indices;
        float x, y;
        SDL_Color color;
    };
    
    std::vector<Mesh> meshes;
    float time;

public:
    Mesh2DGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {
        time = 0;
    }

    void onStart() override {
        // Create a simple triangle mesh
        Mesh triangle;
        triangle.x = 200;
        triangle.y = 200;
        triangle.color = {255, 100, 100, 255};
        triangle.vertices = {0, -60, 50, 40, -50, 40};
        triangle.indices = {0, 1, 2};
        meshes.push_back(triangle);
        
        // Create a quad mesh
        Mesh quad;
        quad.x = 400;
        quad.y = 200;
        quad.color = {100, 255, 100, 255};
        quad.vertices = {-50, -50, 50, -50, 50, 50, -50, 50};
        quad.indices = {0, 1, 2, 2, 3, 0};
        meshes.push_back(quad);
        
        // Create a more complex mesh (house shape)
        Mesh house;
        house.x = 600;
        house.y = 200;
        house.color = {100, 100, 255, 255};
        house.vertices = {
            0, -60,   // roof peak
            -50, -20, // left roof
            50, -20,  // right roof
            -50, 40,  // bottom left
            50, 40    // bottom right
        };
        house.indices = {
            0, 1, 2,  // roof
            1, 3, 4,  // left wall
            1, 4, 2   // right wall
        };
        meshes.push_back(house);
        
        // Create a circle approximation mesh
        Mesh circle;
        circle.x = 300;
        circle.y = 400;
        circle.color = {255, 255, 100, 255};
        float radius = 60;
        int segments = 16;
        
        // Center vertex
        circle.vertices.push_back(0);
        circle.vertices.push_back(0);
        
        // Perimeter vertices
        for (int i = 0; i < segments; i++) {
            float angle = (3.14159f * 2.0f / segments) * i;
            circle.vertices.push_back(cos(angle) * radius);
            circle.vertices.push_back(sin(angle) * radius);
        }
        
        // Triangle fan indices
        for (int i = 0; i < segments; i++) {
            circle.indices.push_back(0);
            circle.indices.push_back(i + 1);
            circle.indices.push_back(i + 2);
        }
        meshes.push_back(circle);
        
        // Create a diamond mesh
        Mesh diamond;
        diamond.x = 500;
        diamond.y = 400;
        diamond.color = {255, 100, 255, 255};
        diamond.vertices = {0, -70, 50, 0, 0, 70, -50, 0};
        diamond.indices = {0, 1, 2, 0, 2, 3};
        meshes.push_back(diamond);
    }

    void update() override {
        time += 0.016f;
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(25, 25, 35, 255);
        
        // Render all meshes
        for (const auto& mesh : meshes) {
            SDL_SetRenderDrawColor(renderer->renderer, 
                mesh.color.r, mesh.color.g, mesh.color.b, mesh.color.a);
            
            // Draw triangles
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                const int i0 = mesh.indices[i];
                const int i1 = mesh.indices[i + 1];
                const int i2 = mesh.indices[i + 2];
                const std::size_t v0 = static_cast<std::size_t>(i0) * 2;
                const std::size_t v1 = static_cast<std::size_t>(i1) * 2;
                const std::size_t v2 = static_cast<std::size_t>(i2) * 2;
                
                float x0 = mesh.x + mesh.vertices[v0];
                float y0 = mesh.y + mesh.vertices[v0 + 1];
                float x1 = mesh.x + mesh.vertices[v1];
                float y1 = mesh.y + mesh.vertices[v1 + 1];
                float x2 = mesh.x + mesh.vertices[v2];
                float y2 = mesh.y + mesh.vertices[v2 + 1];
                
                // Draw triangle outline
                SDL_RenderDrawLine(renderer->renderer, (int)x0, (int)y0, (int)x1, (int)y1);
                SDL_RenderDrawLine(renderer->renderer, (int)x1, (int)y1, (int)x2, (int)y2);
                SDL_RenderDrawLine(renderer->renderer, (int)x2, (int)y2, (int)x0, (int)y0);
            }
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        std::wstring info = L"Mesh 2D - Basic 2D mesh rendering with various shapes";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Mesh2DGame app("Umbra Mesh 2D", 800, 600);
    app.run();
    return 0;
}
