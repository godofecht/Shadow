// Procedural 3D Rendering Example for BrainRot Engine (Optimized)
#include "Engine/Core/SDLApp.h"
#include <vector>
#include <cmath>

struct Edge {
    int v1, v2;
};

class Procedural3DGame : public Game
{
public:
    Procedural3DGame(const char* title, int width, int height) : Game(title, width, height) {}

    void onStart() override {
        // Define a cube
        vertices = {
            {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
            {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}
        };

        edges = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Back face
            {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Front face
            {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connections
        };

        // Reuse projection buffer
        projected.resize(vertices.size());

        // Standard projection
        float fov = 90.0f * (3.14159f / 180.0f);
        float aspect = 1.0f;
        projectionMat = Matrix4x4::projection(fov, aspect, 0.1f, 100.0f);
    }

    void update() override {
        Uint32 ticks = SDL_GetTicks();
        float time = ticks * 0.001f;

        // Precompute combined transformation matrix once per frame
        Matrix4x4 rotX = Matrix4x4::rotationX(time);
        Matrix4x4 rotY = Matrix4x4::rotationY(time * 1.5f);
        Matrix4x4 rotZ = Matrix4x4::rotationZ(time * 0.5f);
        Matrix4x4 trans = Matrix4x4::translation(0, 0, -4.0f); // Move back

        // Matrix multiplication is associative: (Trans * (RotZ * (RotY * RotX)))
        Matrix4x4 world = trans * rotZ * rotY * rotX;
        Matrix4x4 mvp = projectionMat * world;

        Renderer* renderer = getRenderer();
        renderer->clearScreen(10, 20, 40, 255);

        // Transform all vertices
        for (size_t i = 0; i < vertices.size(); ++i) {
            Vector3 v = mvp.multiplyVector(vertices[i]);
            
            // Map to screen space
            projected[i].x = static_cast<int>(v.x * 350 + 350);
            projected[i].y = static_cast<int>(v.y * 350 + 350);
        }

        // Draw edges
        for (const auto& e : edges) {
            const std::size_t v1 = static_cast<std::size_t>(e.v1);
            const std::size_t v2 = static_cast<std::size_t>(e.v2);
            renderer->drawLine(projected[v1].x, projected[v1].y, 
                               projected[v2].x, projected[v2].y);
        }
    }

private:
    std::vector<Vector3> vertices;
    std::vector<Edge> edges;
    std::vector<SDL_Point> projected;
    Matrix4x4 projectionMat;
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static Procedural3DGame app("Procedural 3D Optimized - BrainRot Engine", 700, 700);
#else
    Procedural3DGame app("Procedural 3D Optimized - BrainRot Engine", 700, 700);
#endif
    app.run();
    return 0;
}
