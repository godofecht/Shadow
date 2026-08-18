// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
// Shadow Engine - see LICENSE for details
#pragma once
//
// GameJuice - the shared "feel" toolkit every game ships with, so a game can
// reach the AAA-feel bar (particles, screen shake, hit-stop, floating text,
// procedural sound) without reinventing it per game.
//
// Everything is header-only and asset-free: sound effects are tiny WAVs
// synthesized in memory (SDL_mixer converts them to the opened device), so
// games run identically natively, in WASM, and in headless CI. All systems
// are deterministic and cheap enough to run every frame.
//
// Usage (in a Game2D subclass):
//   uj::ParticleSystem particles;  uj::ScreenShake shake;
//   uj::HitStop hitStop;           uj::SfxSynth sfx;
//   uj::FloatingText floatTexts;   // created via Game2D::createText()
//   // updateGame:  hitStop/particles/shake/floatTexts update(dt)
//   // renderGame:  apply shake.offset() to world drawing, then
//   //              particles.render(renderer), floatTexts.render(renderer)

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "Engine/Core/UI.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace uj {

// ---- Procedural SFX ---------------------------------------------------------
// Tiny 16-bit mono WAVs synthesized in memory. Effect chunks are built lazily
// on first use and cached; play() safely no-ops when audio is unavailable
// (headless CI without a device, WASM before a user gesture, etc).

enum class Sfx {
    // Shared vocabulary across games.
    Swing,   // sword / whoosh
    Hit,     // impact
    Kill,    // enemy down
    Pickup,  // coin / item
    Hurt,    // player damaged
    Door,    // door / creak
    Descend, // stairs / level down
    Win,     // victory arpeggio
    // Arcade verbs.
    Serve,   // launch / fwip
    Ping,    // paddle / blip
    Thock,   // brick / block
    Lose,    // fail / fall
    Jump,    // hop up
    Coin,    // classic two-note pickup
    Shoot,   // projectile
    Explode, // boom
    Clear,   // line / fanfare
    // Musical notes (rhythm / memory games: Simon Says, Beat Mapper).
    Note1,   // 440 Hz A
    Note2,   // 523 Hz C
    Note3,   // 659 Hz E
    Note4,   // 784 Hz G
};

struct Wav16 {
    int rate = 22050;
    std::vector<int16_t> samples;
};

// Build a proper little-endian RIFF/WAVE file image in memory.
inline std::vector<uint8_t> wavBytes(const Wav16& w) {
    const uint32_t dataLen = (uint32_t)w.samples.size() * 2u;
    std::vector<uint8_t> b(44 + dataLen, 0);
    auto put = [&b](size_t at, const void* src, size_t n) {
        for (size_t i = 0; i < n; ++i) b[at + i] = ((const uint8_t*)src)[i];
    };
    const uint32_t chunkSize = 36u + dataLen;
    const uint16_t fmtTag = 1;             // PCM
    const uint16_t channels = 1;
    const uint16_t bits = 16;
    const uint32_t byteRate = (uint32_t)w.rate * channels * bits / 8u;
    const uint16_t blockAlign = channels * bits / 8u;
    put(0, "RIFF", 4); put(4, &chunkSize, 4); put(8, "WAVE", 4);
    put(12, "fmt ", 4); const uint32_t sub1 = 16; put(16, &sub1, 4);
    put(20, &fmtTag, 2); put(22, &channels, 2); put(24, &w.rate, 4);
    put(28, &byteRate, 4); put(32, &blockAlign, 2); put(34, &bits, 2);
    put(36, "data", 4); put(40, &dataLen, 4);
    for (size_t i = 0; i < w.samples.size(); ++i)
        put(44 + i * 2, &w.samples[i], 2);
    return b;
}

inline Mix_Chunk* makeChunk(const Wav16& w) {
    std::vector<uint8_t> bytes = wavBytes(w);
    SDL_RWops* rw = SDL_RWFromConstMem(bytes.data(), (int)bytes.size());
    if (!rw) return nullptr;
    return Mix_LoadWAV_RW(rw, 1);  // frees rw; audio data is owned by the chunk
}

