<div class="shadow-hero" markdown>

<div class="shadow-hero-copy" markdown>

<div class="shadow-kicker">C++17 · 2D · Native + WebAssembly</div>

# Shadow Engine

### Build 2D games fast. Keep the engine understandable.

Shadow is a compact C++17 game engine built on SDL2, Box2D, and SDL_mixer. It combines grid gameplay, physics, audio, rendering, UI, WebAssembly builds, and a first-class interface for agent-driven games without burying the game loop under a large framework.

<div class="shadow-actions" markdown>

[Get started](getting-started.md){ .md-button .md-button--primary }
[Demo gallery](gallery.md){ .md-button }
[Engine API](DOCS.md){ .md-button }
[GitHub](https://github.com/godofecht/Shadow){ .md-button }

</div>

</div>

<div class="shadow-hero-visual">
  <img src="assets/umbra-eclipse.webp" alt="Shadow Engine eclipse mark">
</div>

</div>

<div class="shadow-tech-rail">
  <div><strong>SDL2</strong><span>windowing · input · rendering</span></div>
  <div><strong>Box2D</strong><span>physics · contacts · worlds</span></div>
  <div><strong>Game2D</strong><span>grid · entities · UI</span></div>
  <div><strong>WASM</strong><span>browser builds · demos</span></div>
</div>

<div class="shadow-grid" markdown>

<div class="shadow-card shadow-card--lead" markdown>

## One engine surface

The native loop, grid framework, rendering, audio, physics, examples, and agent-facing state model all live in the same codebase. Start small and reach lower-level systems only when the game needs them.

[Read the architecture](architecture.md)

</div>

<div class="shadow-card" markdown>

## Game2D

A high-level grid game framework with entities, input bindings, UI helpers, delta-time updates, and automatic rendering.

</div>

<div class="shadow-card" markdown>

## Native + Web

Build on macOS, Linux, and Windows, then export games and examples to WebAssembly with Emscripten.

</div>

<div class="shadow-card" markdown>

## LLM-playable

Expose game state as text or JSON, register named actions, and let an LLM or any other agent operate the same game interface.

</div>

<div class="shadow-card" markdown>

## Batteries included

Box2D physics, SDL_mixer audio, sprites, tile maps, particles, UI, procedural systems, post effects, and a large runnable example catalog.

</div>

</div>

## A game in one file

```cpp
#include "Engine/Core/Game2D.h"

class MyGame : public Game2D {
public:
    MyGame() : Game2D("My Game", 800, 600, 20) {}

    void initGame() override {
        createGrid(40, 30, 20);

        player = createEntity<GridEntity>(getGrid(), 5, 5);
        player->setColor({0, 200, 255, 255});

        bindKey(KEY_UP).onPress([this]{ player->tryMove(0, -1); });
        bindKey(KEY_DOWN).onPress([this]{ player->tryMove(0, 1); });
        bindKey(KEY_LEFT).onPress([this]{ player->tryMove(-1, 0); });
        bindKey(KEY_RIGHT).onPress([this]{ player->tryMove(1, 0); });
    }

private:
    std::shared_ptr<GridEntity> player;
};

int main() {
    MyGame game;
    game.run();
}
```

<div class="shadow-next" markdown>

## Where to go next

<div markdown>

### See it first
Open the [Demo Gallery](gallery.md) for a visual tour of real rendering, simulation, physics, AI, audio, performance, and complete game examples.

### Build something
Start with [Getting Started](getting-started.md), then use the [Game Development Guide](GAME_DEV_GUIDE.md) for the shipped game workflow.

### Understand the engine
Read [Architecture](architecture.md), the [Engine API](DOCS.md), and the [LLM Interface](LLM_INTERFACE.md) for the system-level view.

</div>

</div>
