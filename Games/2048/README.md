# 2048 (#22)

The slide-and-merge number puzzle. Swipe the 4×4 board to slide every tile
as far as it goes; two equal tiles that collide merge into their sum. Each
move spawns a fresh 2 (or occasionally a 4). Reach the **2048** tile to win,
or fill the board with no merges left and lose.

## How to play

- **Arrows** or **WASD** — slide up/down/left/right (swipe/touch works too)
- **P** — pause · **R** — restart
- Reach 2048 to win.

## Scoring & feel

Every merge scores the value of the new tile and fires the GameJuice stack: a
burst in the tile's color, a **pitched chime that rises with the value**
(2→A, 4→C, 8→E, 16→G, then coin/fanfare tones), screen shake and hit-stop
that scale with the size of the merge, and a floating `+N`. Spawns pop a small
spark; the 2048 win fires a confetti fanfare; a loss shakes the board with a
falling tone. All sound is synthesized in memory — identical native / WASM /
headless.

## Headless / LLM

```bash
make game GAME=2048                  # build + play natively
SDL_VIDEODRIVER=dummy timeout 10 ./build-games/2048_game
```

LLM actions mirror the input exactly: `move_up`, `move_down`, `move_left`,
`move_right`, `restart`. `getState()` exposes the full board (0 = empty,
else the tile value), score, best, max tile, moves, and empty-cell count. The
smoke autopilot is a greedy corner-seeking solver (prefer the most merging
move, otherwise push tiles toward the bottom-left), so headless CI plays
slide → merge → spawn → win/lose for its whole window.

Web export: `tools/build_web_game.sh 2048`.
