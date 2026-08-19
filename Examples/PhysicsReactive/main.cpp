// Reactive Physics Showcase for Umbra Engine
#include "Engine/Core/SDLApp.h"
#include "Engine/Core/Physics.h"
#include "Engine/Rendering/ParticleSystem.h"
#include <vector>

struct Ball : public PhysicsObject {
    SDL_Color color;
    float radius;
    std::shared_ptr<ParticleSystem> particles;

    Ball(World& w, float x, float y, float r, std::shared_ptr<ParticleSystem> p) 
        : PhysicsObject(w, x, y, r*2, r*2, true), radius(r), particles(std::move(p)) 
    {
        color = {255, 255, 255, 255};
        
        // Define bouncy physics
        b2ShapeId sId;
        b2Body_GetShapes(bodyId, &sId, 1);
        b2Shape_SetRestitution(sId, 1.0f);
        b2Shape_SetFriction(sId, 0.0f);
        
        b2Body_SetLinearVelocity(bodyId, { (float)(rand()%20 - 10), (float)(rand()%20 - 10) });

        // Set collision callback
        onCollision = [this]() {
            this->color = { (Uint8)(rand()%255), (Uint8)(rand()%255), (Uint8)(rand()%255), 255 };
            b2Vec2 pos = b2Body_GetPosition(this->bodyId);
            for(int i=0; i<15; i++) {
                float ang = (rand()%360) * 0.0174f;
                float spd = (rand()%100) * 0.05f;
                this->particles->emit(pos.x, pos.y, cos(ang)*spd, sin(ang)*spd, this->color, 0.8f, 4.0f);
            }
        };
    }
};

class PhysicsReactiveGame : public Game
{
    std::unique_ptr<World> world;
    std::shared_ptr<ParticleSystem> particles;
    std::vector<std::unique_ptr<Ball>> balls;
    std::vector<std::unique_ptr<PhysicsObject>> walls;

public:
    PhysicsReactiveGame(const char* title, int width, int height) : Game(title, width, height) {}

    void onStart() override {
        world = std::make_unique<World>();
        particles = std::make_shared<ParticleSystem>(2000);

        // Walls
        walls.push_back(std::make_unique<PhysicsObject>(*world, 10.0f, 350.0f, 20.0f, 700.0f, false));
        walls.push_back(std::make_unique<PhysicsObject>(*world, 690.0f, 350.0f, 20.0f, 700.0f, false));
        walls.push_back(std::make_unique<PhysicsObject>(*world, 350.0f, 10.0f, 700.0f, 20.0f, false));
        walls.push_back(std::make_unique<PhysicsObject>(*world, 350.0f, 690.0f, 700.0f, 20.0f, false));

        balls.push_back(std::make_unique<Ball>(*world, 350.0f, 350.0f, 15.0f, particles));
    }

    void update() override {
        world->simulateStep();
        particles->update();

        Renderer* renderer = getRenderer();
        renderer->clearScreen(10, 10, 15, 255);

        // Draw Walls
        SDL_SetRenderDrawColor(renderer->renderer, 50, 50, 60, 255);
        SDL_Rect border = {10, 10, 680, 680};
        SDL_RenderDrawRect(renderer->renderer, &border);

        // Draw Balls
        for(auto& ball : balls) {
            b2Vec2 p = b2Body_GetPosition(ball->bodyId);
            SDL_SetRenderDrawColor(renderer->renderer, ball->color.r, ball->color.g, ball->color.b, 255);
            SDL_Rect r = { (int)(p.x - ball->radius), (int)(p.y - ball->radius), (int)ball->radius*2, (int)ball->radius*2 };
            SDL_RenderFillRect(renderer->renderer, &r);
        }

        particles->render(renderer->renderer);

        // Click to spawn
        int mx, my;
        if (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(1)) {
            static uint32_t lastS = 0;
            if (SDL_GetTicks() - lastS > 300) {
                balls.push_back(std::make_unique<Ball>(*world, (float)mx, (float)my, 12.0f, particles));
                lastS = SDL_GetTicks();
            }
        }
    }
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* args[])
{
#ifdef __EMSCRIPTEN__
    static PhysicsReactiveGame app("Umbra Physics Reactive", 700, 700);
#else
    PhysicsReactiveGame app("Umbra Physics Reactive", 700, 700);
#endif
    app.run();
    return 0;
}
