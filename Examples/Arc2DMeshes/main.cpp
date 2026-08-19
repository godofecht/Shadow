// Arc 2D Meshes Showcase for Umbra Engine
// Demonstrates UV-mapping of the circular segment and sector primitives
#include "Engine/Core/SDLApp.h"
#include <cmath>
#include <vector>

class Arc2DMeshesGame : public Game
{
    struct Sector {
        float x, y;
        float radius;
        float startAngle;
        float endAngle;
        SDL_Color color;
        std::vector<float> vertices; // x, y, u, v
        std::vector<int> indices;
    };
    
    std::vector<Sector> sectors;

    void createSector(Sector& sector, int segments = 32) {
        sector.vertices.clear();
        sector.indices.clear();
        
        float centerU = 0.5f;
        float centerV = 0.5f;
        
        // Center vertex
        sector.vertices.push_back(sector.x);
        sector.vertices.push_back(sector.y);
        sector.vertices.push_back(centerU);
        sector.vertices.push_back(centerV);
        
        float angleStep = (sector.endAngle - sector.startAngle) / segments;
        
        // Perimeter vertices
        for (int i = 0; i <= segments; i++) {
            float angle = sector.startAngle + i * angleStep;
            float px = sector.x + cos(angle) * sector.radius;
            float py = sector.y + sin(angle) * sector.radius;
            
            // UV coordinates mapped to circle
            float u = 0.5f + 0.5f * cos(angle);
            float v = 0.5f - 0.5f * sin(angle);
            
            sector.vertices.push_back(px);
            sector.vertices.push_back(py);
            sector.vertices.push_back(u);
            sector.vertices.push_back(v);
        }
        
        // Create triangle fan indices
        for (int i = 0; i < segments; i++) {
            sector.indices.push_back(0);
            sector.indices.push_back(i + 1);
            sector.indices.push_back(i + 2);
        }
    }

public:
    Arc2DMeshesGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {}

    void onStart() override {
        // Create various arc/sector shapes
        Sector sector1;
        sector1.x = 200; sector1.y = 200;
        sector1.radius = 80;
        sector1.startAngle = 0;
        sector1.endAngle = 3.14159f / 2.0f; // 90 degrees
        sector1.color = {255, 100, 100, 255};
        createSector(sector1);
        sectors.push_back(sector1);
        
        Sector sector2;
        sector2.x = 400; sector2.y = 200;
        sector2.radius = 80;
        sector2.startAngle = 0;
        sector2.endAngle = 3.14159f; // 180 degrees
        sector2.color = {100, 255, 100, 255};
        createSector(sector2);
        sectors.push_back(sector2);
        
        Sector sector3;
        sector3.x = 600; sector3.y = 200;
        sector3.radius = 80;
        sector3.startAngle = 3.14159f / 4.0f;
        sector3.endAngle = 7.0f * 3.14159f / 4.0f; // 270 degrees
        sector3.color = {100, 100, 255, 255};
        createSector(sector3);
        sectors.push_back(sector3);
        
        // Circular segment (arc without center)
        Sector segment1;
        segment1.x = 200; segment1.y = 400;
        segment1.radius = 80;
        segment1.startAngle = 0;
        segment1.endAngle = 3.14159f / 2.0f;
        segment1.color = {255, 255, 100, 255};
        createSector(segment1);
        sectors.push_back(segment1);
        
        Sector segment2;
        segment2.x = 400; segment2.y = 400;
        segment2.radius = 60;
        segment2.startAngle = 0;
        segment2.endAngle = 3.14159f * 1.5f;
        segment2.color = {255, 100, 255, 255};
        createSector(segment2);
        sectors.push_back(segment2);
        
        Sector segment3;
        segment3.x = 600; segment3.y = 400;
        segment3.radius = 70;
        segment3.startAngle = -3.14159f / 6.0f;
        segment3.endAngle = 3.14159f * 7.0f / 6.0f;
        segment3.color = {100, 255, 255, 255};
        createSector(segment3);
        sectors.push_back(segment3);
    }

    void update() override {
        Renderer* renderer = getRenderer();
        renderer->clearScreen(20, 20, 30, 255);

        // Render sectors
        for (const auto& sector : sectors) {
            SDL_SetRenderDrawColor(renderer->renderer,
                sector.color.r, sector.color.g, sector.color.b, sector.color.a);

            // Draw triangle fan
            if (sector.vertices.size() >= 12) {
                float cx = sector.vertices[0];
                float cy = sector.vertices[1];

                // Draw all triangles in the fan (including closing triangle)
                for (size_t i = 4; i < sector.vertices.size() - 4; i += 4) {
                    float x1 = sector.vertices[i];
                    float y1 = sector.vertices[i + 1];
                    float x2 = sector.vertices[i + 4];
                    float y2 = sector.vertices[i + 5];

                    // Draw triangle outline
                    SDL_RenderDrawLine(renderer->renderer, (int)cx, (int)cy, (int)x1, (int)y1);
                    SDL_RenderDrawLine(renderer->renderer, (int)x1, (int)y1, (int)x2, (int)y2);
                    SDL_RenderDrawLine(renderer->renderer, (int)x2, (int)y2, (int)cx, (int)cy);
                }
            }

            // Draw arc outline (including the closing segment)
            SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
            for (size_t i = 4; i < sector.vertices.size() - 4; i += 4) {
                float x1 = sector.vertices[i];
                float y1 = sector.vertices[i + 1];
                float x2 = sector.vertices[i + 4];
                float y2 = sector.vertices[i + 5];
                SDL_RenderDrawLine(renderer->renderer, (int)x1, (int)y1, (int)x2, (int)y2);
            }
            // Draw the closing line from last vertex back to first vertex
            if (sector.vertices.size() >= 8) {
                size_t firstIdx = 4;
                size_t lastIdx = sector.vertices.size() - 8;
                float x1 = sector.vertices[firstIdx];
                float y1 = sector.vertices[firstIdx + 1];
                float x2 = sector.vertices[lastIdx];
                float y2 = sector.vertices[lastIdx + 1];
                SDL_RenderDrawLine(renderer->renderer, (int)x1, (int)y1, (int)x2, (int)y2);
            }
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        std::wstring info = L"Arc 2D Meshes - UV-mapped Circular Segments and Sectors";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Arc2DMeshesGame app("Umbra Arc 2D Meshes", 800, 600);
    app.run();
    return 0;
}
