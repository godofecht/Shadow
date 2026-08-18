# Brick Breaker+ (#19)

Breakout with a power-up economy. Smash the brick wall, catch the capsules
that drop out, and ride the chaos when the ball splits into a barrage.

## How to play

- **A/D** or **arrows** — move the paddle (mouse works too)
- **SPACE** — serve the ball
- **P** — pause · **R** — restart
- Three lives; clear the wall to win.

## Power-ups

Every broken brick has a chance to shed a capsule. Catch it on the paddle:

| Capsule | Color | Effect |
|---------|-------|--------|
| **WIDE!** | magenta | Paddle widens for 12s |
| **MULTI BALL!** | cyan | Every ball splits into three (capped at 8) |
| **SLOW!** | green | Balls slow for 10s (great for tight saves) |
| **+1 LIFE!** | gold | Gain a life (capped at 6) |
| **STICKY!** | orange | Paddle catches the ball so you can re-aim and re-serve (12s) |
| **LASER!** | red | Two bolts burn whole brick columns on their way up |

The ball speeds up a touch on every paddle hit, so rallies ramp even without
the capsules. All six effects stack — a wide, sticky paddle with a slow
six-ball barrage and lasers tearing up columns is the dream state.

## Headless / LLM

```bash
make game GAME=BrickBreakerPlus            # build + play natively
SDL_VIDEODRIVER=dummy timeout 10 ./build-games/BrickBreakerPlus_game
```

LLM actions mirror the keyboard exactly: `move_paddle_left`,
`move_paddle_right`, `serve`, `restart`. `getState()` exposes score, lives,
bricks left, paddle position/width, ball count, active capsule count, and the
expand/slow timers. The smoke autopilot chases the lowest ball and dives for
falling capsules, so headless CI plays serve → rally → brick → drop → catch →
effect → win/lose for its whole window.

Web export: `tools/build_web_game.sh BrickBreakerPlus`.
