# Portal Snake — game #17

The classic Snake — with a twist: the edges are portals. The head slides
off one side and re-enters through the matching mouth on the opposite
side, with a shimmer and a flash at both portals.

- **Core loop** — eat food, grow one segment per bite, speed up as you
  grow. Bite your own body and it bursts; reach length 20 to win. The
  portal twist pays off: eat food within a few seconds of teleporting and
  the bite doubles to **+20** with a gold `PORTAL BONUS!` popup — the
  fastest route to the food is often *through* the wall.
- **GameJuice from day one** — food bites burst green + red sparks with a
  coin chime, shake and hit-stop; portal slides fire a swoosh, double
  spark bursts (exit + entrance), and a white flash ring at the arrival
  mouth; death detonates the snake with heavy shake + hit-stop + a gold
  `NEW BEST!` celebration; wins fire a confetti fanfare. All sound is
  synthesized in memory — identical native / WASM / headless.
- **Deterministic** — food placement flows through a fixed-seed LCG, so
  every run reproduces exactly for tests, CI, and LLM agents.
- **LLM surface** — `up/down/left/right`, `restart`; state reports the
  board grid (0 empty / 1 body / 2 head / 3 food), the head/food
  positions, the portal-bonus window, and the wrap count, so an agent can
  route through the portals like a human.
- **Autopilot** — greedily steers the head toward the food using
  wrap-aware (torus) distances, so headless smoke genuinely uses the
  portals: move → portal → eat → grow → juice (→ death → restart).

Controls: arrows / WASD · P pause · R restart.
