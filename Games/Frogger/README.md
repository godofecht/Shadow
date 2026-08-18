# Frogger — game #18

The arcade classic: hop a frog across five lanes of traffic and a river of
drifting logs to fill five goal slots.

- **Core loop** — three lives; a car hit or a drowning costs one and
  respawns you at the bank (goals are kept). Fill all five goals to clear
  the level; traffic gets faster and denser each level; beat level 3 to
  win. Goals pay 100 × level plus a streak bonus; a level clear pays a
  250 × level bonus.
- **GameJuice from day one** — hop pops, goal chimes with confetti,
  the car-hit splat (heavy shake + hit-stop + `SPLAT!`), the blue drowning
  splash, the level-clear fanfare, and the win confetti. All sound is
  synthesized in memory — identical native / WASM / headless.
- **Deterministic** — car spawns, car colors, and log patterns flow
  through a fixed-seed LCG, so every run reproduces exactly for tests, CI,
  and LLM agents.
- **LLM surface** — `move_up/down/left/right`, `restart`; state exposes
  the frog position, lives/score/level, the goal slots, and per-lane car
  and log positions, so an agent can plan crossings like a human watching
  the road.
- **Autopilot** — a genuine planner: a time-bucketed BFS over
  (row, col, t) computes a safe hop sequence to an empty goal, accounting
  for where every car and log will be — the same "wait for a gap, then
  dash" strategy a human uses — and replans when drift invalidates the
  path, so headless smoke genuinely plays the whole game.

Controls: arrow keys · P pause · R restart.
