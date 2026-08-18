# Tetris

The block-stacking classic — game #9 of the 100-game program. A 10×20
playfield, the seven tetrominoes dealt from a **7-bag**, SRS rotation with
**wall kicks**, gravity that ramps with level, a **hold queue**, a ghost piece,
and guideline scoring (100/300/500/800 × level for 1/2/3/4 lines).

## Play

```bash
make game GAME=Tetris            # or: cmake --build build-games --target Tetris_game
./build-games/Tetris_game
```

## Controls

| Key | Action |
|-----|--------|
| A / D or arrows | move piece |
| S / down | soft drop (hold for continuous) |
| SPACE / W / up | hard drop |
| 1 | rotate CW |
| 2 | rotate CCW |
| 3 | hold (once per piece) |
| P | pause |
| R | restart |

The LLM interface mirrors these exactly: `move_left`, `move_right`,
`soft_drop`, `hard_drop`, `rotate_cw`, `rotate_ccw`, `hold`, `restart`.

## Rules

- Pieces are dealt from a shuffled 7-bag, so every 7 pieces contain one of
  each tetromino — no long droughts.
- SRS rotation with wall kicks: pieces can rotate next to walls and stacks.
- Level up every 10 lines; gravity speeds up ~15% per level.
- The ghost shows where the piece will land.
- Hold stores one piece for later; you get one hold per piece.

## Feel

Ships to the AAA-feel bar via `Engine/Core/GameJuice.h` — all synthesized in
memory, so it runs identically native, WASM, and headless:

- **Line-clear flash** — every cleared row throws a colored particle burst
  across the full width; a TETRIS throws four.
- **Shake + hit-stop** — clears shake the board and freeze the world for a
  beat, scaled to 1/2/3/4 lines.
- **Floating score** — `Single +100` / `Double +300` / `Triple +500` /
  `TETRIS! +800` (× level) rises from the cleared rows.
- **SFX** — `Ping` on rotate, `Coin` on hold, `Thock` on lock, a fanfare on
  clear (`Win` arpeggio for a TETRIS).

## LLM view

The board is exposed as a real grid (`state.grid`, 20×10, values 1–7 = piece
types, 0 = empty) plus `state.stats` for `current_piece`/`current_x`/
`current_y`/`current_rot`, `hold`, `next1..3`, `score`, `lines`, `level`,
`stack_height`, and the juice state (`paused`, `frozen`, `particles`,
`shake`). An agent can see exactly what a player sees.

## Determinism

The bag shuffle flows through a fixed-seed LCG, so a given playthrough is
fully reproducible — for tests, CI, and LLM agents alike.

## Headless / CI

```bash
PONG_SMOKE=1 SDL_VIDEODRIVER=dummy timeout 10 ./build-games/Tetris_game
```

The autopilot is a greedy placement solver (try every rotation × column, score
by cleared lines / stack height / holes / bumpiness, apply the best) — so the
dummy-driver run genuinely rotates, kicks, locks, and clears lines. Headless
unit tests live in `tests/test_games.cpp`.
