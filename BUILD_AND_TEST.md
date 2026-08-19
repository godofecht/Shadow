# Shadow Engine - Build and Test Instructions

## Prerequisites

### macOS
Install SDL2 libraries using Homebrew:
```bash
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

## Building

### Build the Game Engine and Tests
```bash
cd Shadow
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DMYGAME_VENDORED=OFF
cmake --build . -j8
```

This will create the executables:
- `sdl_app` - The game engine
- `sdl_app_tests` - The unit test suite
- `sdl_app_backend_tests` - The backend integration tests
- `snake_example`, `minesweeper_example`, `tictactoe_example`, `roguelike_example` - Example games

## Running

### Run the Game Engine
```bash
cd build
./sdl_app
```

Note: The game engine requires a graphical display to run.

### Run the Examples
Four example games are included demonstrating 2D tile-based gameplay:

```bash
# Snake - Classic snake game
./snake_example

# Minesweeper - Mine-sweeping puzzle game
./minesweeper_example

# Tic Tac Toe - Two-player strategy game
./tictactoe_example

# Roguelike - Dungeon crawler with combat
./roguelike_example
```

See `Examples/README.md` for detailed instructions on each game.

### Run the Tests
```bash
cd build
./sdl_app_tests
```

Or using CTest:
```bash
cd build
ctest --output-on-failure
```

### Headless smoke tests (every example and game)

The headless smoke pass CI runs on Linux, macOS, and Windows is available
locally as opt-in ctest tests. Each `smoke_*` entry runs one binary from
`cmake/smoke_targets.txt` (every example + every shipped game) for 5s
under SDL's dummy video/audio drivers, with `PONG_SMOKE=1` so games drive
their real physics via autoplay. `cmake/smoke.cmake` encodes the CI
acceptance rule: a full 5s window ("Process terminated due to timeout")
means the app looped update/render without crashing, while an immediate
exit 0 means `start()` failed and zero code paths were covered.

```bash
# Opt in at configure time (default OFF so plain `ctest` stays fast):
cmake -S . -B build -DUMBRA_SMOKE_CTEST=ON

# Build the binaries first (ctest does not build), then run:
cmake --build build
ctest --test-dir build -R '^smoke' --output-on-failure
```

`-R '^smoke'` matches one test per entry in `cmake/smoke_targets.txt`;
`ctest -L smoke` selects by label instead. Each test carries a 120s
TIMEOUT as a hang guard (a clean pass is ~5s per binary). The
ASan+UBSan-instrumented variant of the same pass is `make smoke` - see
"Sanitizer Testing (ASan + UBSan)" below.

## Pre-commit sanity gates (no build needed)

The fast local gates below need neither a build nor a CMake configure - each
runs in well under a second and mirrors a CI step exactly, so they are safe
(and encouraged) to run before every commit. `make verify-all` runs all
three in one command; a failure there is a failure the first CI minutes
would have caught.

```bash
# Every shipped Games/ dir must appear in cmake/smoke_targets.txt (the
# canonical smoke list), otherwise it ships with zero smoke/memcheck/
# browser CI coverage.
make check-games

# Every Examples/ dir must be registered via add_umbra_example in
# CMakeLists.txt, and every *_example smoke entry must map to a target.
make check-examples

# The vendored-layout gate: dependencies/ is the single canonical vendored
# location, so none of the seven vendored trees (sdl, sdl-image, SDL_ttf,
# SDL2_mixer, box2d, rtaudio, libgamepad) may exist at the repo root - and
# no build file (CMakeLists.txt, Makefile, build_wasm.sh, tools/, cmake/)
# may reference a root-level vendored path. Mirrors the CMake configure
# guard, but catches a dirty checkout before any configure runs.
make check-layout

# All three gates in one command (order: games, examples, layout).
make verify-all
```

## Memory Checking with Valgrind (Linux)

Valgrind catches what compiler warnings and even ASan cannot: uninitialized
reads, invalid accesses, and leak classes ASan's allocator does not see.
The exact CI passes (`linux-valgrind` / `linux-valgrind-examples`) are
wrapped in CMake custom targets, so there is no need to copy flags:

```bash
# Unit tests under memcheck (full leak policy, valgrind.supp applied)
cmake --build build --target memcheck      # or: make memcheck / ninja memcheck

