# Example Games for BrainRot Engine

This folder contains 4 example games demonstrating the 2D tile-based capabilities of the BrainRot Engine.

## Games

### 1. Snake (`snake_example`)
Classic snake game where you control a snake that grows longer as it eats food.
- **Controls**: Arrow keys or WASD to move
- **Objective**: Eat food to grow and increase your score
- **Features**: 
  - Snake wraps around screen edges
  - Speed increases as you eat more food
  - Game resets on self-collision

### 2. Minesweeper (`minesweeper_example`)
Classic minesweeper puzzle game.
- **Controls**: 
  - Left-click to reveal a cell
  - Right-click to flag/unflag a cell
  - Press R to restart
- **Objective**: Reveal all non-mine cells without triggering any mines
- **Features**:
  - 20x20 grid with 50 mines
  - Auto-reveal for empty areas
  - Win/lose detection

### 3. Tic Tac Toe (`tictactoe_example`)
Classic tic-tac-toe game for two players.
- **Controls**: 
  - Click to place X or O
  - Press R to restart
- **Objective**: Get 3 in a row before your opponent does
- **Features**:
  - Two-player local gameplay
  - Win detection for rows, columns, and diagonals
  - Draw detection

### 4. Roguelike (`roguelike_example`)
Simple dungeon crawler roguelike game.
- **Controls**: 
  - Arrow keys or WASD to move
  - Press R to restart after death
- **Objective**: Descend deeper into the dungeon, collect gold, and defeat enemies
- **Features**:
  - Procedurally generated dungeon levels
  - Turn-based combat
  - Enemies that chase the player
  - Gold collection
  - Progressive difficulty with each level

## Building

All examples are built automatically when you build the project:

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DMYGAME_VENDORED=OFF
cmake --build . -j8
```

## Running

After building, run any example from the build directory:

```bash
# Snake
./snake_example

# Minesweeper
./minesweeper_example

# Tic Tac Toe
./tictactoe_example

# Roguelike
./roguelike_example
```

## Technical Details

All examples use:
- **TileMap** class for rendering the game board
- **Scene** management for game state
- **AssetManager** for resource handling
- Standard SDL2 input handling for keyboard and mouse
- 700x700 pixel window with tile-based rendering

## Extending the Examples

These examples serve as templates for creating your own games. Key patterns:

1. **Game Class**: Inherit from `Game` base class
2. **onStart()**: Initialize game state
3. **initializeComponents()**: Set up scenes and tilemaps
4. **update()**: Game logic (called every frame)
5. **handleInput()**: Process player input

The TileMap system renders different tile types based on float values in a 2D array, making it easy to create grid-based games.
