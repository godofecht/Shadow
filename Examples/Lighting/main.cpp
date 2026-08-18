// Dynamic 2D Lighting & Shadows for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include "Engine/Core/UI.h"
#include <vector>
#include <cmath>
#include <algorithm>

struct Obstacle {
    SDL_Rect rect;
};

struct Vec2 {
    float x, y;
};

static Vec2 normalize(const Vec2& v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len < 0.0001f) return {0.0f, 0.0f};
    return {v.x / len, v.y / len};
}

static void drawFilledQuad(SDL_Renderer* renderer, const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d, SDL_Color color) {
    SDL_Vertex vertices[4];
    vertices[0].position = {a.x, a.y};
    vertices[1].position = {b.x, b.y};
    vertices[2].position = {c.x, c.y};
    vertices[3].position = {d.x, d.y};
    for (auto& v : vertices) {
        v.color = color;
        v.tex_coord = {0.0f, 0.0f};
    }
    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, nullptr, vertices, 4, indices, 6);
}

static void drawRadialLight(SDL_Renderer* renderer, float cx, float cy, float radius, SDL_Color center, SDL_Color edge) {
    const int segments = 64;
    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    verts.reserve(segments + 2);
    idx.reserve(static_cast<std::size_t>(segments) * 3);

    SDL_Vertex c{};
    c.position = {cx, cy};
    c.color = center;
    c.tex_coord = {0.0f, 0.0f};
    verts.push_back(c);

    for (int i = 0; i <= segments; ++i) {
        float a = (6.2831853f * i) / (float)segments;
        SDL_Vertex v{};
        v.position = {cx + std::cos(a) * radius, cy + std::sin(a) * radius};
        v.color = edge;
        v.tex_coord = {0.0f, 0.0f};
        verts.push_back(v);
    }

    for (int i = 1; i <= segments; ++i) {
        idx.push_back(0);
        idx.push_back(i);
        idx.push_back(i + 1);
    }

    SDL_RenderGeometry(renderer, nullptr, verts.data(), (int)verts.size(), idx.data(), (int)idx.size());
}

class LightingGame : public Game
{
    std::vector<Obstacle> obstacles;
    std::unique_ptr<ExplanationOverlay> explanation;

public:
    LightingGame(const char* title, int width, int height) : Game(title, width, height) {}

    void onStart() override {
        obstacles = {
            {{100, 100, 50, 50}},
            {{500, 200, 80, 80}},
            {{300, 400, 100, 40}},
            {{150, 550, 60, 60}}
        };
        explanation = std::make_unique<ExplanationOverlay>(20, 20, 350);
        explanation->addLine("FEATURE: Dynamic 2D Lighting");
        explanation->addLine("TECH: Geometric Shadow Casting");
        explanation->addLine("LOGIC: Occlusion Volumes per Obstacle");
        explanation->addLine("INPUT: Move mouse to control Light");
    }

