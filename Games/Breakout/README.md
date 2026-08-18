# Breakout

The classic brick-breaker — game #7 of the 100-game program. Bounce the ball
through a 12×5 brick wall, deflect off your paddle, and don't drop it.

## Play

```bash
make game GAME=Breakout      # or: cmake --build build-games --target Breakout_game
./build-games/Breakout_game
```

## Controls

| Key | Action |
|-----|--------|
| A / D or arrows | move paddle |
| Mouse | move paddle (follows cursor) |
| SPACE | serve / launch |
| P | pause |
| R | restart |

The LLM interface mirrors these exactly: `move_paddle_left`,
`move_paddle_right`, `serve`, `restart`.

## Feel (the AAA-feel reference)

Breakout is the reference implementation of the program's AAA-feel bar —
see `Engine/Core/GameJuice.h`, the shared toolkit every game uses:

- **Particles** — every brick shatter throws a colored spark burst; losing a
  ball bursts at the impact point; clearing the wall fires a 3-color
  celebration.
- **Screen shake** — trauma-based; paddle hits nudge, brick breaks kick, and
  losing a ball or winning slams.
- **Hit-stop** — the world freezes for a beat on brick breaks and on ball
  loss, so impacts have weight.
- **Floating text** — `+10` pops up at every brick; life loss and wall-clear
  labels rise and fade.
- **Procedural SFX** — serve whoosh, paddle ping, brick thock, lose sweep,
  and a win arpeggio, all synthesized in memory (zero asset files).
- **Best score** — a session best that survives restarts, shown in the HUD.

## Rules

- Clear the whole wall to win; 3 lives, lose one when the ball drops.
- The ball speeds up slightly with every paddle hit.
- Where you hit the paddle sets the launch angle (±55° from straight up).

## Headless / CI

```bash
PONG_SMOKE=1 SDL_VIDEODRIVER=dummy timeout 10 ./build-games/Breakout_game
```

The autopilot serves and sweeps the paddle so the dummy-driver run exercises
serve → rally → brick → scoring. Headless unit tests live in
`tests/test_games.cpp`.
