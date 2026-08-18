// Generic 2D Rotation Showcase for Umbra Engine
// Demonstrates rotating entities in 2D with quaternions
#include "Engine/Core/SDLApp.h"
#include <cmath>
#include <vector>

// Simple 2D quaternion representation (using complex numbers for 2D rotation)
class Quaternion2D {
public:
    float w, z; // w = cos(angle/2), z = sin(angle/2) for 2D
    
    Quaternion2D() : w(1.0f), z(0.0f) {}
    
    // Create from angle in radians
    static Quaternion2D fromAngle(float angle) {
        Quaternion2D q;
        q.w = cos(angle / 2.0f);
        q.z = sin(angle / 2.0f);
        return q;
    }
    
    // Create from axis-angle (2D rotation around Z axis)
    static Quaternion2D fromAxisAngle([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] float z, float angle) {
        // For 2D, we only care about Z-axis rotation
        return fromAngle(angle);
    }
    
    // Multiply quaternions
    Quaternion2D operator*(const Quaternion2D& other) const {
        Quaternion2D result;
        result.w = w * other.w - z * other.z;
        result.z = w * other.z + z * other.w;
        return result;
    }
    
    // Rotate a 2D point
    void rotate(float& x, float& y) const {
        // q * v * q^-1 where v = (x, y, 0)
        float x2 = x * (w * w - z * z) - y * (2 * w * z);
        float y2 = x * (2 * w * z) + y * (w * w - z * z);
        x = x2;
        y = y2;
    }
    
    // Get angle from quaternion
    float toAngle() const {
        return atan2(z, w) * 2.0f;
    }
    
    // Spherical linear interpolation
    static Quaternion2D slerp(const Quaternion2D& a, const Quaternion2D& b, float t) {
        float dot = a.w * b.w + a.z * b.z;
        
        // If dot product is negative, negate one quaternion
        Quaternion2D b2 = b;
        if (dot < 0) {
            b2.w = -b.w;
            b2.z = -b.z;
            dot = -dot;
        }
        
        // Clamp dot to [-1, 1]
        dot = std::max(-1.0f, std::min(1.0f, dot));
        
        float theta = acos(dot);
        float sinTheta = sin(theta);
        
        if (sinTheta < 0.001f) {
            // Quaternions are very close, use linear interpolation
            Quaternion2D result;
            result.w = a.w * (1 - t) + b2.w * t;
            result.z = a.z * (1 - t) + b2.z * t;
            return result;
        }
        
        float ratioA = sin((1 - t) * theta) / sinTheta;
        float ratioB = sin(t * theta) / sinTheta;
        
        Quaternion2D result;
        result.w = ratioA * a.w + ratioB * b2.w;
        result.z = ratioA * a.z + ratioB * b2.z;
        return result;
    }
};

class Generic2DRotationGame : public Game
{
    struct RotatingObject {
        float x, y;
        float width, height;
        Quaternion2D rotation;
        Quaternion2D targetRotation;
        SDL_Color color;
        float rotationSpeed;
    };
    
    std::vector<RotatingObject> objects;
    float time;

public:
    Generic2DRotationGame(const char* title, int width, int height) : Game(title, width, height) {
        time = 0;
    }

    void onStart() override {
        // Create rotating objects with different quaternion behaviors
        RotatingObject obj1;
        obj1.x = 200; obj1.y = 200;
        obj1.width = 80; obj1.height = 40;
        obj1.rotation = Quaternion2D::fromAngle(0);
        obj1.targetRotation = Quaternion2D::fromAngle(3.14159f);
        obj1.color = {255, 100, 100, 255};
        obj1.rotationSpeed = 2.0f;
        objects.push_back(obj1);
        
        RotatingObject obj2;
        obj2.x = 500; obj2.y = 200;
        obj2.width = 60; obj2.height = 60;
        obj2.rotation = Quaternion2D::fromAngle(0);
        obj2.targetRotation = Quaternion2D::fromAngle(3.14159f * 2.0f);
        obj2.color = {100, 255, 100, 255};
        obj2.rotationSpeed = 3.0f;
        objects.push_back(obj2);
        
        RotatingObject obj3;
        obj3.x = 350; obj3.y = 400;
        obj3.width = 100; obj3.height = 30;
        obj3.rotation = Quaternion2D::fromAngle(0);
        obj3.targetRotation = Quaternion2D::fromAngle(3.14159f * 4.0f);
        obj3.color = {100, 100, 255, 255};
        obj3.rotationSpeed = 1.5f;
        objects.push_back(obj3);
    }

    void update() override {
        time += 0.016f;
        
        // Update rotations using quaternion slerp
        for (auto& obj : objects) {
            // Continuously rotate
            float currentAngle = obj.rotation.toAngle();
            obj.targetRotation = Quaternion2D::fromAngle(currentAngle + obj.rotationSpeed * 0.016f);
            
            // Smooth interpolation
            obj.rotation = Quaternion2D::slerp(obj.rotation, obj.targetRotation, 0.1f);
        }
        
        Renderer* renderer = getRenderer();
        renderer->clearScreen(25, 25, 35, 255);
        
        // Render rotating objects
        for (const auto& obj : objects) {
            SDL_SetRenderDrawColor(renderer->renderer, 
                obj.color.r, obj.color.g, obj.color.b, obj.color.a);
            
            // Get rotated corners
            float corners[8] = {
                -obj.width/2, -obj.height/2,
                obj.width/2, -obj.height/2,
                obj.width/2, obj.height/2,
                -obj.width/2, obj.height/2
            };
            
            // Apply quaternion rotation
            for (std::size_t i = 0; i < 4; i++) {
                float x = corners[i * 2];
                float y = corners[i * 2 + 1];
                obj.rotation.rotate(x, y);
                corners[i * 2] = x + obj.x;
                corners[i * 2 + 1] = y + obj.y;
            }
            
            // Draw rotated rectangle
            for (std::size_t i = 0; i < 4; i++) {
                std::size_t next = (i + 1) % 4;
                SDL_RenderDrawLine(renderer->renderer,
                    (int)corners[i * 2], (int)corners[i * 2 + 1],
                    (int)corners[next * 2], (int)corners[next * 2 + 1]);
            }
            
            // Draw center point
            SDL_SetRenderDrawColor(renderer->renderer, 255, 255, 255, 255);
            SDL_Rect pt = {(int)obj.x - 3, (int)obj.y - 3, 6, 6};
            SDL_RenderFillRect(renderer->renderer, &pt);
        }
    }

    void renderPostFX() override {
        Renderer* renderer = getRenderer();
        
        wchar_t info[256];
        swprintf(info, 256, 
            L"Generic 2D Rotation - Using Quaternions for smooth rotation\n"
            L"Quaternion slerp interpolation demonstrated");
        Rect<float> textBounds(10, 10, 500, 50);
        renderer->getTextWriter()->drawTextToRenderer(info, renderer->renderer, textBounds, "/default.ttf");
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
    Generic2DRotationGame app("Umbra Generic 2D Rotation", 800, 600);
    app.run();
    return 0;
}
