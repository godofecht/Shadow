# Flappy Bird

The one-button gravity classic — game #10 of the 100-game program. The bird
has a fixed x and a y velocity: gravity accelerates it downward every frame,
and a flap snaps the velocity upward. Pipes scroll in from the right with a
procedurally placed gap; pass one to score, touch anything to die.

Ships to the AAA-feel bar: every flap throws a white hop puff, passing a pipe
pops a gold `+1` with a two-note coin, and dying bursts the bird into a feather
explosion with screen shake, a hit-stop beat, and a falling `Lose` tone — all
synthesized in memory via the shared `GameJuice` kit (zero binary assets).

## Play

```bash
make game GAME=FlappyBird       # or: cmake --build build-games --target FlappyBird_game
./build-games/FlappyBird_game
```

## Controls

| Key | Action |
|-----|--------|
| SPACE / W / up | flap |
| P | pause |
| R | restart |

The LLM interface mirrors this exactly: `flap`, `restart`.

## Rules

- One button, one bird, endless pipes. Gravity does the rest.
- Pass a pipe to score; touch a pipe, the ground, or the ceiling to die.

## LLM view

`state.stats` exposes `bird_x`, `bird_y`, `bird_vy`, the next pipe's
`next_pipe_x` / `next_gap_top` / `next_gap_bottom`, `pipes_ahead`, `score`,
`best`, and the juice state `paused` / `frozen` / `particles` — everything an
agent needs to fly the exact game a human plays.

## Determinism

Pipe gaps come from a fixed-seed LCG, so a given playthrough is fully
reproducible — for tests, CI, and LLM agents alike.

## Headless / CI

```bash
PONG_SMOKE=1 SDL_VIDEODRIVER=dummy timeout 10 ./build-games/FlappyBird_game
```

The autopilot is a bang-bang controller: flap when the bird dips below the
next gap's center, coast when above — it survives comfortably and racks up
score in the dummy-driver run. Headless unit tests live in
`tests/test_games.cpp`.