// --- tiny synth primitives ---------------------------------------------------
inline Wav16 tone(float freqHz, float seconds, float vol, float decay = 6.0f) {
    Wav16 w;
    const int n = (int)(w.rate * seconds);
    w.samples.reserve((size_t)n);
    for (int i = 0; i < n; ++i) {
        const float t = (float)i / (float)w.rate;
        const float env = std::exp(-decay * t);
        w.samples.push_back((int16_t)(std::sin(6.2831853f * freqHz * t) * vol * env * 32767.0f));
    }
    return w;
}

inline Wav16 sweep(float f0, float f1, float seconds, float vol, float decay = 5.0f) {
    Wav16 w;
    const int n = (int)(w.rate * seconds);
    w.samples.reserve((size_t)n);
    for (int i = 0; i < n; ++i) {
        const float t = (float)i / (float)w.rate;
        const float f = f0 + (f1 - f0) * (t / seconds);
        const float env = std::exp(-decay * t);
        w.samples.push_back((int16_t)(std::sin(6.2831853f * f * t) * vol * env * 32767.0f));
    }
    return w;
}

inline Wav16 noise(float seconds, float vol, float decay = 10.0f, float lp = 0.35f) {
    Wav16 w;
    const int n = (int)(w.rate * seconds);
    w.samples.reserve((size_t)n);
    uint32_t rng = 0x1234567u;
    float prev = 0.0f;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const float white = ((float)(rng >> 8) / 8388608.0f) - 1.0f;
        prev = prev * (1.0f - lp) + white * lp;   // crude low-pass = whoosh
        const float t = (float)i / (float)w.rate;
        const float env = std::exp(-decay * t);
        w.samples.push_back((int16_t)(prev * vol * env * 32767.0f));
    }
    return w;
}

inline Wav16 mix(std::initializer_list<Wav16> parts) {
    Wav16 out;
    size_t n = 0;
    for (const auto& p : parts) n = std::max(n, p.samples.size());
    out.samples.assign(n, 0);
    for (const auto& p : parts)
        for (size_t i = 0; i < p.samples.size(); ++i)
            out.samples[i] = (int16_t)std::max(-32767, std::min(32767,
                (int)out.samples[i] + (int)p.samples[i]));
    return out;
}

class SfxSynth {
public:
    ~SfxSynth() {
        for (auto& kv : chunks_) {
            if (kv.second) Mix_FreeChunk(kv.second);
        }
    }

    // Plays one synthesized effect; safely does nothing if audio is
    // unavailable or the chunk could not be built.
    void play(Sfx kind) {
        Mix_Chunk* c = chunkFor(kind);
        if (c) Mix_PlayChannel(-1, c, 0);
    }

private:
    Mix_Chunk* chunkFor(Sfx kind) {
        const int key = (int)kind;
        auto it = chunks_.find(key);
        if (it != chunks_.end()) return it->second;
        Mix_Chunk* c = build(kind);
        chunks_[key] = c;
        return c;
    }

