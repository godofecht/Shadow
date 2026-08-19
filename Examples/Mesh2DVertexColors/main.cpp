// Mesh 2D With Vertex Colors Showcase for Umbra Engine
// Renders a 2D mesh with vertex color attributes
#include "Engine/Core/SDLApp.h"
#include <vector>

class Mesh2DVertexColorsGame : public Game
{
    struct Vertex {
        float x, y;
        Uint8 r, g, b, a;
    };
    
    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<int> indices;
        float x, y;
    };
    
    Mesh colorMesh;
    float rotation;

public:
    Mesh2DVertexColorsGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {
        rotation = 0;
    }

    void onStart() override {
        colorMesh.x = 400;
        colorMesh.y = 300;
        
        // Create a hexagon with different vertex colors
        float radius = 120;
        int vertices = 6;
        
        for (int i = 0; i < vertices; i++) {
            Vertex v;
            float angle = (3.14159f * 2.0f / vertices) * i - 3.14159f / 2.0f;
            
            v.x = cos(angle) * radius;
            v.y = sin(angle) * radius;
            
            // Rainbow colors around the hexagon
            v.r = (Uint8)(255 * (0.5f + 0.5f * cos(i * 1.0f)));
            v.g = (Uint8)(255 * (0.5f + 0.5f * cos(i * 1.0f + 2.0f)));
            v.b = (Uint8)(255 * (0.5f + 0.5f * cos(i * 1.0f + 4.0f)));
            v.a = 255;
            
            colorMesh.vertices.push_back(v);
        }
        
        // Center vertex with white color
        Vertex center;
        center.x = 0; center.y = 0;
        center.r = 255; center.g = 255; center.b = 255; center.a = 255;
        colorMesh.vertices.push_back(center);
        int centerIndex = static_cast<int>(colorMesh.vertices.size()) - 1;
        
        // Create triangle fan
        for (size_t i = 0; i < colorMesh.vertices.size() - 2; i++) {
            colorMesh.indices.push_back(centerIndex);
            colorMesh.indices.push_back(static_cast<int>(i));
            colorMesh.indices.push_back(static_cast<int>(i + 1));
        }
    }

    void update() override {
        rotation += 0.3f * 0.016f;
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(20, 20, 30, 255);
        
        float cosR = cos(rotation);
        float sinR = sin(rotation);
        
        // Render mesh with vertex color interpolation
        SDL_SetRenderDrawBlendMode(renderer->renderer, SDL_BLENDMODE_BLEND);
        
        for (size_t i = 0; i < colorMesh.indices.size(); i += 3) {
            const Vertex& v0 = colorMesh.vertices[static_cast<std::size_t>(colorMesh.indices[i])];
            const Vertex& v1 = colorMesh.vertices[static_cast<std::size_t>(colorMesh.indices[i + 1])];
            const Vertex& v2 = colorMesh.vertices[static_cast<std::size_t>(colorMesh.indices[i + 2])];
            
            // Transform vertices
            float x0 = colorMesh.x + v0.x * cosR - v0.y * sinR;
            float y0 = colorMesh.y + v0.x * sinR + v0.y * cosR;
            float x1 = colorMesh.x + v1.x * cosR - v1.y * sinR;
            float y1 = colorMesh.y + v1.x * sinR + v1.y * cosR;
            float x2 = colorMesh.x + v2.x * cosR - v2.y * sinR;
            float y2 = colorMesh.y + v2.x * sinR + v2.y * cosR;
            
            // Interpolate colors for each edge
            // Draw edge 0-1 with gradient
            int steps = 20;
            for (int s = 0; s < steps; s++) {
                float t1 = s / (float)steps;
                float t2 = (s + 1) / (float)steps;
                
                // Edge 0-1
                Uint8 r1 = (Uint8)(v0.r + (v1.r - v0.r) * t1);
                Uint8 g1 = (Uint8)(v0.g + (v1.g - v0.g) * t1);
                Uint8 b1 = (Uint8)(v0.b + (v1.b - v0.b) * t1);
                
                float ex1 = x0 + (x1 - x0) * t1;
                float ey1 = y0 + (y1 - y0) * t1;
                float ex2 = x0 + (x1 - x0) * t2;
                float ey2 = y0 + (y1 - y0) * t2;
                
                SDL_SetRenderDrawColor(renderer->renderer, r1, g1, b1, 255);
                SDL_RenderDrawLine(renderer->renderer, (int)ex1, (int)ey1, (int)ex2, (int)ey2);
                
                // Edge 1-2
                r1 = (Uint8)(v1.r + (v2.r - v1.r) * t1);
                g1 = (Uint8)(v1.g + (v2.g - v1.g) * t1);
                b1 = (Uint8)(v1.b + (v2.b - v1.b) * t1);
                
                ex1 = x1 + (x2 - x1) * t1;
                ey1 = y1 + (y2 - y1) * t1;
                ex2 = x1 + (x2 - x1) * t2;
                ey2 = y1 + (y2 - y1) * t2;
                
                SDL_SetRenderDrawColor(renderer->renderer, r1, g1, b1, 255);
                SDL_RenderDrawLine(renderer->renderer, (int)ex1, (int)ey1, (int)ex2, (int)ey2);
                
                // Edge 2-0
                r1 = (Uint8)(v2.r + (v0.r - v2.r) * t1);
                g1 = (Uint8)(v2.g + (v0.g - v2.g) * t1);
                b1 = (Uint8)(v2.b + (v0.b - v2.b) * t1);
                
                ex1 = x2 + (x0 - x2) * t1;
                ey1 = y2 + (y0 - y2) * t1;
                ex2 = x2 + (x0 - x2) * t2;
                ey2 = y2 + (y0 - y2) * t2;
                
                SDL_SetRenderDrawColor(renderer->renderer, r1, g1, b1, 255);
                SDL_RenderDrawLine(renderer->renderer, (int)ex1, (int)ey1, (int)ex2, (int)ey2);
            }
        }
        
        // Draw vertices
        for (const auto& v : colorMesh.vertices) {
            float vx = colorMesh.x + v.x * cosR - v.y * sinR;
            float vy = colorMesh.y + v.x * sinR + v.y * cosR;
            SDL_SetRenderDrawColor(renderer->renderer, v.r, v.g, v.b, 255);
            SDL_Rect pt = {(int)vx - 5, (int)vy - 5, 10, 10};
            SDL_RenderFillRect(renderer->renderer, &pt);
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        std::wstring info = L"Mesh 2D With Vertex Colors - Color interpolation across mesh";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Mesh2DVertexColorsGame app("Umbra Mesh 2D Vertex Colors", 800, 600);
    app.run();
    return 0;
}
