# Twin Stick Starter (template)

The arena-shooter starting point for `tools/new_game.sh --twin-stick`.

Copy it (via the scaffold script) and replace the placeholder rules with
your real game — the pattern to keep is **one shared movement path**:
keyboard held-keys and LLM actions both call `movePlayerBy()`.

Patterns demonstrated:

- Continuous **fractional** movement with a velocity model, dt-scaled.
- Mouse aim + projectile spawning with a fire cooldown.
- Enemy waves: spawn timer, drift-toward-player AI, despawn on contact.
- Lives, invulnerability frames, win/lose, restart-cleans-everything.

Headless smoke test: `PONG_SMOKE=1 SDL_VIDEODRIVER=dummy` auto-aims and
auto-fires so CI can exercise the physics without a display.
