# Cinderfall

A top-down action-adventure roguelite — the flagship of the 100-game program.
Descend the dying kingdom, rekindle the forge, and leave something behind for
the next run. See [DESIGN.md](DESIGN.md) for the long-term plan.

## Play

```bash
make game GAME=Cinderfall      # or: cmake --build build-games --target Cinderfall_game
./build-games/Cinderfall_game
```

## Controls

| Key | Action |
|-----|--------|
| WASD / arrows | move |
| SPACE / J | sword swing |
| K / TAB | dodge roll (i-frames) |
| E | interact (doors / chests) |
| ENTER | start run (title) |
| R | restart |

The LLM interface mirrors these exactly: `move_up/down/left/right`, `attack`,
`roll`, `interact`, `start`, `restart`.

## Rules

- A locked door guards the stairs on every floor.
- The key is in a chest somewhere on the floor.
- Enemies drop gold; gold and hearts are scattered around.
- Reach the stairs to descend; rekindle the final forge to win.

## Bestiary (M2)

| Enemy | Behavior |
|-------|----------|
| Chaser | Hunts you head-on; drops 5 gold |
| Spitter | Keeps distance, spits projectiles; drops 8 gold |
| Brute | Slow, tanky, hits for 2; drops 15 gold |
| Ghost | **Phases through walls** — walls can't stop it, only the map edge; drops 12 gold |
| Turret | Stationary; fires 3-shot volleys aimed at you on a cooldown; drops 10 gold |

Ghosts and turrets only appear on floor 2+.

## Game feel (M1)

- **Screen shake** — trauma builds on hits/kills/damage and eases off over a
  second or two; the world (not the HUD) visibly jolts.
- **Hit-stop** — the whole world freezes for a few frames on impactful hits,
  so swings and kills land with weight.
- **Particles** — ember sparks on hits, blood bursts on kills, gold sparkles on
  pickups. Simulated inside the pure state, so they run headless too.
- **Sound** — procedural SFX (swing / hit / kill / pickup / hurt / door /
  descend / win) are synthesized in memory; the game still ships **zero binary
  assets** and plays the same sounds natively and in WASM. Silently no-ops if
  no audio device is present.

## Headless / CI

```bash
PONG_SMOKE=1 SDL_VIDEODRIVER=dummy timeout 10 ./build-games/Cinderfall_game
```

The autopilot walks toward the stairs, swings and rolls, exercising the full
real-time simulation with no display. Unit tests live in
`tests/test_cinderfall.cpp`.
