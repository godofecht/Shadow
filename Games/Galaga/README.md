# Galaga (#20)

The classic fixed-position shooter. A marching formation of bug fighters
weaves side to side and drops toward your ship — and one at a time, bugs
peel out of formation and dive at you on a looping sine path. Sweep the
bottom, fire up to two shots at a time, and clear three waves to win.

## How to play

- **A/D** or **arrows** — move the ship (mouse works too)
- **SPACE** — fire (two shots in flight, Galaga style)
- **P** — pause · **R** — restart
- Three lives; clear three waves to win.

## The wave ramp

| Wave | Formation | March pace | Dive pace |
|------|-----------|-----------|-----------|
| 1 | 8×4 (32 bugs) | 0.9s/step | 4.0s |
| 2 | 9×4 (36 bugs) | 0.7s/step | 3.0s |
| 3 | 10×5 (50 bugs) | 0.55s/step | 2.2s |

Top rows pay more (100/80/60/40/20); a bug you shoot **while it dives** pays
double. The formation marches faster as it thins, and divers come more often.
Losing a ship runs the full arcade beat: hidden respawn, then an invulnerable
blink (the engine's `ShipRespawn` helper).

## Headless / LLM

```bash
make game GAME=Galaga                  # build + play natively
SDL_VIDEODRIVER=dummy timeout 10 ./build-games/Galaga_game
```

LLM actions mirror the keyboard exactly: `move_left`, `move_right`, `fire`,
`restart`. `getState()` exposes score, wave, lives, enemies left, the
formation origin, active-diver count, and the respawn/blink state. The smoke
autopilot sweeps the ship and keeps a two-shot stream in flight, so headless
CI plays march → dive → collision → respawn → wave-clear for its whole
window.

Web export: `tools/build_web_game.sh Galaga`.