    static Mix_Chunk* build(Sfx kind) {
        Wav16 w;
        switch (kind) {
            case Sfx::Swing:  w = noise(0.14f, 0.55f, 9.0f, 0.28f); break;
            case Sfx::Hit:    w = mix({sweep(320.0f, 140.0f, 0.10f, 0.7f, 14.0f),
                                       noise(0.05f, 0.4f, 24.0f, 0.5f)}); break;
            case Sfx::Kill:   w = mix({sweep(260.0f, 70.0f, 0.22f, 0.75f, 6.0f),
                                       noise(0.16f, 0.5f, 9.0f, 0.3f)}); break;
            case Sfx::Pickup: w = mix({tone(880.0f, 0.08f, 0.5f, 16.0f),
                                       tone(1320.0f, 0.12f, 0.5f, 12.0f)}); break;
            case Sfx::Hurt:   w = mix({sweep(240.0f, 110.0f, 0.28f, 0.7f, 4.0f),
                                       noise(0.2f, 0.35f, 8.0f, 0.4f)}); break;
            case Sfx::Door:   w = noise(0.30f, 0.5f, 5.0f, 0.15f); break;
            case Sfx::Descend: w = mix({sweep(220.0f, 55.0f, 0.5f, 0.7f, 2.5f),
                                        noise(0.5f, 0.3f, 3.0f, 0.12f)}); break;
            case Sfx::Win:    w = mix({tone(523.0f, 0.14f, 0.5f, 9.0f),
                                       tone(659.0f, 0.14f, 0.5f, 9.0f),
                                       tone(784.0f, 0.20f, 0.5f, 7.0f)}); break;
            case Sfx::Serve:  w = noise(0.12f, 0.5f, 9.0f, 0.25f); break;
            case Sfx::Ping:   w = mix({tone(660.0f, 0.05f, 0.5f, 18.0f),
                                       tone(990.0f, 0.07f, 0.45f, 14.0f)}); break;
            case Sfx::Thock:  w = mix({noise(0.06f, 0.55f, 22.0f, 0.5f),
                                       sweep(180.0f, 90.0f, 0.08f, 0.6f, 16.0f)}); break;
            case Sfx::Lose:   w = sweep(400.0f, 70.0f, 0.5f, 0.7f, 3.0f); break;
            case Sfx::Jump:   w = sweep(300.0f, 700.0f, 0.15f, 0.5f, 8.0f); break;
            case Sfx::Coin:   w = mix({tone(988.0f, 0.08f, 0.5f, 16.0f),
                                       tone(1319.0f, 0.10f, 0.5f, 12.0f)}); break;
            case Sfx::Shoot:  w = mix({noise(0.08f, 0.5f, 18.0f, 0.3f),
                                       sweep(600.0f, 200.0f, 0.10f, 0.5f, 10.0f)}); break;
            case Sfx::Explode: w = mix({noise(0.30f, 0.7f, 8.0f, 0.3f),
                                        sweep(150.0f, 40.0f, 0.30f, 0.7f, 4.0f)}); break;
            case Sfx::Clear:  w = mix({tone(659.0f, 0.10f, 0.5f, 12.0f),
                                       tone(784.0f, 0.10f, 0.5f, 12.0f),
                                       tone(988.0f, 0.16f, 0.5f, 9.0f)}); break;
            case Sfx::Note1:  w = tone(440.0f, 0.18f, 0.6f, 6.0f); break;
            case Sfx::Note2:  w = tone(523.25f, 0.18f, 0.6f, 6.0f); break;
            case Sfx::Note3:  w = tone(659.25f, 0.18f, 0.6f, 6.0f); break;
            case Sfx::Note4:  w = tone(783.99f, 0.18f, 0.6f, 6.0f); break;
        }
        return makeChunk(w);
    }

    std::map<int, Mix_Chunk*> chunks_;
};

// ---- Screen shake (trauma-based) ----------------------------------------------
// Trauma 0..1 decays over time; the pixel offset uses trauma^2 so shakes are
// punchy at first and ease off. Deterministic pseudo-random jitter.
class ScreenShake {
public:
    void add(float amount) { trauma_ = std::min(1.0f, trauma_ + amount); }
    void update(float dt, float decayPerSec = 2.2f) {
        trauma_ = std::max(0.0f, trauma_ - decayPerSec * dt);
    }
    float level() const { return trauma_; }

    // Pixel offset for the current frame; (0,0) when settled.
    std::pair<int, int> offset() const {
        const float t = trauma_ * trauma_;
        if (t < 0.001f) return {0, 0};
        const int maxPx = (int)(t * 6.0f) + 1;
        rng_ = rng_ * 1664525u + 1013904223u;
        const int dx = (int)(rng_ % (uint32_t)(maxPx * 2 + 1)) - maxPx;
        const int dy = (int)((rng_ >> 8) % (uint32_t)(maxPx * 2 + 1)) - maxPx;
        return {dx, dy};
    }

private:
    float trauma_ = 0.0f;
    mutable uint32_t rng_ = 0x5A17E5E5u;
};

// ---- Hit-stop ----------------------------------------------------------------
// Freezes the world for a beat on impactful events (brick shatter, damage).
class HitStop {
public:
    void trigger(float seconds) { timer_ = std::max(timer_, seconds); }
    void update(float dt) {
        if (timer_ > 0.0f) timer_ -= dt;
    }
    bool frozen() const { return timer_ > 0.0f; }
    float remaining() const { return timer_; }

private:
    float timer_ = 0.0f;
};