    void update() override {
        int mx, my;
        SDL_GetMouseState(&mx, &my);

        SDL_Renderer* sdl = getRenderer()->renderer;
        getRenderer()->clearScreen(40, 42, 48, 255);

        // Ground grid for depth cues.
        SDL_SetRenderDrawBlendMode(sdl, SDL_BLENDMODE_NONE);
        for (int y = 0; y < 700; y += 40) {
            for (int x = 0; x < 700; x += 40) {
                bool checker = ((x / 40 + y / 40) % 2) == 0;
                SDL_SetRenderDrawColor(sdl, checker ? 52 : 58, checker ? 55 : 62, checker ? 66 : 72, 255);
                SDL_Rect tile = {x, y, 40, 40};
                SDL_RenderFillRect(sdl, &tile);
            }
        }

        // Ambient darkness layer.
        SDL_SetRenderDrawBlendMode(sdl, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(sdl, 0, 0, 0, 120);
        SDL_Rect full = {0, 0, 700, 700};
        SDL_RenderFillRect(sdl, &full);

        // Physically-plausible radial falloff (core + halo).
        SDL_SetRenderDrawBlendMode(sdl, SDL_BLENDMODE_ADD);
        drawRadialLight(sdl, (float)mx, (float)my, 250.0f, {255, 235, 175, 130}, {255, 235, 175, 0});
        drawRadialLight(sdl, (float)mx, (float)my, 90.0f, {255, 250, 220, 180}, {255, 250, 220, 0});

        // Shadow volumes (filled quads) extruded away from the light.
        SDL_SetRenderDrawBlendMode(sdl, SDL_BLENDMODE_BLEND);
        const float shadowLen = 1200.0f;
        for (const auto& obs : obstacles) {
            float cx = obs.rect.x + obs.rect.w * 0.5f;
            float cy = obs.rect.y + obs.rect.h * 0.5f;
            float dx = cx - (float)mx;
            float dy = cy - (float)my;

            Vec2 p1, p2;
            if (std::abs(dx) > std::abs(dy)) {
                if (dx > 0) { // obstacle is right of light -> cast from right face
                    p1 = {(float)(obs.rect.x + obs.rect.w), (float)obs.rect.y};
                    p2 = {(float)(obs.rect.x + obs.rect.w), (float)(obs.rect.y + obs.rect.h)};
                } else {      // cast from left face
                    p1 = {(float)obs.rect.x, (float)(obs.rect.y + obs.rect.h)};
                    p2 = {(float)obs.rect.x, (float)obs.rect.y};
                }
            } else {
                if (dy > 0) { // obstacle is below light -> cast from bottom face
                    p1 = {(float)obs.rect.x, (float)(obs.rect.y + obs.rect.h)};
                    p2 = {(float)(obs.rect.x + obs.rect.w), (float)(obs.rect.y + obs.rect.h)};
                } else {      // cast from top face
                    p1 = {(float)(obs.rect.x + obs.rect.w), (float)obs.rect.y};
                    p2 = {(float)obs.rect.x, (float)obs.rect.y};
                }
            }

            Vec2 d1 = normalize({p1.x - mx, p1.y - my});
            Vec2 d2 = normalize({p2.x - mx, p2.y - my});
            Vec2 e1 = {p1.x + d1.x * shadowLen, p1.y + d1.y * shadowLen};
            Vec2 e2 = {p2.x + d2.x * shadowLen, p2.y + d2.y * shadowLen};
            drawFilledQuad(sdl, p1, p2, e2, e1, {0, 0, 0, 190});

            // Penumbra edge lines for clearer shadow shape.
            SDL_SetRenderDrawColor(sdl, 0, 0, 0, 120);
            SDL_RenderDrawLine(sdl, (int)p1.x, (int)p1.y, (int)e1.x, (int)e1.y);
            SDL_RenderDrawLine(sdl, (int)p2.x, (int)p2.y, (int)e2.x, (int)e2.y);
        }

        // Obstacles (occluders).
        SDL_SetRenderDrawBlendMode(sdl, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(sdl, 96, 102, 118, 255);
        for (const auto& obs : obstacles) {
            SDL_RenderFillRect(sdl, &obs.rect);
            SDL_SetRenderDrawColor(sdl, 170, 180, 210, 255);
            SDL_RenderDrawRect(sdl, &obs.rect);
            SDL_SetRenderDrawColor(sdl, 96, 102, 118, 255);
        }

        // Light source marker.
        SDL_SetRenderDrawColor(sdl, 255, 255, 240, 255);
        SDL_Rect lightCenter = {mx - 7, my - 7, 14, 14};
        SDL_RenderFillRect(sdl, &lightCenter);

        explanation->render(getRenderer());
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static LightingGame app("Umbra Dynamic Lighting", 700, 700);
#else
    LightingGame app("Umbra Dynamic Lighting", 700, 700);
#endif
    app.run();
    return 0;
}
