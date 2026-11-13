# Shadow

Shadow Engine: build 2D games fast and keep the engine understandable.

**Read the project website:** [Open the Shadow Engine documentation site](https://godofecht.github.io/Shadow/)

<p align="center">
  <img src="docs/assets/umbra-eclipse.webp" width="176" alt="Shadow Engine logo: a total solar eclipse, a black disc ringed by a thin white glow on a black background">
</p>

## What this is

Shadow is a game engine: a library of code that handles the window, drawing, sound, physics and keyboard input, so a game only has to describe its own rules. It is written in C++17 and built on SDL2 (a library that opens windows and reads input on every major operating system), Box2D (a 2D physics library) and SDL_mixer (an audio library). The repository holds the engine, about 50 small example programs, and a growing set of finished arcade games such as Pong, Tetris, Breakout and Pac-Man. Each game can also be driven by a large language model (LLM), the kind of AI behind chat assistants, through the same actions a human player uses.

## What you can learn from it

Computer Science:

- How a game loop works: read input, update the world, draw a frame, repeat.
- How object-oriented programming is used in practice. Every game is a class that inherits from `Game2D` and overrides a few methods.
- How a grid stores a game world, and how flood fill reveals cells in Minesweeper.
- How a program is built from source with CMake (a tool that writes the build instructions for your compiler).
- How unit tests (small programs that check one piece of code gives the right answer) protect a large project from mistakes.
- How a game can expose its state and actions so an AI agent can play it.

Maths and Physics:

- Vectors, velocity and reflection, in Pong, Breakout and Asteroids.
- Perlin noise (a way to make smooth random values) for generating terrain.
- Gravity and collision in the Box2D examples.

## Run it

The website above has the documentation and a gallery of the examples. The games themselves run on your own computer.

### On your own computer

**You will need:**

- A C++17 compiler. On macOS, install the Xcode Command Line Tools with `xcode-select --install`. On Linux, install `g++` from your package manager.
- [CMake](https://cmake.org/download/) version 3.22 or newer.
- The SDL2 libraries: SDL2, SDL2_image, SDL2_ttf and SDL2_mixer.
- [Git](https://git-scm.com/downloads) to download the code.

The project's automated tests build it on Linux (Ubuntu 24.04), macOS 14 and Windows Server 2022 for every change.

1. Install the libraries.

   On macOS, with [Homebrew](https://brew.sh/):

   ```bash
   brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer cmake
   ```

   On Debian or Ubuntu:

   ```bash
   sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev cmake
   ```

2. Download the code.

   ```bash
   git clone https://github.com/godofecht/Shadow.git
   cd Shadow
   ```

3. Build everything: the engine, the examples, the games and the tests. The first build takes several minutes.

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   ```

4. Run the default demo. A 700 by 700 window opens showing a tile map made from Perlin noise. It has no controls.

   ```bash
   ./build/sdl_app
   ```

5. Run one of the example games.

   ```bash
   ./build/snake_example
   ./build/minesweeper_example
   ./build/tictactoe_example
   ./build/roguelike_example
   ```

6. Run one of the finished games from the `Games/` folder. Each one builds to a program named after its folder, such as `Pong_game` or `Tetris_game`.

   ```bash
   ./build/Pong_game
   ./build/Tetris_game
   ```

7. Run the tests. Every test should report as passed.

   ```bash
   ctest --test-dir build --output-on-failure
   ```

### Controls

Every example game below uses **P** to pause and **R** to restart.

| Game | Controls |
|---|---|
| Snake | Arrow keys or W A S D to turn |
| Minesweeper | Left-click to reveal, right-click to flag |
| Tic-tac-toe | Click a cell, or arrow keys then Enter or Space |
| Roguelike | Arrow keys or W A S D to move |
| Pong | W and S move the left paddle, Up and Down move the right paddle, Space serves |
| Tetris | A and D or Left and Right to move, S or Down for soft drop, Space, W or Up for hard drop, 1 and 2 to rotate, 3 to hold |
| Breakout | A and D, the arrow keys or the mouse move the paddle, Space launches the ball |

Each folder in `Games/` has its own `README.md` with that game's controls.

### Make your own game

The repository has a starter game called Coin Collector. One command copies it into a new folder for you. Pick a name that is not already used in `Games/`.

```bash
make new-game GAME=MyGame
make game GAME=MyGame
./build-games/MyGame_game
```

`make new-game` creates `Games/MyGame/`. CMake finds every folder in `Games/` that has a `main.cpp`, so you do not edit any build files. [GAME_DEV_GUIDE.md](GAME_DEV_GUIDE.md) walks through the template.

## How it works

A Shadow game is a C++ class. The simplest kind inherits from `Game2D`, which gives you a grid of tiles, entities that move on the grid, and a way to bind keys to functions. The engine runs the loop: it reads input, calls your update code with the time since the last frame, and draws the result. Here is a whole program that puts a blue square on a grid and moves it with the arrow keys:

```cpp
#include "Engine/Core/Game2D.h"

class MyGame : public Game2D {
public:
    MyGame() : Game2D("My Game", 800, 600, /*tileSize=*/20) {}

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

The classes fit together like this:

- `Game` (in `Engine/Core/SDLApp.h`) owns the window, the main loop, physics, audio and loaded files.
- `Game2D` builds on `Game` and adds the grid, grid entities, input bindings, on-screen text and the LLM interface.
- `LLMPlayable` (in `Engine/Core/GameState.h`) lets a game describe its state as text or JSON and list the actions it accepts. A human key press and an AI action call the same method, so both play the same game.

| File or folder | What is in it |
|---|---|
| `main.cpp` | The default demo, a Perlin noise tile map |
| `Engine/Core/` | The game loop, `Game2D`, input, physics, UI and the LLM interface |
| `Engine/Core/GameJuice.h` | Particles, screen shake, hit-stop, floating text and generated sound effects |
| `Engine/Audio/` | Sound and music playback |
| `Engine/EntityAndScene/` | Scenes, sprites, grids and tile maps |
| `Engine/Rendering/` | The renderer and particle system |
| `Examples/` | About 50 small programs, one feature each |
| `Games/` | Finished games, plus starter templates in folders that begin with `_` |
| `tests/` | Unit tests |
| `tools/` | Scripts for new games, web builds and checks |
| `dependencies/` | Copies of SDL2, Box2D, RtAudio and other libraries |

## Ideas to try

1. Make Snake faster or slower. In `Examples/Snake/main.cpp`, change `MOVE_START` (seconds per step at the start) and `MOVE_RAMP` (how much faster it gets per food).
2. Change the size of the default demo's map. In `main.cpp`, the call `generatePerlinNoiseMap(20, 20)` sets the grid size.
3. Make Pong a longer match. `Games/Pong/README.md` says first to 7 wins. Change `WIN_SCORE` in `Games/Pong/main.cpp`.
4. Start your own game with `make new-game GAME=MyGame`, then follow the five marked edits at the top of `Games/MyGame/main.cpp`.
5. Pick an unbuilt game from the list in `GAMES.md`, such as Connect Four or Hangman, and build it from the template.
6. Write a program that plays one of the games through the `LLMPlayable` interface in `Engine/Core/GameState.h`. `Examples/llm_test.cpp` shows the interface in use.

## Technical details

### Engine features

- Game loop and windowing: `Game` base class (`Engine/Core/SDLApp.h`) with window, renderer, physics, audio, asset manager, scene list and FPS limiting.
- Grid-based games: `Game2D` (`Engine/Core/Game2D.h`) with tile grid, entities, bindable input, UI helpers and a delta-time game loop. Optional overrides are `updateGame(float dt)`, `renderGame()` and `onGameOver()`.
- LLM-playable games: the `LLMPlayable` interface (`Engine/Core/GameState.h`) exports game state as text or JSON, registers named actions, and lets an LLM or any agent drive the game. Its methods are `getState()`, `executeAction()`, `getAvailableActions()`, `reset()`, `isGameOver()` and `isGameWon()`. `make llm-demo` builds and runs `llm_test_example --demo`. See [LLM_INTERFACE.md](LLM_INTERFACE.md).
- Physics: a Box2D v3 wrapper (`Engine/Core/Physics.h`) for worlds, rigid bodies and collision callbacks.
- Audio: SDL_mixer groups and players (`Engine/Audio/AudioEngine.h`) for WAV chunks and MP3/OGG music.
- Versioned API: `Engine/Version.h`, generated by CMake from `project(sdl_app VERSION ...)`, exposes `UMBRA_VERSION_MAJOR`, `UMBRA_VERSION_MINOR`, `UMBRA_VERSION_PATCH` and `UMBRA_VERSION_STRING`.
- Rendering: sprites with hierarchical parts, tile maps, grid rendering, UI text, buttons and stats, and 2D effects examples (bloom, lighting, particles, meshes, procedural terrain and more).
- Platforms: macOS, Linux, Windows (vendored dependencies) and WebAssembly through Emscripten.

The full API reference is in [DOCS.md](DOCS.md). The class diagram and design notes are in `docs/architecture.md`.

### The 100-game program

[GAMES.md](GAMES.md) lists 100 planned games in tiers, with the engine features each one exercises. A game counts as done only when it has a full start, play, win or lose, restart loop, pause, a session best score, particle and screen-shake feedback, procedural sound, keyboard controls mirrored by LLM actions, a headless smoke run and a web export. The flagship title, Cinderfall, is on hold while the simpler games ship.

New games can also start from the twin-stick or platformer templates:

```bash
make new-game GAME=MyGame VARIANT=twin-stick
make new-game GAME=MyGame VARIANT=platformer
```

`make game GAME=<name>` configures a separate `build-games/` folder with `-DUMBRA_BUILD_EXAMPLES=OFF` and builds only that game. A headless run, with no window, is `SDL_VIDEODRIVER=dummy timeout 10 ./build-games/<name>_game`.

### CMake options

| Option | Default | Description |
|---|---|---|
| `UMBRA_BUILD_EXAMPLES` | `ON` | Build the examples |
| `UMBRA_BUILD_GAMES` | `ON` | Build every game in `Games/` |
| `UMBRA_BUILD_TESTS` | `ON` | Build the unit test suite |
| `UMBRA_WERROR` | `ON` | Treat compiler warnings as errors |
| `UMBRA_SANITIZE` | `OFF` | Build with ASan and UBSan (GCC or Clang) |
| `UMBRA_SMOKE_CTEST` | `OFF` | Register headless smoke runs as tests |
| `UMBRA_MEMCHECK_CTEST` | `OFF` | Register valgrind memcheck runs as tests |
| `MYGAME_VENDORED` | `OFF` | Use vendored SDL2 (Windows) |
| `USE_VENDORED_RTAUDIO` | `OFF` | Use vendored RtAudio |

On Windows, `vcpkg.json` lists the SDL2 packages for a vcpkg manifest build.

### Tests

```bash
ctest --test-dir build --output-on-failure
# or run the test programs directly:
./build/sdl_app_tests
./build/sdl_app_backend_tests
```

[BUILD_AND_TEST.md](BUILD_AND_TEST.md) covers headless smoke tests, valgrind memcheck, callgrind profiling and the sanitizer builds. These checks need no build:

```bash
make check-games
make check-examples
make check-layout
```

`dependencies/` is the only allowed location for vendored libraries. A root-level copy of any vendored tree (for example a stray `sdl/` or `box2d/`) fails every CI job, the CMake configure step and `make check-layout`.

### WebAssembly

With [Emscripten](https://emscripten.org/) installed:

```bash
emcmake cmake -S . -B build_wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build_wasm -j
# or build every example and every game in Games/ into web/:
./build_wasm.sh
# the same through make, using the local emsdk config and bin_shims:
make wasm
# export one game:
./tools/build_web_game.sh Pong
```

Examples land at `web/*.html` and games at `web/games/<name>/`. The compiled `.js` and `.wasm` files are not committed, so the pages in `web/` only work after you build them.

### Project layout

```text
├── main.cpp               # Entry point / default demo
├── Games/                 # The 100-game program: auto-registered games
│   ├── _template/         #   collector starter (default new-game template)
│   ├── _twin_stick/       #   arena-shooter starter (--twin-stick)
│   ├── _platformer/       #   jump-and-run starter (--platformer)
│   └── <name>/            #   one folder per game, no CMake edits
├── tools/                 # new_game.sh, build_web_game.sh, checks
├── Engine/
│   ├── Audio/             # AudioEngine, AudioFile (SDL_mixer + WAV)
│   ├── Core/              # SDLApp (Game base), AssetManager, Game2D,
│   │                      #   Helpers, InputManager, Physics, Script, UI
│   ├── EntityAndScene/    # Scene, Sprite, Grid, TileMap
│   ├── Rendering/         # Renderer, ParticleSystem
│   ├── ResourceHandling/  # Texture
│   └── Text/              # TextWriter
├── Examples/              # Example games and 2D rendering demos
├── tests/                 # Unit tests (CTest)
├── docs/                  # Source for the documentation website (MkDocs)
└── dependencies/          # Vendored SDL2, SDL2_image, SDL_ttf, SDL2_mixer,
                           #   Box2D v3, RtAudio, libgamepad
```

## Project status

[![CI](https://github.com/godofecht/Shadow/actions/workflows/ci.yml/badge.svg)](https://github.com/godofecht/Shadow/actions/workflows/ci.yml)
![Version 1.0.0](https://img.shields.io/badge/version-1.0.0-blue)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C)

Version 1.0.0. See [CHANGELOG.md](CHANGELOG.md).

CI runs on Linux (GCC, ASan and UBSan, valgrind), macOS (Clang), Windows (MSVC) and Emscripten. The latest run on `main` passed.

The 100-game program is in progress. `GAMES.md` marks the shipped games with a tick. Cinderfall is parked. The website hosts documentation only: the games and examples are not playable in the browser there yet.

## Licence

You can use, copy and change this engine for free under the GPL, as long as you share your own game's source code under the GPL too. If you want to sell a game, keep its code private, or earn money from it in any way (ads, subscriptions, in-app purchases), you need a paid commercial licence from the author instead. That licence covers one game and costs 10% of the game's gross revenue.

- [LICENSE](LICENSE) explains the two options and lists third-party licences.
- [LICENSE.GPL-3.0](LICENSE.GPL-3.0) is the full GNU General Public License, version 3 or later.
- [LICENSE.COMMERCIAL](LICENSE.COMMERCIAL) has the commercial terms.

The libraries in `dependencies/` keep their own licences: zlib for the SDL2 libraries, MIT for Box2D and RtAudio, and BSD-3 or GPL-3.0 for hidapi.

## Credits

Written by Abhishek Shivakumar. Built on [SDL2](https://www.libsdl.org/), SDL2_image, SDL2_ttf, SDL2_mixer, [Box2D](https://box2d.org/), RtAudio, AudioFile and libgamepad.
