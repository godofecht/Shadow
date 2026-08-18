# Doodle Jump

The bouncy climber — game #11 of the 100-game program. Bounce up a
procedurally scrolling column of platforms; altitude is your score, and you
only die by missing a platform and falling off the bottom.

## Play

```bash
make game GAME=DoodleJump       # or: cmake --build build-games --target DoodleJump_game
./build-games/DoodleJump_game
```

## Controls

| Key | Action |
|-----|--------|
| A / D or arrows | steer |
| Mouse | steer (follows cursor) |
| P | pause |
| R | restart |

The LLM interface mirrors these exactly: `move_left`, `move_right`, `restart`.

## Platforms

- **Green** — normal bounce.
- **Blue** — moves side to side.
- **Brown** — breakable: one bounce, then gone.
- **Green + yellow coil** — spring: launches you much higher.

Edges wrap around. Higher you climb, the nastier the platform mix gets.

## Feel (GameJuice — shipped from day one)

Every bounce throws dust, springs throw a yellow burst with a bigger shake and
a hit-stop beat, breakable platforms shatter, every 50-height milestone
chimes, and all sound is synthesized in memory (zero asset files) — see
`Engine/Core/GameJuice.h`.

The doodle itself is animated: it **squashes on landing** and **stretches
while rising**, its nose points in the direction it steers, moving platforms
kick a little dust when they bounce off a wall, and falling off the bottom
bursts the doodle at the edge — with a gold `NEW BEST!` celebration when a
run beats your session record.

## LLM view

`state.stats` exposes `player_x`, `player_y` (altitude), `player_vy`, `camera`,
`platforms`, the next landing `target_x`/`target_y`, the juice state
`paused` / `frozen` / `particles` / `squash` / `stretch`, plus `score` and
`best`.

## Determinism

Platform layout flows through a fixed-seed LCG, so a given run is fully
reproducible — for tests, CI, and LLM agents alike.

## Headless / CI

```bash
PONG_SMOKE=1 SDL_VIDEODRIVER=dummy timeout 10 ./build-games/DoodleJump_game
```

The autopilot steers toward the next reachable platform, so the dummy-driver
run genuinely bounces, climbs, and hits springs/breakables/moving platforms.
Headless unit tests live in `tests/test_games.cpp`.
