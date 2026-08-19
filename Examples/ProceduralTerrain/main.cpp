// Procedural 3D Terrain Example with Perlin Noise for BrainRot Engine
#include "Engine/Core/SDLApp.h"
#include "Engine/Core/Helpers.h"
#include <vector>
#include <cmath>

class Terrain3DGame : public Game
{
public:
    Terrain3DGame(const char* windowTitle, int width, int height) : Game(windowTitle, width, height) {}

    void onStart() override {
        gridWidth = 30;
        gridHeight = 30;
        spacing = 0.2f;
        
        generateTerrain();

        // Standard projection
        float fov = 60.0f * (3.14159f / 180.0f);
        float aspect = 700.0f / 700.0f;
        projectionMat = Matrix4x4::projection(fov, aspect, 0.1f, 100.0f);
        
        angle = 0;
    }

    void generateTerrain() {
        vertices.clear();
        float time = SDL_GetTicks() * 0.0005f;
        
        for (int y = 0; y < gridHeight; ++y) {
            for (int x = 0; x < gridWidth; ++x) {
                float nx = static_cast<float>(x) * 0.1f;
                float ny = static_cast<float>(y) * 0.1f;
                // Add time for a "flowing" effect
                float h = PerlinNoiseGenerator::perlinNoise(nx, ny + time, 3) * 1.5f;
                
                vertices.emplace_back(
                    (x - gridWidth / 2.0f) * spacing,
                    h,
                    (y - gridHeight / 2.0f) * spacing
                );
            }
        }
        projected.resize(vertices.size());
    }

    void update() override {
        // Regenerate for animation effect
        generateTerrain();

        angle += 0.01f;

        // Matrices
        Matrix4x4 rotY = Matrix4x4::rotationY(angle * 0.5f);
        Matrix4x4 rotX = Matrix4x4::rotationX(0.5f); // Tilt down to see the terrain
        Matrix4x4 trans = Matrix4x4::translation(0, -1.0f, -6.0f);
        
        Matrix4x4 world = trans * rotX * rotY;
        Matrix4x4 mvp = projectionMat * world;

        Renderer* renderer = getRenderer();
        renderer->clearScreen(10, 15, 30, 255);

        // Transform
        for (size_t i = 0; i < vertices.size(); ++i) {
            Vector3 v = mvp.multiplyVector(vertices[i]);
            projected[i].x = static_cast<int>(v.x * 400 + 350);
            projected[i].y = static_cast<int>(v.y * 400 + 350);
        }

        // Draw grid lines
        for (int y = 0; y < gridHeight; ++y) {
            for (int x = 0; x < gridWidth; ++x) {
                const std::size_t idx = static_cast<std::size_t>(y) * static_cast<std::size_t>(gridWidth) + static_cast<std::size_t>(x);
                
                // Draw line to right
                if (x < gridWidth - 1) {
                    renderer->drawLine(projected[idx].x, projected[idx].y, 
                                       projected[idx + 1].x, projected[idx + 1].y);
                }
                
                // Draw line down
                if (y < gridHeight - 1) {
                    renderer->drawLine(projected[idx].x, projected[idx].y, 
                                       projected[idx + static_cast<std::size_t>(gridWidth)].x, projected[idx + static_cast<std::size_t>(gridWidth)].y);
                }
            }
        }
    }

private:
    int gridWidth, gridHeight;
    float spacing;
    float angle;
    std::vector<Vector3> vertices;
    std::vector<SDL_Point> projected;
    Matrix4x4 projectionMat;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static Terrain3DGame app("Procedural 3D Terrain - BrainRot Engine", 700, 700);
#else
    Terrain3DGame app("Procedural 3D Terrain - BrainRot Engine", 700, 700);
#endif
    app.run();
    return 0;
}
