# Changelog

All notable changes to the Shadow Engine are documented here.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Version numbers are the single source of truth in `CMakeLists.txt`
(`project(sdl_app VERSION ...)`); `Engine/Version.h` is generated from it.

## [Unreleased]

### Added
- **CI pipeline** (`.github/workflows/ci.yml`): Linux (GCC + Ninja), macOS
  (Clang), Windows (MSVC + vcpkg manifest), and Emscripten (WASM) jobs that
  build the engine, all examples, and run the test suite via CTest.
- **Comprehensive unit test suite** — 160+ tests across the engine:
  geometry/matrix math, Perlin noise and easing, grid movement/collision,
  the LLM game interface (`Game2D`/`GameState`), thread-safe asset map
  (including concurrency tests), particles, input bindings, UI components,
  scenes, audio groups, physics objects, and roguelike state.
- **Licensing** — `LICENSE` (dual: GPL-3.0-or-later or proprietary
  commercial), `LICENSE.GPL-3.0`, `LICENSE.COMMERCIAL` (royalty-bearing
  commercial terms), and SPDX headers (`GPL-3.0-or-later OR
  LicenseRef-Commercial`) on all engine source files.
- **Commercial licensing** — free GPL games pay nothing; commercial
  releases require a commercial license with royalties (see
  `LICENSE.COMMERCIAL`).
- **Canonical header layout** — the engine now lives under `Engine/`
  (`Audio/`, `Core/`, `EntityAndScene/`, `Rendering/`, `ResourceHandling/`,
  `Text/`) with all includes updated; the old flat root mirror and the
  root-level TankGame scripts were removed.
- **Generated version header** — `Engine/Version.h` with
  `UMBRA_VERSION_MAJOR/MINOR/PATCH` and `UMBRA_VERSION_STRING`, configured
  by CMake and used by `sdl_app` (window title + startup banner).
- `vcpkg.json` manifest for reproducible Windows dependency builds.

### Fixed
- Native build on macOS/Linux (SDL2 config discovery, target names,
  Windows-only libs guarded, hardcoded Homebrew paths removed).
- Undefined behavior in `Helpers.h::OutBounce` (unsequenced mutation) and
  `M_PI` portability guard.
- Hardcoded `C:/Users/...` asset path in the TankGame `Bullet.h`.
- C99 variable-length array in `Examples/PixelGridSnapping` (MSVC blocker).
- Missing virtual destructors on `Object`/`Part`; dead `drawFPS` code.

### Changed
- `CMakeLists.txt` rewritten: shared `umbra_engine` static library, correct
  SDL2 imported targets, `UMBRA_BUILD_TESTS`/`UMBRA_BUILD_EXAMPLES` options,
  CTest registration, and Emscripten link flags.
- `README.md` rewritten with quick start, build/test/WASM instructions, and
  project layout.

## [1.0.0] - 2024-11-16

First feature-complete release.

### Added
- SDL2 game loop and windowing — `Game` base class with renderer, physics,
  audio, asset manager, scene list, and FPS limiting.
- Grid-based game framework — `Game2D` with tile grid, entities, bindable
  input, UI helpers, and delta-time loop.
- LLM-playable game interface (`LLMPlayable`/`GameState`): export state,
  register named actions, and let an agent drive the game.
- Box2D physics manager (worlds, rigid bodies, collision callbacks).
- SDL_mixer audio engine with media groups and per-player control.
- Rendering: sprites with hierarchical parts, tile maps, grid rendering,
  UI text/buttons/stats, and 2D effects examples (bloom, lighting,
  particles, meshes, procedural terrain).
- 50+ runnable example games and demos (`Examples/`).
- WebAssembly builds via Emscripten (`build_wasm.sh`, `web/` outputs).
- Windows support with vendored SDL2 dependencies.

[Unreleased]: https://github.com/godofecht/Shadow/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/godofecht/Shadow/releases/tag/v1.0.0
