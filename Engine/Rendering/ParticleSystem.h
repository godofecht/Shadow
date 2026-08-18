// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once

#include <vector>
#include <SDL2/SDL.h>
#include <algorithm>
#include <random>

struct Particle {
    float x, y;
    float vx, vy;
    float life;      // 1.0 to 0.0
    float decay;     // life reduction per frame
    SDL_Color color;
    float size;
};

class ParticleSystem {
public:
    ParticleSystem(std::size_t maxParticles = 1000) : maxParticles(maxParticles) {
        particles.reserve(maxParticles);
    }

    void emit(float x, float y, float vx, float vy, SDL_Color color, float life = 1.0f, float size = 4.0f) {
        if (particles.size() >= maxParticles) return;
        
        particles.push_back({
            x, y, vx, vy, life, 
            0.01f + (rand() % 100) / 5000.0f, 
            color, size
        });
    }

    void update() {
        for (auto it = particles.begin(); it != particles.end(); ) {
            it->x += it->vx;
            it->y += it->vy;
            it->life -= it->decay;

            if (it->life <= 0) {
                it = particles.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Observability for tests / debug
    int size() const { return static_cast<int>(particles.size()); }
    bool empty() const { return particles.empty(); }

    void render(SDL_Renderer* renderer) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        
        for (const auto& p : particles) {
            float currentSize = std::max(2.0f, p.size * p.life);
            Uint8 alpha = static_cast<Uint8>(p.color.a * p.life);
            
            SDL_SetRenderDrawColor(renderer, p.color.r, p.color.g, p.color.b, alpha);
            
            // Draw particle as a small square
            SDL_Rect rect = {
                static_cast<int>(p.x - currentSize/2),
                static_cast<int>(p.y - currentSize/2),
                static_cast<int>(currentSize),
                static_cast<int>(currentSize)
            };
            SDL_RenderFillRect(renderer, &rect);
        }

        // Restore a standard mode for subsequent UI/text rendering.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }

private:
    std::vector<Particle> particles;
    std::size_t maxParticles;
};
