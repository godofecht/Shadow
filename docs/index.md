<div class="shadow-hero" markdown>

# Shadow Engine

### Build 2D games fast. Keep the engine understandable.

Shadow is a C++17 game engine built on SDL2, Box2D, and SDL_mixer. It combines a compact native engine with grid gameplay, physics, audio, rendering, UI, WebAssembly builds, and a first-class interface for agent-driven games.

[Get started](getting-started.md){ .md-button .md-button--primary }
[Engine API](DOCS.md){ .md-button }
[View on GitHub](https://github.com/godofecht/Shadow){ .md-button }

</div>

<div class="shadow-grid" markdown>

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

## Where to go next

Start with [Getting Started](getting-started.md) for a native build and your first game. Read [Architecture](architecture.md) to understand how `Game`, `Game2D`, scenes, rendering, physics, audio, and the grid layer fit together. The [Game Development Guide](GAME_DEV_GUIDE.md) covers the shipped game workflow, while the [LLM Interface](LLM_INTERFACE.md) documents agent-controlled gameplay.

The complete public API is indexed in [Engine API](DOCS.md), and the [Examples](Examples/README.md) page points to the engine's runnable demonstrations.
