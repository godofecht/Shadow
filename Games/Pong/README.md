# Pong

The classic two-player duel, with real continuous ball physics: the ball
moves in fractional cell coordinates with a velocity model (cells/second),
reflects off the top/bottom walls, and the return angle is set by **where**
the ball hits the paddle — hit the edge for a steep shot, the middle for a
flat one. Every paddle hit speeds the ball up (to a ceiling), so rallies
climb in intensity. First to **7** wins.

Game #6 in the [100-game program](../GAMES.md).

## Controls

- **W / S** — left paddle
- **UP / DOWN** — right paddle
- **SPACE** — serve
- **R** — restart (also after game over)

## How to build & run

```bash
make game GAME=Pong
./build-games/Pong_game
```

Web export: `./tools/build_web_game.sh Pong`.

Headless smoke test (used by CI): `PONG_SMOKE=1 SDL_VIDEODRIVER=dummy \
./build-games/Pong_game` — the env var enables autoplay (auto-serve +
robot paddle nudging) so a dummy-driver run exercises the real serve →
rally → deflect → score physics instead of parking in serve-wait.

## How it's put together

- The static court (dark background, dashed center line) is drawn through
  the grid; the ball and paddles are filled rects at rounded cell
  positions — the same primitive `GridEntity::render` uses.
- Physics notes: `MAX_SPEED = 54` cells/s keeps the per-frame step under
  1 cell so the ball can never tunnel through a paddle; wall reflections
  mirror the overshoot so they're exact; the serve angle alternates each
  rally deterministically.
- One code path serves both players: keyboard held-key polling and the LLM
  actions both funnel through `movePaddleBy()`'s clamp, and SPACE / `serve`
  share `doServe()`.

## LLM interface

The game is LLM-playable via `GameState`/actions (see
[GAME_DEV_GUIDE.md](../GAME_DEV_GUIDE.md)):

- `paddle_left_up` / `paddle_left_down` / `paddle_right_up` /
  `paddle_right_down` — move a paddle 4 cells
- `serve` — launch the ball (only valid while waiting)
- `restart` — start a new game

`getState()` reports both scores, the ball's cell, both paddle rows, and
who serves next, plus the grid — enough for an agent to play a full match.
