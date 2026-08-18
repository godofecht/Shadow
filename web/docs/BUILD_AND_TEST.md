# BrainRot Engine - Build and Test Instructions

## Prerequisites

### macOS
Install SDL2 libraries using Homebrew:
```bash
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

## Building

### Build the Game Engine and Tests
```bash
cd Shadow
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DMYGAME_VENDORED=OFF
cmake --build . -j8
```

This will create two executables:
- `sdl_app` - The game engine
- `sdl_app_tests` - The test suite

## Running

### Run the Game Engine
```bash
cd build
./sdl_app
```

Note: The game engine requires a graphical display to run.

### Run the Examples
Four example games are included demonstrating 2D tile-based gameplay:

```bash
# Snake - Classic snake game
./snake_example

# Minesweeper - Mine-sweeping puzzle game
./minesweeper_example

# Tic Tac Toe - Two-player strategy game
./tictactoe_example

# Roguelike - Dungeon crawler with combat
./roguelike_example
```

See `Examples/README.md` for detailed instructions on each game.

### Run the Tests
```bash
cd build
./sdl_app_tests
```

Or using CTest:
```bash
cd build
ctest --output-on-failure
```

## Test Coverage

The test suite covers:

### Geometry Tests (8 tests)
- Point2D creation and operations
- Vector2D creation, normalization, and operations
- Rect creation, points calculation, and updatePoints

### Physics Tests (4 tests)
- World creation
- RigidBody creation
- PhysicsManager creation
- Body setters/getters

### Perlin Noise Tests (6 tests)
- Single point noise generation
- Noise range validation
- Noise continuity
- Noise map generation and dimensions
- Interpolation

## Project Structure

```
Shadow/
├── CMakeLists.txt          # Main CMake configuration
├── main.cpp                # Game entry point
├── SDLApp.cpp/h            # Game application class
├── Scene.cpp/h             # Scene management
├── Sprite.cpp/h            # Sprite rendering
├── Renderer.h              # Renderer class
├── Physics.h               # Box2D physics wrapper
├── Geometry.h              # Geometry primitives
├── TextWriter.h            # Text rendering (cross-platform)
├── AssetManager.h          # Asset management
├── tests/
│   ├── test_main.cpp       # Test runner main
│   ├── test_main.h         # Test framework
│   ├── test_geometry.cpp   # Geometry tests
│   ├── test_physics.cpp    # Physics tests
│   └── test_perlin_noise.cpp  # Perlin noise tests
└── build/
    ├── sdl_app             # Game executable
    └── sdl_app_tests       # Test executable
```

## Troubleshooting

### SDL2 Libraries Not Found
If you get errors about SDL2 libraries not being found:
```bash
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

### Build Errors
If you encounter build errors, try cleaning the build directory:
```bash
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DMYGAME_VENDORED=OFF
cmake --build . -j8
```

### Test Failures
If tests fail, check that:
1. All required headers are included
2. The Box2D API matches the version installed
3. No floating-point precision issues in comparisons
