// Mesh2D Alpha Mode Showcase for Umbra Engine
// Used to test alpha modes with mesh2d
#include "Engine/Core/SDLApp.h"
#include <vector>

class Mesh2DAlphaGame : public Game
{
    struct MeshVertex {
        float x, y;
        float u, v;
    };
    
    struct Mesh {
        std::vector<MeshVertex> vertices;
        std::vector<int> indices;
        SDL_Color color;
        float x, y;
        std::string name;
    };
    
    std::vector<Mesh> meshes;
    SDL_BlendMode currentBlendMode;
    int selectedMode;

public:
    Mesh2DAlphaGame(const char* title, int width, int height) : Game(title, width, height) {
        currentBlendMode = SDL_BLENDMODE_BLEND;
        selectedMode = 0;
    }

    void onStart() override {
        // Create test meshes with different alpha values
        Mesh quad1;
        quad1.x = 150; quad1.y = 150;
        quad1.color = {255, 0, 0, 200};
        quad1.name = "Alpha Blend";
        quad1.vertices = {
            {0, 0, 0, 0}, {100, 0, 1, 0}, {100, 100, 1, 1}, {0, 100, 0, 1}
        };
        quad1.indices = {0, 1, 2, 2, 3, 0};
        meshes.push_back(quad1);
        
        Mesh quad2;
        quad2.x = 350; quad2.y = 150;
        quad2.color = {0, 255, 0, 150};
        quad2.name = "Semi-Transparent";
        quad2.vertices = {
            {0, 0, 0, 0}, {100, 0, 1, 0}, {100, 100, 1, 1}, {0, 100, 0, 1}
        };
        quad2.indices = {0, 1, 2, 2, 3, 0};
        meshes.push_back(quad2);
        
        Mesh quad3;
        quad3.x = 550; quad3.y = 150;
        quad3.color = {0, 0, 255, 100};
        quad3.name = "Highly Transparent";
        quad3.vertices = {
            {0, 0, 0, 0}, {100, 0, 1, 0}, {100, 100, 1, 1}, {0, 100, 0, 1}
        };
        quad3.indices = {0, 1, 2, 2, 3, 0};
        meshes.push_back(quad3);
        
        Mesh quad4;
        quad4.x = 250; quad4.y = 350;
        quad4.color = {255, 255, 0, 180};
        quad4.name = "Additive Test";
        quad4.vertices = {
            {0, 0, 0, 0}, {100, 0, 1, 0}, {100, 100, 1, 1}, {0, 100, 0, 1}
        };
        quad4.indices = {0, 1, 2, 2, 3, 0};
        meshes.push_back(quad4);
        
        Mesh quad5;
        quad5.x = 450; quad5.y = 350;
        quad5.color = {255, 0, 255, 120};
        quad5.name = "Multiply Test";
        quad5.vertices = {
            {0, 0, 0, 0}, {100, 0, 1, 0}, {100, 100, 1, 1}, {0, 100, 0, 1}
        };
        quad5.indices = {0, 1, 2, 2, 3, 0};
        meshes.push_back(quad5);
    }

    void update() override {
        // Cycle blend modes with spacebar
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
                selectedMode = (selectedMode + 1) % 4;
                switch(selectedMode) {
                    case 0: currentBlendMode = SDL_BLENDMODE_NONE; break;
                    case 1: currentBlendMode = SDL_BLENDMODE_BLEND; break;
                    case 2: currentBlendMode = SDL_BLENDMODE_ADD; break;
                    case 3: currentBlendMode = SDL_BLENDMODE_MOD; break;
                    default: break;  // selectedMode is always 0-3 (mod 4)
                }
            }
        }
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(30, 30, 40, 255);
        
        // Draw background pattern to show transparency
        SDL_SetRenderDrawColor(renderer->renderer, 100, 100, 100, 255);
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 8; j++) {
                if ((i + j) % 2 == 0) {
                    SDL_Rect rect = {i * 80, j * 75, 80, 75};
                    SDL_RenderFillRect(renderer->renderer, &rect);
                }
            }
        }
        
        // Set blend mode
        SDL_SetRenderDrawBlendMode(renderer->renderer, currentBlendMode);
        
        // Render meshes
        for (const auto& mesh : meshes) {
            SDL_SetRenderDrawColor(renderer->renderer, 
                mesh.color.r, mesh.color.g, mesh.color.b, mesh.color.a);
            
            // Draw filled quad
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                const MeshVertex& v0 = mesh.vertices[static_cast<std::size_t>(mesh.indices[i])];
                const MeshVertex& v1 = mesh.vertices[static_cast<std::size_t>(mesh.indices[i + 1])];
                const MeshVertex& v2 = mesh.vertices[static_cast<std::size_t>(mesh.indices[i + 2])];
                
                // Simple triangle rasterization using lines
                SDL_RenderDrawLine(renderer->renderer,
                    (int)(mesh.x + v0.x), (int)(mesh.y + v0.y),
                    (int)(mesh.x + v1.x), (int)(mesh.y + v1.y));
                SDL_RenderDrawLine(renderer->renderer,
                    (int)(mesh.x + v1.x), (int)(mesh.y + v1.y),
                    (int)(mesh.x + v2.x), (int)(mesh.y + v2.y));
                SDL_RenderDrawLine(renderer->renderer,
                    (int)(mesh.x + v2.x), (int)(mesh.y + v2.y),
                    (int)(mesh.x + v0.x), (int)(mesh.y + v0.y));
            }
            
            // Draw label
            wchar_t label[64];
            swprintf(label, 64, L"%s", mesh.name.c_str());
            Rect<float> textBounds(mesh.x, mesh.y + 110, 120, 30);
            renderer->getTextWriter()->drawTextToRenderer(label, renderer->renderer, textBounds, "/default.ttf");
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        const wchar_t* modeNames[] = {L"NONE", L"BLEND", L"ADD", L"MOD"};
        wchar_t info[128];
        swprintf(info, 128, L"Alpha Modes - Press SPACE to cycle\nCurrent: %s", modeNames[selectedMode]);
        Rect<float> textBounds(10, 10, 400, 50);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Mesh2DAlphaGame app("Umbra Mesh2D Alpha Mode", 800, 600);
    app.run();
    return 0;
}
