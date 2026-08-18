// Mesh2D Repeated Texture Showcase for Umbra Engine
// Showcase of using uv_transform on the ColorMaterial of a Mesh2D
#include "Engine/Core/SDLApp.h"
#include <cmath>

class Mesh2DRepeatedTextureGame : public Game
{
    struct TexturedQuad {
        float x, y, width, height;
        float uScale, vScale;      // UV scale (repetition)
        float uOffset, vOffset;    // UV offset
        float rotation;
        SDL_Color color;
    };
    
    std::vector<TexturedQuad> quads;
    float time;

public:
    Mesh2DRepeatedTextureGame(const char* title, int width, int height) : Game(title, width, height) {
        time = 0;
    }

    void onStart() override {
        // Create quads with different UV transforms
        TexturedQuad q1;
        q1.x = 100; q1.y = 100; q1.width = 150; q1.height = 150;
        q1.uScale = 1; q1.vScale = 1;
        q1.uOffset = 0; q1.vOffset = 0;
        q1.rotation = 0;
        q1.color = {255, 200, 100, 255};
        quads.push_back(q1);
        
        TexturedQuad q2;
        q2.x = 300; q2.y = 100; q2.width = 150; q2.height = 150;
        q2.uScale = 4; q2.vScale = 4;  // 4x4 repetition
        q2.uOffset = 0; q2.vOffset = 0;
        q2.rotation = 0;
        q2.color = {100, 255, 150, 255};
        quads.push_back(q2);
        
        TexturedQuad q3;
        q3.x = 500; q3.y = 100; q3.width = 150; q3.height = 150;
        q3.uScale = 8; q3.vScale = 8;  // 8x8 repetition
        q3.uOffset = 0; q3.vOffset = 0;
        q3.rotation = 0;
        q3.color = {100, 150, 255, 255};
        quads.push_back(q3);
        
        TexturedQuad q4;
        q4.x = 200; q4.y = 350; q4.width = 150; q4.height = 150;
        q4.uScale = 2; q4.vScale = 2;
        q4.uOffset = 0; q4.vOffset = 0;
        q4.rotation = 0;
        q4.color = {255, 100, 150, 255};
        quads.push_back(q4);
        
        TexturedQuad q5;
        q5.x = 400; q5.y = 350; q5.width = 150; q5.height = 150;
        q5.uScale = 2; q5.vScale = 2;
        q5.uOffset = 0.5f; q5.vOffset = 0.5f;  // Offset UV
        q5.rotation = 0;
        q5.color = {150, 100, 255, 255};
        quads.push_back(q5);
    }

    void update() override {
        time += 0.016f;
        
        // Animate UV transforms
        quads[3].uOffset = sin(time) * 0.5f;
        quads[3].vOffset = cos(time * 0.7f) * 0.5f;
        
        quads[4].rotation = time * 0.5f;
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(30, 30, 40, 255);
        
        // Render quads with UV transform visualization
        for (const auto& quad : quads) {
            // Draw quad outline
            SDL_SetRenderDrawColor(renderer->renderer, 
                quad.color.r, quad.color.g, quad.color.b, 255);
            
            SDL_Rect rect = {(int)quad.x, (int)quad.y, (int)quad.width, (int)quad.height};
            SDL_RenderDrawRect(renderer->renderer, &rect);
            
            // Draw internal grid to visualize UV repetition
            SDL_SetRenderDrawColor(renderer->renderer, 
                quad.color.r, quad.color.g, quad.color.b, 100);
            
            int gridLines = (int)quad.uScale;
            for (int i = 1; i < gridLines; i++) {
                float lineX = quad.x + (quad.width / gridLines) * i;
                SDL_RenderDrawLine(renderer->renderer, (int)lineX, (int)quad.y, 
                    (int)lineX, (int)(quad.y + quad.height));
                float lineY = quad.y + (quad.height / gridLines) * i;
                SDL_RenderDrawLine(renderer->renderer, (int)quad.x, (int)lineY, 
                    (int)(quad.x + quad.width), (int)lineY);
            }
            
            // Draw label
            wchar_t label[64];
            swprintf(label, 64, L"UV: %.1fx%.1f", quad.uScale, quad.vScale);
            Rect<float> textBounds(quad.x, quad.y + quad.height + 5, 150, 20);
            renderer->getTextWriter()->drawTextToRenderer(label, renderer->renderer, textBounds, "/default.ttf");
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        std::wstring info = L"Mesh2D Repeated Texture - UV Transform Demonstration";
        Rect<float> textBounds(10, 10, 500, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Mesh2DRepeatedTextureGame app("Umbra Mesh2D Repeated Texture", 800, 600);
    app.run();
    return 0;
}
