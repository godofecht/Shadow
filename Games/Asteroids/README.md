# Asteroids

The drift-and-shoot classic — game #12 of the 100-game program. Your ship is
a point mass: rotation turns it, thrust accelerates it, and there is **no
friction** — momentum carries you forever until you steer. The screen wraps
on all four edges; rocks drift, spin, and split when shot.

## Play

```bash
make game GAME=Asteroids       # or: cmake --build build-games --target Asteroids_game
./build-games/Asteroids_game
```

## Controls

| Key | Action |
|-----|--------|
| A / D or left / right | rotate |
| W / up | thrust |
| SPACE | fire |
| P | pause |
| R | restart |

The LLM interface mirrors these exactly: `rotate_left`, `rotate_right`,
`thrust`, `fire`, `restart`.

## Rules

- A big rock splits into two mediums, a medium into two smalls; scoring is
  20 / 50 / 100 per size.
- Clear the field to advance a level (each with more rocks) for a 1000-point
  bonus. Three lives; every 10,000 points earns an extra.
- A bonus saucer crosses the top now and then — worth 200. After a hit you
  respawn in the middle with a short invulnerable blink.

## Feel (GameJuice — from day one)

Thrust burns a flickering flame with exhaust particles; breaking a rock
shatters it in its own color with screen shake, a hit-stop beat, and a
floating score; losing the ship is a full explosion with a gold `NEW BEST!`
celebration when you beat your session record. All sound is synthesized in
memory (zero asset files) — see `Engine/Core/GameJuice.h`.

## LLM view

`state.stats` exposes `ship_x`, `ship_y`, `ship_angle` (degrees), `ship_vx`,
`ship_vy`, `rocks`, `bullets`, `saucer_active`, the nearest rock's
`nearest_rock_x` / `nearest_rock_y` / `nearest_rock_dist`, `lives`, `level`,
plus `score`, `best`, and the juice state `paused` / `frozen` / `particles`.

## Determinism

Rock spawns, drift, spin, and the saucer all flow through a fixed-seed LCG,
so a given run is fully reproducible — for tests, CI, and LLM agents alike.

## Headless / CI

```bash
PONG_SMOKE=1 SDL_VIDEODRIVER=dummy timeout 10 ./build-games/Asteroids_game
```

The autopilot aims at the nearest rock, closes in, and fires — the
dummy-driver run genuinely breaks rocks and scores. Headless unit tests live
in `tests/test_games.cpp`.
