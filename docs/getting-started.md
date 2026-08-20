# Getting Started

Shadow Engine targets C++17 and currently supports macOS, Linux, Windows, and WebAssembly.

## Install dependencies

=== "macOS"

    ```bash
    brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer cmake
    ```

=== "Debian / Ubuntu"

    ```bash
    sudo apt install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev cmake
    ```

Windows builds can use the vendored dependency configuration exposed by the repository's CMake setup.

## Build the engine

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run the default demo:

```bash
./build/sdl_app
```

Run one of the shipped examples:

```bash
./build/snake_example
./build/minesweeper_example
./build/tictactoe_example
./build/roguelike_example
```

## Create a game

The fastest path is the repository's game scaffold:

```bash
make new-game GAME=Pong
make game GAME=Pong
./build-games/Pong_game
```

`make new-game` creates a complete game under `Games/Pong/`. Game directories are auto-registered by the CMake setup, so a new game does not need manual build wiring.

For the full workflow, continue to the [Game Development Guide](generated/game-development.md).

## Minimal Game2D program

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

`Game2D` is the recommended entry point for grid-based games. It owns the grid layer, entity list, input shortcuts, UI helpers, delta-time game loop, and the `LLMPlayable` interface.

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

The repository also contains sanitizer, smoke, layout, example-registration, and platform-specific CI coverage. See [Build & Test](generated/build-and-test.md) for the complete matrix.

## Build for WebAssembly

With Emscripten configured:

```bash
emcmake cmake -S . -B build_wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build_wasm -j
```

Or use the repository helper to build the browser catalog:

```bash
./build_wasm.sh
```

A single shipped game can be exported with:

```bash
./tools/build_web_game.sh Pong
```