# Headless examples under memcheck (SDL dummy drivers, 10s each)
cmake --build build --target memcheck-examples
```

The same passes are also available as opt-in ctest tests - the CI jobs run
this exact path - so they integrate with your usual test workflow:

```bash
# Opt in at configure time (default OFF so plain `ctest` stays fast):
cmake -S . -B build -DUMBRA_MEMCHECK_CTEST=ON

# Build the binaries first (ctest does not build), then run:
cmake --build build --target sdl_app_tests sdl_app_backend_tests
ctest --test-dir build -R memcheck --output-on-failure
```

`-R memcheck` matches one test per unit binary (`memcheck_unit_*`) and one
per example (`memcheck_example_*`); `ctest -L memcheck` selects by label
instead. Each test has a per-test TIMEOUT (600s per unit binary - the same
hang guard the CI job uses - and 300s per example) to guard against
hangs; the `make memcheck` custom target keeps a more generous 3600s
backstop.

If the first Linux run surfaces new library noise that is not yet in
`valgrind.supp`, refine it automatically:

```bash
# Runs --gen-suppressions=all on the unit tests and appends new blocks
cmake --build build --target memcheck-suppressions
```

The helper only ever auto-adds `Memcheck:Leak` blocks whose
`match-leak-kinds` is exactly `possible` (the same policy the file's
entries follow) - definite/indirect leaks and error-kind blocks are never
auto-suppressed, since those are real defects. Blocks already covered by
an existing entry are skipped (matched by frame prefix), and the updated
file is validated with valgrind's own suppression parse before it
replaces the original. The manual command is still documented in
`valgrind.supp`'s header for cases where the automated harvest is not
enough.

On a clean tree the harvest is a no-op, so the `linux-valgrind` CI job
also runs it in **check-only mode** as a fail-fast suppression-drift
check - if the harvest WOULD add blocks, the job fails with the exact
command to run locally (the file is never touched by the check):

```bash
cmake -DMEMCHECK_BUILD_DIR=$PWD/build-valgrind \
      -DMEMCHECK_SUPPRESSIONS=$PWD/valgrind.supp \
      -DMEMCHECK_CHECK_ONLY=ON -P cmake/memcheck-suppressions.cmake
```

Prerequisites: Linux (WSL works) with `valgrind` installed
(`sudo apt install valgrind`). The targets and ctest tests only exist on
Linux builds and only for what you configured: `memcheck` and
`memcheck_unit_*` need `-DUMBRA_BUILD_TESTS=ON` (default),
`memcheck-examples` and `memcheck_example_*` need `-DUMBRA_BUILD_EXAMPLES=ON`
(default). A Debug build gives the most readable stack traces. The targets
build their binaries first, validate `valgrind.supp` syntax, then run
valgrind with the exact CI flag sets - any memcheck error or
definite/indirect/possibly-lost leak fails the run.

### Profiling with Callgrind (Linux)

Local-only profiling tooling (deliberately not wired into CI - it is noise,
and the run is bounded by a SIGTERM kill rather than a pass/fail signal):

```bash
# Profile bunnymark_example (a CPU hotspot demo) and emit a report
cmake --build build --target callgrind      # or: make callgrind / ninja callgrind

# Profile the engine app itself
cmake --build build --target callgrind-app
```

Each run executes the binary under `valgrind --tool=callgrind` for 10
seconds by default (tune at configure time with
`cmake -DCALLGRIND_SECONDS=30`), then kills it with SIGTERM via GNU
`timeout` - callgrind flushes its profile data on SIGTERM, and
`--kill-after` is deliberately avoided since SIGKILL would lose the
profile. Two outputs land in the build dir:

- `callgrind.out.<target>` - the raw callgraph data (kcachegrind format;
  open it in kcachegrind for the interactive call graph)
- `callgrind.annotate.<target>.txt` - the annotated hotspot report (the
  terminal also prints its top lines)

The profiled process runs with SDL's dummy drivers by default (headless,
profiles the CPU game loop deterministically); set
`-DCALLGRIND_DUMMY_DRIVERS=OFF` to use the real display. If the binary
exits before the window instead of being killed, the script warns loudly -
for these infinite-loop apps that means start() failed and the profile
covers only init.

Prerequisites: Linux (WSL works) with `valgrind` installed (which includes
`callgrind_annotate`). The targets only exist on Linux builds and build
their binary first. The profiled process runs from the source root so
cwd-relative assets (e.g. `fly.png` for bunnymark) resolve.

## Sanitizer Testing (ASan + UBSan)

Two Makefile targets reproduce the sanitizer CI jobs (`linux-sanitize`,
`linux-ubsan`) locally without copying flags. Both build into their own
build dir (separate from `build/` and `build-games/` so the `-fsanitize`
objects never mix with the fast native build):

```bash
# Unit suite (265 tests) + a Brick Breaker+ headless smoke under ASan+UBSan
make sanitize

