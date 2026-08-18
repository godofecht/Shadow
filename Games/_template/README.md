# Game Template

This directory is the starting point for every game in the
[100-game catalog](../GAMES.md). It contains a complete, playable, LLM-aware
game — *Coin Collector* — so every new game starts from something that
compiles, runs, and is fun to extend.

## Quick start

```bash
# One command creates Games/YourGame/ from this template:
tools/new_game.sh YourGame

# Build and run it (target name = <dir>_game, auto-registered by CMake):
cmake --build build --target YourGame_game
./build/YourGame_game
```

## Anatomy

| File          | Purpose                                                        |
|---------------|----------------------------------------------------------------|
| `main.cpp`    | The whole game: rules, rendering, input, LLM interface         |
| `assets/`     | Drop PNGs / WAVs here (bunnymark and the audio examples show
                 how to load them from the working directory)     |
| `README.md`   | Fill this in per game (controls, rules, screenshots)           |

## Make it yours

The five edits are marked at the top of `main.cpp`. The two patterns the
template demonstrates are non-negotiable for every game in the catalog:

1. **Clear the entity vectors in `initGame()`** — restart re-runs
   `initGame()`, and stale entities would pile up otherwise.
2. **One method per action** — input bindings and LLM `registerAction`
   callbacks share the same code path, so the game can never be played
   differently by a human than by an agent.

See [GAME_DEV_GUIDE.md](../GAME_DEV_GUIDE.md) for the full walkthrough.
