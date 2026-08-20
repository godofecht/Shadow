# Architecture

Shadow keeps the high-level game model small while exposing the lower-level engine subsystems directly when a game needs them.

```text
Game  (SDL window, main loop, subsystems)
 └─ Game2D  (grid-based games + LLM interface)
      ├─ Grid           (tile grid, coordinate mapping)
      │   └─ GridEntity (entities that live on the grid)
      ├─ InputManager2D (bindable key/mouse input)
      ├─ Scene          (sprite collection)
      │   └─ SimpleSprite (renderable object with parts & scripts)
      ├─ PhysicsManager (Box2D world wrapper)
      ├─ AudioEngine    (SDL_mixer groups & players)
      └─ UI             (TextDisplay, Button, GameStats)
```

## `Game`

`Game` is the native engine shell. It owns the SDL window and renderer, the physics manager, audio engine, asset manager, scenes, frame loop, and FPS control.

Use `Game` directly for lower-level rendering demos or games that do not want the grid framework.

Primary hooks include:

```cpp
void onStart() override;
void initializeComponents() override;
void update() override;
void renderPostFX() override;
```

Subsystems are available through accessors such as `getRenderer()`, `getAssetManager()`, `getAudioEngine()`, and `getPhysicsManager()`.

## `Game2D`

`Game2D` extends `Game` with the opinionated pieces needed by a large class of 2D games: a tile grid, grid entities, bindable input, UI helpers, game-state lifecycle, delta time, and the `LLMPlayable` contract.

A `Game2D` implementation provides:

```cpp
void initGame() override;
```

and can optionally provide:

```cpp
void updateGame(float dt) override;
void renderGame() override;
void onGameOver() override;
```

This separation keeps game code focused on game rules instead of the SDL event loop.

## Grid and entities

`Grid` stores cells, values, solidity, visibility, colors, borders, dimensions, and coordinate conversion. `GridEntity` provides the entity model for objects that occupy and move through that grid.

`Game2D::createEntity<T>()` creates entities and registers them with the game so they can participate in the normal update lifecycle.

## Scene and rendering

The scene layer handles sprite collections and renderable objects. The renderer-facing parts of the engine cover sprites, hierarchical sprite parts, tile maps, particles, meshes, procedural rendering, lighting, bloom, and other 2D effects demonstrated in `Examples/`.

The example catalog is intentionally important to the architecture: rendering features are represented by runnable programs rather than documentation-only APIs.

## Physics

`PhysicsManager` wraps Box2D v3 worlds, rigid bodies, and collision callbacks. Games can use grid collision for simple tile logic, Box2D for rigid-body simulation, or combine both depending on the game.

## Audio

`AudioEngine` wraps SDL_mixer and provides grouped audio playback for chunks and music. WAV chunks and MP3/OGG music are supported through the current audio layer.

## Agent interface

The agent interface is not a separate game runtime. A game can implement the same state/action surface used by human input and expose it to an LLM or another controller. This keeps agent-driven play attached to the actual game state rather than a parallel simulation.

See [LLM Interface](generated/llm-interface.md) for the concrete state export and action registration API.

## Project layout

```text
Engine/
├── Audio/
├── Core/
├── EntityAndScene/
├── Rendering/
├── ResourceHandling/
└── Text/

Examples/        runnable engine and game demonstrations
Games/           shipped games and game templates
tests/           CTest suite
tools/           scaffolding, web builds, and validation helpers
dependencies/    canonical vendored third-party dependencies
```

For the complete class and method reference, see [Engine API](generated/api.md).
