# Whack-a-Mole — game #16

The arcade classic: moles pop up from a 3×3 warren at a pace that ramps as
you score. Whack them before they hide.

- **Core loop** — every mole pays 10 × combo; a mole that escapes costs a
  life and resets the combo. Lose all three lives → game over; reach 25
  whacks → win. The spawn rate and the mole's visible window both tighten
  as your score climbs, and at higher scores a second mole can pop
  simultaneously.
- **GameJuice from day one** — whack bursts with shake + hit-stop + rising
  `+N` popups, mole-pop dirt puffs, escape misses, the game-over explosion
  with a gold `NEW BEST!` celebration, and a confetti win fanfare. All
  sound is synthesized in memory — identical native / WASM / headless.
- **Deterministic** — spawns flow through a fixed-seed LCG, so every run
  reproduces exactly for tests, CI, and LLM agents.
- **LLM surface** — `move_up/down/left/right`, `whack`, `restart`; state
  exposes the warren (`0` empty / `1` mole up), per-hole timers,
  score/best/lives/combo and the cursor. The moles an agent sees are
  exactly the moles in state.
- **Autopilot** — reads the visible warren and whacks any up mole, walking
  the cursor to the nearest one; with the engine's smoke auto-restart it
  loops through hits, escapes, game overs, and wins headlessly.

Controls: click a hole (or arrows + Space/Enter) · P pause · R restart.
