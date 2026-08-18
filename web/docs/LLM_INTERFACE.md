# LLM-Playable Game Engine

This engine now supports LLM-based gameplay through a state-based interface.

## Overview

The engine provides a clean interface for LLMs to:
1. **Read game state** - Get text or JSON representation of the game
2. **Take actions** - Execute named actions programmatically
3. **Query available actions** - Know what moves are valid

## Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   LLM/Agent     │────▶│  LLMPlayable     │────▶│    Game2D       │
│                 │◀────│  Interface       │◀────│    (Snake, etc) │
└─────────────────┘     └──────────────────┘     └─────────────────┘
       │                        │                        │
       │  getState()            │                        │
       │  executeAction()       │                        │
       │  getAvailableActions() │                        │
       ▼                        ▼                        ▼
┌─────────────────────────────────────────────────────────────────┐
│                      GameState                                   │
│  - gameRunning, gameOver, score, level                          │
│  - grid (2D array)                                               │
│  - entities (name → position)                                    │
│  - availableActions                                              │
│  - toString() / toJSON()                                         │
└─────────────────────────────────────────────────────────────────┘
```

## Usage

### For Game Developers

Extend `Game2D` and implement the LLM interface:

```cpp
class MyGame : public Game2D {
    void initGame() override {
        // Register actions for LLM
        registerAction("move_up", [this]() {
            player.move(0, -1);
            return ActionResult{true, "Moved up"};
        });
        registerAction("move_down", [this]() {
            player.move(0, 1);
            return ActionResult{true, "Moved down"};
        });
    }
    
    GameState getState() const override {
        GameState state = Game2D::getState();
        state.score = score;
        state.entities["player"] = {player.x, player.y};
        return state;
    }
};
```

### For LLM Integration

```cpp
// Create game
MyGame game;
game.onStart();
game.initializeComponents();

// Get state for LLM
GameState state = game.getState();
std::string prompt = "Current game state:\n" + state.toString();
prompt += "\nAvailable actions: ";
for (const auto& action : game.getAvailableActions()) {
    prompt += action + " ";
}
prompt += "\nWhat action do you take?";

// Send to LLM, get response
std::string llmResponse = callLLM(prompt);  // Your LLM call
std::string action = parseAction(llmResponse);

// Execute action
ActionResult result = game.executeAction(action);
```

## GameState Format

### Text Output (toString)
```
=== GAME STATE ===
Status: PLAYING
Score: 100
Level: 2

=== GRID ===
....................
..#.................
..#.................
..$.................
....................

=== ENTITIES ===
player: (5, 10)
enemy: (12, 8)
food: (3, 15)

=== AVAILABLE ACTIONS ===
  - up
  - down
  - left
  - right
  - attack
==================
```

### JSON Output (toJSON)
```json
{
  "gameRunning": true,
  "gameOver": false,
  "score": 100,
  "level": 2,
  "gridWidth": 20,
  "gridHeight": 20,
  "availableActions": ["up", "down", "left", "right", "attack"]
}
```

## Example: LLM Playing Snake

```
System: Here's the current game state:
=== GAME STATE ===
Status: PLAYING
Score: 20
Level: 1

=== GRID ===
....................
....................
......O.............
....................
...$................
....................

=== ENTITIES ===
snake_head: (6, 4)
food: (3, 6)

=== AVAILABLE ACTIONS ===
  - up
  - down
  - left
  - right
==================

What action do you take?

LLM: I'll move down and left to approach the food at (3, 6).
Action: down

System: Action result - Moved
New state:
=== GAME STATE ===
...
snake_head: (6, 5)
...
```

## Testing

Run the demo:
```bash
cd build
./llm_test --demo
```

This shows:
1. Initial game state
2. Available actions
3. Action execution
4. State updates
5. JSON output format

## Games with LLM Support

| Game | Executable | Actions |
|------|------------|---------|
| Snake | `snake_example` | up, down, left, right, restart |
| Minesweeper | `minesweeper_example` | reveal, flag, restart |
| Tic Tac Toe | `tictactoe_example` | place_X, place_O, restart |
| Roguelike | `roguelike_example` | up, down, left, right, attack, restart |

## Integration Examples

### Python Integration
```python
import subprocess
import json

# Run game and capture state
process = subprocess.Popen(['./llm_test', '--demo'], 
                          stdout=subprocess.PIPE, 
                          stderr=subprocess.PIPE)

# Parse state
state_output = process.stdout.read().decode()
state = parse_game_state(state_output)

# Send to LLM
response = llm.generate(f"Game state: {state}\nWhat action?")
action = extract_action(response)

# Execute action
result = execute_game_action(action)
```

### Direct C++ Integration
```cpp
#include "Game2D.h"

class LLMGameLoop {
    Game2D* game;
    LLMInterface* llm;
    
    void run() {
        while (!game->isGameOver()) {
            GameState state = game->getState();
            std::string prompt = buildPrompt(state);
            std::string action = llm->generate(prompt);
            game->executeAction(action);
        }
    }
};
```

## Best Practices

1. **Keep state concise** - Only include relevant information
2. **Use clear action names** - "move_up" not "mu"
3. **Provide feedback** - ActionResult messages help LLM learn
4. **Validate actions** - Return success=false for invalid moves
5. **Include grid visualization** - ASCII grids are LLM-friendly