// ---- Particles ----------------------------------------------------------------
// A capped, deterministic particle field: gravity, drag, lifetime fade.
class ParticleSystem {
public:
    struct Particle {
        float x, y, vx, vy, life, maxLife;
        float size;
        SDL_Color color;
    };

    void burst(float x, float y, int count, SDL_Color color,
               float speed = 7.0f, float life = 0.5f, float size = 5.0f) {
        for (int i = 0; i < count && (int)ps_.size() < cap_; ++i) {
            const float ang = randf() * 6.2831853f;
            const float sp = speed * (0.4f + randf() * 0.6f);
            Particle p;
            p.x = x + (randf() - 0.5f) * 4.0f;
            p.y = y + (randf() - 0.5f) * 4.0f;
            p.vx = std::cos(ang) * sp;
            p.vy = std::sin(ang) * sp - speed * 0.15f;
            p.life = p.maxLife = life * (0.6f + randf() * 0.6f);
            p.size = size * (0.7f + randf() * 0.6f);
            p.color = color;
            ps_.push_back(p);
        }
    }

    void update(float dt) {
        for (Particle& p : ps_) {
            p.life -= dt;
            p.vy += 16.0f * dt;           // gravity
            const float drag = 2.5f * dt; // drag
            p.vx -= p.vx * drag;
            p.vy -= p.vy * drag;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
        }
        ps_.erase(std::remove_if(ps_.begin(), ps_.end(),
            [](const Particle& p) { return p.life <= 0.0f; }), ps_.end());
    }

    void render(SDL_Renderer* sdl, int offsetX = 0, int offsetY = 0) const {
        for (const Particle& p : ps_) {
            const int a = (int)(255.0f * (p.life / p.maxLife));
            SDL_SetRenderDrawColor(sdl, p.color.r, p.color.g, p.color.b, a);
            const int s = (int)p.size;
            SDL_Rect r = {(int)p.x + offsetX - s / 2,
                          (int)p.y + offsetY - s / 2, s, s};
            SDL_RenderFillRect(sdl, &r);
        }
    }

    void clear() { ps_.clear(); }
    int count() const { return (int)ps_.size(); }
    void setCap(int cap) { cap_ = cap; }

private:
    float randf() const {
        rng_ = rng_ * 1664525u + 1013904223u;
        return (float)(rng_ & 0xFFFFu) / 65535.0f;
    }
    std::vector<Particle> ps_;
    int cap_ = 400;
    mutable uint32_t rng_ = 0xDA7A5EEDu;
};

// ---- Floating text --------------------------------------------------------------
// Small rising, fading labels ("+10", "WALL CLEARED"). Each label owns a
// TextDisplay the game creates (make_shared<TextDisplay>) and hands in; the
// system moves it and fades it, then releases it when done.
class FloatingText {
public:
    // The display is created by the game (so it uses the game's font
    // pipeline); the system owns its motion and removes it when done. x/y
    // are the starting pixel position.
    void spawn(std::shared_ptr<TextDisplay> display, int x, int y,
               float risePx = 26.0f, float life = 0.9f) {
        Label l;
        l.display = display;
        l.baseX = (float)x;
        l.baseY = (float)y;
        l.rise = risePx;
        l.life = l.maxLife = life;
        labels_.push_back(l);
    }

    void update(float dt) {
        for (Label& l : labels_) {
            l.life -= dt;
            const float t = 1.0f - l.life / l.maxLife;      // 0 -> 1
            if (l.display) {
                l.display->setPosition((int)l.baseX,
                    (int)(l.baseY - l.rise * (1.0f - (1.0f - t) * (1.0f - t))));
                // On the frame a label's life crosses 0, life/maxLife is
                // negative; casting a negative float to uint8_t is UB
                // (float-cast-overflow). Clamp the alpha to [0, 255] first.
                const float alpha =
                    std::clamp(255.0f * (l.life / l.maxLife), 0.0f, 255.0f);
                l.display->setColor({255, 255, 255, (uint8_t)alpha});
            }
        }
        labels_.erase(std::remove_if(labels_.begin(), labels_.end(),
            [](const Label& l) { return l.life <= 0.0f; }), labels_.end());
    }

