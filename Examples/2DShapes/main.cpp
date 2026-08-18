// 2D Shapes Showcase for Umbra Engine
// Renders simple 2D primitive shapes like circles and polygons
#include "Engine/Core/Game2D.h"
#include <cmath>

class Shapes2DGame : public Game2D
{
    struct Circle {
        float x, y, radius;
        SDL_Color color;
    };
    
    struct Polygon {
        std::vector<Point2D> vertices;
        SDL_Color color;
        float x, y; // center position
    };
    
    std::vector<Circle> circles;
    std::vector<Polygon> polygons;

public:
    Shapes2DGame() : Game2D("Umbra 2D Shapes", 800, 600, 20) {}

    void initGame() override {
        // Create circles
        circles = {
            {200, 150, 50, {255, 0, 0, 255}},
            {400, 150, 70, {0, 255, 0, 255}},
            {600, 150, 40, {0, 0, 255, 255}},
            {300, 300, 80, {255, 255, 0, 255}},
            {500, 300, 60, {255, 0, 255, 255}}
        };
        
        // Create polygons
        Polygon triangle;
        triangle.x = 200; triangle.y = 450;
        triangle.color = {255, 128, 0, 255};
        triangle.vertices = {
            Point2D(-40, -35), Point2D(40, -35), Point2D(0, 35)
        };
        polygons.push_back(triangle);
        
        Polygon square;
        square.x = 400; square.y = 450;
        square.color = {0, 255, 255, 255};
        square.vertices = {
            Point2D(-35, -35), Point2D(35, -35), Point2D(35, 35), Point2D(-35, 35)
        };
        polygons.push_back(square);
        
        Polygon pentagon;
        pentagon.x = 600; pentagon.y = 450;
        pentagon.color = {128, 0, 255, 255};
        float angleStep = 2.0f * 3.14159f / 5.0f;
        for (int i = 0; i < 5; i++) {
            float angle = i * angleStep - 3.14159f / 2.0f;
            pentagon.vertices.emplace_back(
                cos(angle) * 40,
                sin(angle) * 40
            );
        }
        polygons.push_back(pentagon);
    }

    void renderGame() override {
        Renderer* renderer = getRenderer();
        
        // Render circles using filled circles (approximated with rectangles for SDL2)
        for (const auto& circle : circles) {
            SDL_SetRenderDrawColor(renderer->renderer, circle.color.r, circle.color.g, circle.color.b, circle.color.a);

            // Draw circle using filled rectangle (SDL2 doesn't have native circle drawing)
            SDL_Rect rect = {
                static_cast<int>(circle.x - circle.radius),
                static_cast<int>(circle.y - circle.radius),
                static_cast<int>(circle.radius * 2),
                static_cast<int>(circle.radius * 2)
            };
            SDL_RenderFillRect(renderer->renderer, &rect);

            // Draw circle outline using points (approximation)
            SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
            int cx = static_cast<int>(circle.x);
            int cy = static_cast<int>(circle.y);
            int r = static_cast<int>(circle.radius);
            for (int angle = 0; angle < 360; angle += 5) {
                float rad = angle * 3.14159f / 180.0f;
                int px = cx + (int)(cos(rad) * r);
                int py = cy + (int)(sin(rad) * r);
                SDL_RenderDrawPoint(renderer->renderer, px, py);
            }
        }
        
        // Render polygons
        for (const auto& poly : polygons) {
            SDL_SetRenderDrawColor(renderer->renderer, poly.color.r, poly.color.g, poly.color.b, poly.color.a);
            
            // Fill polygon
            for (size_t i = 0; i < poly.vertices.size(); i++) {
                Point2D v1 = poly.vertices[i] + Point2D(poly.x, poly.y);
                Point2D v2 = poly.vertices[(i + 1) % poly.vertices.size()] + Point2D(poly.x, poly.y);
                SDL_RenderDrawLine(renderer->renderer, 
                    static_cast<int>(v1.x), static_cast<int>(v1.y),
                    static_cast<int>(v2.x), static_cast<int>(v2.y));
            }
        }
        
        // Draw info text
        std::wstring info = L"2D Shapes: Circles and Polygons";
        Rect<float> textBounds(10, 10, 400, 30);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Shapes2DGame app;
    app.run();
    return 0;
}
