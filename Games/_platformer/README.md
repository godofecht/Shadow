# Platformer Starter (template)

The jump-and-run starting point for `tools/new_game.sh --platformer`.

Copy it (via the scaffold script) and redesign the level in `buildLevel()`
and the feel at the top of the class.

Patterns demonstrated:

- **Gravity + jump**: velocity model with downward acceleration, jump
  impulse, and variable jump height.
- **Solid-cell collision**: the player bounding box resolves against the
  grid's solid cells axis-by-axis — slides along walls, lands on
  platforms, never tunnels.
- Patrol enemies you can stomp (bounce) or be hurt by (side contact),
  invulnerability frames, respawn point.
- A goal to reach -> win; lives -> lose; restart-cleans-everything.

Headless smoke test: `PONG_SMOKE=1 SDL_VIDEODRIVER=dummy` auto-runs
toward the goal with auto-jumps so CI can exercise gravity and collision
without a display.
