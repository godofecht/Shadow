# Pac-Man

The maze-chomping classic — game #13 of the 100-game program. Eat every
pellet in a 24×15 maze before the ghosts get you. Movement is authentic
tile-center steering: you only turn at the center of a tile.

## Play

```bash
make game GAME=PacMan          # or: cmake --build build-games --target PacMan_game
./build-games/PacMan_game
```

## Controls

| Key | Action |
|-----|--------|
| Arrows / WASD | steer |
| P | pause |
| R | restart |

The LLM interface mirrors these exactly: `move_up`, `move_down`,
`move_left`, `move_right`, `restart`.

## The ghosts (real scatter/chase AI)

- **Red** chases you directly; **pink** ambushes 4 tiles ahead; **cyan**
  targets 2 ahead; **orange** is a coward (scatters when you get close).
- Scatter/chase cycles: 4s scatter → 20s chase → repeat, on a fixed clock.
- Ghosts never reverse mid-corridor (classic rule); frightened ghosts pick
  random turns; eaten ghosts zip home as eyes and re-emerge.
- **Power pellets** flip nearby ghosts blue: they reverse, slow down, and can
  be eaten for 200 / 400 / 800 / 1600 in a combo. The pink door is a gap
  ghosts pass but you can't.

## Rules

- Pellet = 10, power pellet = 50. Clear all 197 pellets to win the maze.
- Three lives; losing one to a ghost resets the round (pellets stay eaten).

## Feel (GameJuice — from day one)

Every chomp throws a sparkle with a rate-limited chomp; power pellets surge
gold with a shake + hit-stop; eating a ghost bursts its color with a floating
combo score; dying is a full explosion with a gold `NEW BEST!` celebration;
clearing the maze fires a confetti fanfare. All sound is synthesized in
memory (zero asset files) — see `Engine/Core/GameJuice.h`.

## LLM view

`state.stats` exposes `pac_x`, `pac_y`, `pac_dir`, `pellets_left`,
`power_left`, `frightened`, `eaten`, `ghosts`, the nearest threat's
`nearest_ghost_x` / `nearest_ghost_y` / `nearest_ghost_dist`, `lives`,
`score`, `best`, and the juice state `paused` / `frozen` / `particles`.
`state.entities` lists the player plus `ghost_0..3`.

## Determinism

The frightened-turn RNG is a fixed-seed LCG and the maze is fixed, so a given
run is fully reproducible — for tests, CI, and LLM agents alike.

## Headless / CI

```bash
PONG_SMOKE=1 SDL_VIDEODRIVER=dummy timeout 10 ./build-games/PacMan_game
```

The autopilot BFS-routes to the nearest pellet at every tile center, so the
dummy-driver run genuinely chomps its way around the maze. Headless unit
tests live in `tests/test_games.cpp`.
