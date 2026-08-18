# Space Invaders

The classic marching-invaders shooter — game #8 of the 100-game program. An
11×5 formation of invaders marches side to side, drops a row each time it hits
an edge, and speeds up as it thins out. Clear all 55 to win.

## Play

```bash
make game GAME=SpaceInvaders   # or: cmake --build build-games --target SpaceInvaders_game
./build-games/SpaceInvaders_game
```

## Controls

| Key | Action |
|-----|--------|
| A / D or arrows | move cannon |
| Mouse | move cannon (follows cursor) |
| SPACE | fire |
| P | pause |
| R | restart |

The LLM interface mirrors these exactly: `move_left`, `move_right`, `fire`,
`restart`.

## Feel (GameJuice)

Ships to the AAA-feel bar via `Engine/Core/GameJuice.h`:

- **Explosions** — every invader shatters into a particle burst in its own
  color; saucers burst purple; the cannon bursts cyan when hit; shield chips
  spark green.
- **Screen shake + hit-stop** — kills kick the screen and freeze the world for
  a beat; a lost life or a win slams harder.
- **Floating score** — `+30` / `+10` / saucer payoff pops rise from each kill.
- **Procedural SFX** — shoot blip, explosion boom, saucer coin payoff, lose
  sweep, and a win arpeggio, all synthesized in memory (zero asset files).
- **Best score** — a session best that survives restarts, shown in the HUD.

## Rules

- Three shields sit between you and the invaders — and they **erode**: both
  your shots and enemy bombs chip a cell, so you can blast your own cover away.
- Invaders fire bombs from the bottom of the formation; the fire rate ramps up
  as they die, and the whole formation marches faster too.
- A bonus saucer occasionally flies across the top for 50–300 points.
- Top row is worth 30, the middle two 20, the bottom two 10.
- Lose all three lives, or let the invaders reach your row — game over.

## Determinism

All "random" behavior (which invader fires, saucer direction/points) flows
through a fixed-seed LCG, so a given playthrough is fully reproducible — for
tests, CI, and LLM agents alike.

## Headless / CI

```bash
PONG_SMOKE=1 SDL_VIDEODRIVER=dummy timeout 10 ./build-games/SpaceInvaders_game
```

The autopilot sweeps the cannon and fires continuously so the dummy-driver run
exercises march → bombs → shields → collisions → scoring. Headless unit tests
live in `tests/test_games.cpp`.