    void render(Renderer* renderer) {
        for (Label& l : labels_) {
            if (l.display) l.display->render(renderer);
        }
    }

    bool empty() const { return labels_.empty(); }

private:
    struct Label {
        std::shared_ptr<TextDisplay> display;
        float baseX = 0.0f, baseY = 0.0f;
        float rise = 26.0f;
        float life = 0.9f, maxLife = 0.9f;
    };
    std::vector<Label> labels_;
};

// ---- Ship respawn + invulnerable blink --------------------------------------
// The classic arcade "lost a life" beat: the ship is hidden for a short
// respawn moment, then becomes invulnerable and blinks (so it can't be
// double-killed the instant it reappears, but the player still sees it coming
// back). Timing-only state machine - games call hittable() before dealing
// damage and visible() while drawing. Edge-triggered: update() returns true
// exactly on the frame the respawn beat completes, for a "Respawned" message.
class ShipRespawn {
public:
    ShipRespawn(float respawnSeconds = 1.2f, float invulnSeconds = 2.0f,
                float blinkHz = 20.0f)
        : respawnTime_(respawnSeconds), invulnTime_(invulnSeconds),
          blinkHz_(blinkHz) {}

    void reset() {
        respawnTimer_ = 0.0f;
        invulnTimer_ = 0.0f;
        blinkClock_ = 0.0f;
    }

    // The ship died but the game continues: begin the hidden respawn beat.
    void start() {
        respawnTimer_ = respawnTime_;
        invulnTimer_ = 0.0f;
    }

    // Grant invulnerability directly (power-ups, test hooks, spawn grace).
    void grant(float seconds) {
        respawnTimer_ = 0.0f;
        invulnTimer_ = std::max(invulnTimer_, seconds);
    }

    // Advance the timers; returns true on the frame the respawn beat ends
    // (the ship just became invulnerable and visible again).
    bool update(float dt) {
        blinkClock_ += dt;
        bool justRespawned = false;
        if (respawnTimer_ > 0.0f) {
            respawnTimer_ -= dt;
            if (respawnTimer_ <= 0.0f) {
                invulnTimer_ = invulnTime_;
                justRespawned = true;
            }
        }
        if (invulnTimer_ > 0.0f) invulnTimer_ -= dt;
        return justRespawned;
    }

    bool waiting() const { return respawnTimer_ > 0.0f; }     // hidden
    bool invulnerable() const { return invulnTimer_ > 0.0f; } // blinking
    // True while the ship can take damage (not waiting, not invulnerable).
    bool hittable() const { return !waiting() && !invulnerable(); }
    // True when the ship should be drawn this frame.
    bool visible() const {
        if (waiting()) return false;
        if (!invulnerable()) return true;
        return ((int)(blinkClock_ * blinkHz_) % 2) == 0;
    }
    float remaining() const { return std::max(respawnTimer_, invulnTimer_); }

private:
    float respawnTime_, invulnTime_, blinkHz_;
    float respawnTimer_ = 0.0f, invulnTimer_ = 0.0f, blinkClock_ = 0.0f;
};

// ---- Split-on-hit entities ---------------------------------------------------
// The Asteroids rock pattern: an entity at tier > 0 breaks into two children
// of tier-1 on death, each inheriting a fraction of the parent's momentum plus
// a fresh deterministic split impulse (so splits feel physical but replay the
// same every run). Games own their entity vectors (the kit can't know their
// fields); this helper owns only the deterministic placement of children:
//
//   if (split.splits(rock.size)) {
//       for (int i = 0; i < 2; ++i) {
//           const auto c = split.child(rock.vx, rock.vy, 0.7f, 4.0f, 7.0f);
//           spawnChild(rock.size - 1, rock.x, rock.y, c.vx, c.vy, c.seed, c.spin);
//       }
//   }
class SplitOnHit {
public:
    explicit SplitOnHit(uint32_t seed = 0x5EEDBEEFu) : rng_(seed) {}