# Reproduce the linux-sanitize smoke pass: every headless example/game
# binary (the smoke_* ctest entries) under ASan+UBSan, 5s each
make smoke
```

`make sanitize` configures `build-sanitize/` with `UMBRA_SANITIZE=ON`,
builds the test binaries plus `BrickBreakerPlus_game`, and runs
`cmake/sanitize.cmake` - the same runtime half as the CI job. `make smoke`
configures `build-smoke/` with `UMBRA_SANITIZE=ON` +
`UMBRA_BUILD_EXAMPLES=ON` (both flags gate the `smoke_*` ctest entries in
CMakeLists.txt), builds the `sanitize_smoke_binaries` target, then runs
`ctest --test-dir build-smoke -R '^smoke' --output-on-failure`. Each entry
smoke-runs one binary for 5s under SDL dummy drivers + `PONG_SMOKE=1`
autoplay via `cmake/smoke.cmake`, which encodes the CI acceptance rule
(exit 124 = clean full window; immediate exit 0 = start() failure = fail).

The instrumented builds also register the focused `sanitize_path_*` ctest
entries (the exact code paths the sanitizers found bugs in), so
`ctest --test-dir build-sanitize --output-on-failure` runs the full suite
plus those four named regressions.

Platform notes: both targets set `UBSAN_OPTIONS=halt_on_error=1` and
detect_leaks on Linux only - macOS clang ships no LeakSanitizer, where
detect_leaks=1 makes every ASan binary abort with "detect_leaks is not
supported on this platform" (the smoke scripts omit it there; memory
errors and UB are still caught). UBSan-only coverage (signed/float-cast
overflow) is available by configuring with `-DUMBRA_SANITIZER_SET=undefined`
instead of the default combined set.

Prerequisites: a C++17 toolchain with clang/gcc ASan support (both targets
need GNU `timeout`; on macOS install coreutils for `gtimeout`). The smoke
build recompiles all ~24 example/game binaries instrumented, so the first
`make smoke` is the slow one.

## Test Coverage

The test suite covers:

### Geometry Tests (8 tests)
- Point2D creation and operations
- Vector2D creation, normalization, and operations
- Rect creation, points calculation, and updatePoints

### Physics Tests (4 tests)
- World creation
- RigidBody creation
- PhysicsManager creation
- Body setters/getters

### Perlin Noise Tests (6 tests)
- Single point noise generation
- Noise range validation
- Noise continuity
- Noise map generation and dimensions
- Interpolation

## Project Structure

```
Shadow/
├── CMakeLists.txt          # Main CMake configuration
├── main.cpp                # Game entry point
├── SDLApp.cpp/h            # Game application class
├── Scene.cpp/h             # Scene management
├── Sprite.cpp/h            # Sprite rendering
├── Renderer.h              # Renderer class
├── Physics.h               # Box2D physics wrapper
├── Geometry.h              # Geometry primitives
├── TextWriter.h            # Text rendering (cross-platform)
├── AssetManager.h          # Asset management
├── tests/
│   ├── test_main.cpp       # Test runner main
│   ├── test_main.h         # Test framework
│   ├── test_geometry.cpp   # Geometry tests
│   ├── test_physics.cpp    # Physics tests
│   └── test_perlin_noise.cpp  # Perlin noise tests
└── build/
    ├── sdl_app             # Game executable
    └── sdl_app_tests       # Test executable
```

## Troubleshooting

### SDL2 Libraries Not Found
If you get errors about SDL2 libraries not being found:
```bash
brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

### Build Errors
If you encounter build errors, try cleaning the build directory:
```bash
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DMYGAME_VENDORED=OFF
cmake --build . -j8
```

### Test Failures
If tests fail, check that:
1. All required headers are included
2. The Box2D API matches the version installed
3. No floating-point precision issues in comparisons
