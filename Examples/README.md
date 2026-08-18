# Examples — the Shadow Engine demo catalog

`Examples/` holds the engine's demo catalog: classic game examples (Snake,
Minesweeper, TicTacToe, Roguelike) and one-dir-per-topic feature demos (2D
rendering, sprites, procedural, effects, audio, GPU, multi-window). Every
`Examples/<dir>/main.cpp` is a standalone binary. The full, always-current
catalog is the list of `add_umbra_example(...)` lines in CMakeLists.txt —
there are ~50 entries; the curated table below is just the headless ones.

| Example | Target | What it shows |
|---|---|---|
| `Snake` | `snake_example` | classic arcade, GameJuice kit |
| `Minesweeper` | `minesweeper_example` | click/tile logic |
| `TicTacToe` | `tictactoe_example` | mouse input, win lines |
| `Roguelike` | `roguelike_example` | procedural dungeons |
| `GameOfLife` | `game_of_life_example` | cellular automata |
| `FixedTimestep` | `fixed_timestep_example` | deterministic stepping |
| `PerlinNoise` | `perlin_noise_example` | engine `PerlinNoiseGenerator` + `TileMap` |
| `TankAndBullet` | `tank_and_bullet_example` | Game2D arena, textures, GameJuice |

---

## Adding an example (the whole workflow)

Unlike `Games/` (which CMake auto-globs), **examples are registered
explicitly** — the directory → target name mapping is hand-curated
(`2DBloom/` → `bloom2d_example`, `ParticleShowcase/` → `particles_example`,
…), so there is no glob rule to derive it. Adding an example is exactly
three things:

**1. Write `Examples/MyExample/main.cpp`** — one class inheriting `Game2D`
(for grid/LLM games, see GAME_DEV_GUIDE.md) or the legacy `Game` class
(for raw demos like Bunnymark). It must compile **warning-free**: every
example target builds with `UMBRA_WARN_FLAGS` (`-Werror` / `/WX`), and CI
rejects warnings. See the "House rules" section of GAME_DEV_GUIDE.md for
the three warnings you'll actually hit.

**2. Register it in CMakeLists.txt** — one line inside the
`if(UMBRA_BUILD_EXAMPLES)` block, next to its topic siblings:

```cmake
add_umbra_example(my_example_example Examples/MyExample/main.cpp)
```

Target names end in `_example`. That's the entire CMake edit — the target
gets the engine link and the warning flags for free. WASM support (the
`.js` suffix + Emscripten link flags) is applied to the curated example
list in the `EMSCRIPTEN` block of CMakeLists.txt — if your example should
ship to the browser, add it there (and to the `emscripten` CI job's build
list), or to `cmake/smoke_targets.txt` for the smoke coverage.

**3. (Optional but recommended) Make it headless-capable and add it to
`cmake/smoke_targets.txt`.** The smoke list is the single source that
drives the `smoke_*` ctest entries, the ASan+UBSan / UBSan-only smoke
passes, the valgrind memcheck example runs, and the browser smoke. Add one
line `my_example_example` to that file and your example is covered by all
of them (games get this automatically via `new_game.sh`; examples do it by
hand). Examples that need a real display or assets can simply skip this —
they stay registered as build/CI-covered demos.

Then build it:

```bash
cmake -S . -B build && cmake --build build --target my_example_example
```

---

## Enforcement: the registration gates

A new `Examples/` directory with no `add_umbra_example()` line ships with
zero build/CI coverage. Three layers make that impossible:

1. **`make check-examples`** (runs `tools/check_example_registration.sh`,
   no build needed) — the fast local gate. It verifies *forward*: every
   `Examples/<dir>/main.cpp` has a matching `add_umbra_example()` line;
   and *reverse*: every `*_example` entry in `cmake/smoke_targets.txt` maps
   to a registered target (a stale smoke entry fails here instead of at
   build time).

   ```bash
   make check-examples      # OK: every Examples/ dir is registered...
   ```

2. **CI** — the `linux` job runs `tools/check_example_registration.sh`
   right after checkout, so a pull request with an unregistered example
   fails in seconds with a full diagnosis.

3. **CMake configure** — the `UMBRA_BUILD_EXAMPLES` block also verifies the
   smoke-list direction at configure time (`FATAL_ERROR` on a stale
   `*_example` entry), so every job's configure step enforces it too.

### The exception list — keep it empty

`tools/check_example_registration.sh` has a `KNOWN EXCEPTIONS` list for
directories that intentionally have no registration. It exists only to
record resolved or in-flight debt; **never add to it** — either register
the example (it must compile warning-free) or delete the directory. All
three historic exceptions (a scratch `DemoRunner` dir, a non-compiling
`PerlinNoise`, a stale `TankAndBullet` main.cpp) were resolved that way.

---

## Where examples get coverage

- **Native**: every CMake build compiles all registered examples
  (`UMBRA_BUILD_EXAMPLES=ON` is the default).
- **WASM**: `make wasm` / `build_wasm.sh` build the full catalog for the
  browser; the `emscripten` CI job builds a representative subset to keep
  CI fast.
- **Smoke / sanitizers / valgrind / browser smoke**: only examples listed
  in `cmake/smoke_targets.txt` — currently the six headless ones in the
  table above (Snake, Minesweeper, TicTacToe, Roguelike, GameOfLife,
  FixedTimestep) — driven through the same shared list as the games.

The other docs you'll want: [GAME_DEV_GUIDE.md](../GAME_DEV_GUIDE.md) for
building games, [BUILD_AND_TEST.md](../BUILD_AND_TEST.md) for the full
build/test matrix, and [GAMES.md](../GAMES.md) for the 100-game program.