    // A hit entity at this tier breaks into children (tier 0 is atomic).
    static bool splits(int tier) { return tier > 0; }
    static int childTier(int tier) { return std::max(0, tier - 1); }

    struct ChildParams {
        float vx, vy;   // velocity: parent momentum + split impulse
        float spin;     // rotation rate for the child's shape
        uint32_t seed;  // shape seed (silhouette, color index, ...)
    };

    // Deterministic child placement. Call once per child, in order: the same
    // sequence of calls always yields the same children.
    ChildParams child(float parentVx, float parentVy, float momentumFactor,
                      float minImpulse, float maxImpulse,
                      float spinRange = 1.2f) const {
        const float ang = randf() * 6.2831853f;
        const float sp = minImpulse + randf() * (maxImpulse - minImpulse);
        ChildParams c;
        c.vx = parentVx * momentumFactor + std::cos(ang) * sp;
        c.vy = parentVy * momentumFactor + std::sin(ang) * sp;
        c.spin = (randf() - 0.5f) * 2.0f * spinRange;
        c.seed = next();
        return c;
    }

private:
    uint32_t next() const {
        rng_ = rng_ * 1664525u + 1013904223u;
        return rng_;
    }
    float randf() const { return (float)(next() & 0xFFFFu) / 65535.0f; }
    mutable uint32_t rng_;
};

// ---- Projectile pool ----------------------------------------------------------
// A managed projectile collection for shooters: spawn with a life, advance,
// optionally wrap into a [0, wrapW) x [0, wrapH) court, and cull expired or
// killed projectiles. The `tag` field is a game-defined owner/faction/color
// index, so one pool can carry player shots AND enemy bombs. This is a plain
// vector (arcade counts are small), not a free-list - kill/lifetime expiry
// remove in place.
//
//   if (shots.fire(x, y, vx, vy, 1.6f)) sfx.play(uj::Sfx::Shoot);
//   shots.update(dt, GRID_W, GRID_H);                 // move + wrap + cull
//   for (const auto& p : shots.all()) { /* hit test */ shots.kill(i); }
class ProjectilePool {
public:
    struct Projectile {
        float x, y, vx, vy;
        float life = 0.0f;
        int tag = 0;
    };

    // Spawn one projectile. Returns false (spawning nothing) when the cap is
    // reached, so games can gate their recoil/SFX on success.
    bool fire(float x, float y, float vx, float vy, float life, int tag = 0) {
        if (cap_ > 0 && (int)ps_.size() >= cap_) return false;
        Projectile p;
        p.x = x;
        p.y = y;
        p.vx = vx;
        p.vy = vy;
        p.life = life;
        p.tag = tag;
        ps_.push_back(p);
        return true;
    }

    // Advance every projectile, wrap into [0, wrapW) x [0, wrapH) when both
    // are > 0, and cull any whose life has expired. Pass no bounds for
    // free flight (e.g. a projectile that leaves the screen and dies).
    void update(float dt, float wrapW = -1.0f, float wrapH = -1.0f) {
        const bool wrap = wrapW > 0.0f && wrapH > 0.0f;
        for (Projectile& p : ps_) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.life -= dt;
            if (wrap) {
                while (p.x < 0.0f) p.x += wrapW;
                while (p.x >= wrapW) p.x -= wrapW;
                while (p.y < 0.0f) p.y += wrapH;
                while (p.y >= wrapH) p.y -= wrapH;
            }
        }
        ps_.erase(std::remove_if(ps_.begin(), ps_.end(),
            [](const Projectile& p) { return p.life <= 0.0f; }), ps_.end());
    }

    // Remove one projectile by index (call from a hit-detection loop).
    void kill(size_t i) { ps_.erase(ps_.begin() + (long)i); }

    const std::vector<Projectile>& all() const { return ps_; }
    size_t size() const { return ps_.size(); }
    bool empty() const { return ps_.empty(); }
    void clear() { ps_.clear(); }
    void setCap(int cap) { cap_ = cap; }
    int cap() const { return cap_; }

private:
    std::vector<Projectile> ps_;
    int cap_ = 0;  // 0 = unlimited
};

}  // namespace uj
