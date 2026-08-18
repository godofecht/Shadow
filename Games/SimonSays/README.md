# Simon Says — Game #15

The memory classic with the GameJuice kit baked in from day one.

Four colored tiles (red, blue, green, yellow) light up in a growing random
sequence, each with its own musical note (A, C, E, G — the kit's new
`Sfx::Note1..Note4`). Watch the sequence, then repeat it exactly: one tile at
a time, in order. A wrong tile ends the run; a correct full replay grows the
sequence by one. Reach round 10 to win.

## Feel (GameJuice)

- Every playback flash **glows the tile** and throws a colored spark with its
  note — the four pitches are the game's core mechanic.
- Your presses chime the matching note with a brief white press-flash.
- A completed round pays a rising **`+N`** (10 × round) with the clear
  fanfare, a small shake, and a spark fountain.
- A wrong press **slams the board**: heavy shake, hit-stop, a red burst, the
  falling lose tone — and a gold **`NEW BEST!`** celebration on a session
  record.
- A win fires a four-color confetti fanfare with **`YOU WIN!`**.
- Pause (P) with a veil; session best in the HUD. All sound is synthesized
  in memory, so it runs identically native, WASM, and headless.

## Play

- Click a tile, or press **1–4** (red, blue, green, yellow).
- **P** pause · **R** restart after a game over / win.
- Build with: `make games && ./build/bin/SimonSays_game`

## LLM actions

`press_red` / `press_blue` / `press_green` / `press_yellow` share the exact
`pressTile()` the mouse and keys drive; `restart` starts a fresh run.
`getState()` exposes the score/best/round, the phase (`0` showing / `1`
input), your input progress, the tile currently lit — and the **sequence
itself** (`seq_0..seq_n-1`), so an agent reads the flashes and replays them
like a human.

## Headless

The smoke autopilot has perfect recall: it reads the current sequence and
presses the exact next required tile, so it replays every round headlessly
and wins at round 10. The same code path is exercised by the CI sanitizer
and valgrind runs. Run it locally:

```bash
PONG_SMOKE=1 ./build/bin/SimonSays_game   # autopilot replays + wins
```
