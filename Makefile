# BrainRot Engine Makefile
# Targets for both Native and WASM builds

CC = g++
EMCC_CXX = /opt/homebrew/opt/python@3.14/bin/python3.14 /opt/homebrew/Cellar/emscripten/5.0.0/libexec/emcc.py
EMCC_CC = /opt/homebrew/opt/python@3.14/bin/python3.14 /opt/homebrew/Cellar/emscripten/5.0.0/libexec/emcc.py
INCLUDES = -I. -Idependencies/box2d/include -Idependencies/box2d/src
CPPFLAGS = $(INCLUDES) -std=c++17
CFLAGS = $(INCLUDES)
WASM_FLAGS = -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s USE_SDL_TTF=2 -s USE_SDL_MIXER=2 -s WASM=1
SHIM_PATH = $(PWD)/bin_shims

CORE_SOURCES = Engine/Core/SDLApp.cpp Engine/Core/AssetManager.cpp \
               Engine/Core/Game2D.cpp Engine/Core/UI.cpp Engine/Core/InputManager.cpp \
               Engine/EntityAndScene/Sprite.cpp Engine/EntityAndScene/Scene.cpp Engine/EntityAndScene/Grid.cpp
BOX2D_C_SOURCES = $(shell find dependencies/box2d/src -name "*.c")
BOX2D_CPP_SOURCES = $(shell find dependencies/box2d/src -name "*.cpp")

all: native wasm

native:
	mkdir -p build && cd build && cmake .. && make -j8

wasm:
	mkdir -p web
	@export PATH=$(SHIM_PATH):$$PATH; \
	export EM_CONFIG=$(PWD)/web/.emscripten_local; \
	for demo in procedural3d terrain snake minesweeper tictactoe roguelike particles parenting physics_reactive tweening bunnymark bloom skeletal spatial_audio text2d fixed_timestep game_of_life lighting navigation ai_states compute_sim; do \
		echo "Building $$demo WASM..."; \
		if [ "$$demo" = "procedural3d" ]; then \
			SRC="Examples/Procedural3D/main.cpp"; \
		elif [ "$$demo" = "terrain" ]; then \
			SRC="Examples/ProceduralTerrain/main.cpp"; \
		elif [ "$$demo" = "particles" ]; then \
			SRC="Examples/ParticleShowcase/main.cpp"; \
		elif [ "$$demo" = "parenting" ]; then \
			SRC="Examples/Parenting/main.cpp"; \
		elif [ "$$demo" = "physics_reactive" ]; then \
			SRC="Examples/PhysicsReactive/main.cpp"; \
		elif [ "$$demo" = "tweening" ]; then \
			SRC="Examples/Tweening/main.cpp"; \
		elif [ "$$demo" = "bunnymark" ]; then \
			SRC="Examples/Bunnymark/main.cpp"; \
		elif [ "$$demo" = "bloom" ]; then \
			SRC="Examples/BloomShowcase/main.cpp"; \
		elif [ "$$demo" = "skeletal" ]; then \
			SRC="Examples/SkeletalAnim/main.cpp"; \
		elif [ "$$demo" = "spatial_audio" ]; then \
			SRC="Examples/SpatialAudio/main.cpp"; \
		elif [ "$$demo" = "text2d" ]; then \
			SRC="Examples/Text2D/main.cpp"; \
		elif [ "$$demo" = "fixed_timestep" ]; then \
			SRC="Examples/FixedTimestep/main.cpp"; \
		elif [ "$$demo" = "game_of_life" ]; then \
			SRC="Examples/GameOfLife/main.cpp"; \
		elif [ "$$demo" = "lighting" ]; then \
			SRC="Examples/Lighting/main.cpp"; \
		elif [ "$$demo" = "navigation" ]; then \
			SRC="Examples/Navigation/main.cpp"; \
		elif [ "$$demo" = "ai_states" ]; then \
			SRC="Examples/AIStates/main.cpp"; \
		elif [ "$$demo" = "compute_sim" ]; then \
			SRC="Examples/ComputeSim/main.cpp"; \
		elif [ "$$demo" = "snake" ]; then \
			SRC="Examples/Snake/main.cpp"; \
		elif [ "$$demo" = "minesweeper" ]; then \
			SRC="Examples/Minesweeper/main.cpp"; \
		elif [ "$$demo" = "tictactoe" ]; then \
			SRC="Examples/TicTacToe/main.cpp"; \
		elif [ "$$demo" = "roguelike" ]; then \
			SRC="Examples/Roguelike/main.cpp"; \
		fi; \
		EXTRA_FLAGS=""; \
		if [ "$$demo" = "bunnymark" ]; then EXTRA_FLAGS="--preload-file fly.png"; fi; \
		if [ "$$demo" = "spatial_audio" ]; then EXTRA_FLAGS="--preload-file looptheme.wav"; fi; \
		$(EMCC_CXX) $$SRC $(CORE_SOURCES) $(BOX2D_CPP_SOURCES) $(CPPFLAGS) $(WASM_FLAGS) $$EXTRA_FLAGS -c; \
		$(EMCC_CC) $(BOX2D_C_SOURCES) $(CFLAGS) $(WASM_FLAGS) -c; \
		$(EMCC_CXX) *.o $(WASM_FLAGS) $$EXTRA_FLAGS -o web/$$demo.js; \
		rm -f *.o; \
	done; \
	for main in Games/*/main.cpp; do \
		[ -e "$$main" ] || continue; \
		gamedir=$$(dirname "$$main"); \
		gamename=$$(basename "$$gamedir"); \
		case "$$gamename" in \
			_*) echo "Skipping generator template: $$gamename"; continue ;; \
		esac; \
		echo "Building game $$gamename WASM..."; \
		GAME_EXTRA=""; \
		if [ -d "$$gamedir/assets" ] && [ -n "$$(ls -A "$$gamedir/assets" 2>/dev/null)" ]; then \
			GAME_EXTRA="--preload-file $$gamedir/assets@/assets"; \
		fi; \
		if [ -f web/default.ttf ]; then GAME_EXTRA="$$GAME_EXTRA --preload-file web/default.ttf@/default.ttf"; fi; \
		mkdir -p web/games/$$gamename; \
		$(EMCC_CXX) $$main $(CORE_SOURCES) $(BOX2D_CPP_SOURCES) $(CPPFLAGS) $(WASM_FLAGS) $$GAME_EXTRA -c; \
		$(EMCC_CC) $(BOX2D_C_SOURCES) $(CFLAGS) $(WASM_FLAGS) -c; \
		$(EMCC_CXX) *.o $(WASM_FLAGS) $$GAME_EXTRA -o web/games/$$gamename/index.html; \
		rm -f *.o; \
	done

# --- Games: the 100-game program -------------------------------------------
# make new-game GAME=my_game                    scaffold Games/my_game (collector starter)
# make new-game GAME=my_game VARIANT=twin-stick  scaffold the arena-shooter starter
# make new-game GAME=my_game VARIANT=platformer  scaffold the jump-and-run starter
# make game GAME=my_game        build one game natively
# make games                    build every game in Games/ natively
# make check-games              verify every shipped Games/ dir is in
#                               cmake/smoke_targets.txt (no build needed)
# make check-examples           verify every Examples/ dir is registered via
#                               add_umbra_example (no build needed)
# make check-layout              verify no root-level vendored dirs reappeared
#                               (everything lives under dependencies/; no build
#                               needed)
# make check-docs                verify every 'make <target>' documented in
#                               CONTRIBUTING.md's recipe exists in the Makefile
#                               (no build needed)
# make check-github              verify .github/*.md templates reference only
#                               real CI job names, and CODEOWNERS references
#                               only handles in MAINTAINERS (no build needed)
# make verify-all                 run check-games + check-examples + check-layout
#                               + check-docs + check-github in one command
#                               (the complete pre-commit pass)
# make hooks                    install the git pre-commit hook (one-time
#                               setup - runs make verify-all before every commit)
# make protect-branch           apply full branch protection to main via the
#                               GitHub API (PR reviews + approvals + required
#                               status checks; needs gh CLI + admin token)
# make check-branch-protection   verify live GitHub protection matches
#                               CONTRIBUTING.md (read-only; needs gh + network)
# make llm-demo                 build and run the LLM-interface demo
#                               (Examples/llm_test.cpp, console-only)
# make wasm                     examples + every shipped game to web/ (needs emsdk)

new-game:
	@test -n "$(GAME)" || (echo "Usage: make new-game GAME=<game_name> [VARIANT=twin-stick|platformer]"; exit 1)
	@./tools/new_game.sh $(if $(VARIANT),--$(VARIANT)) $(GAME)

game:
	@test -n "$(GAME)" || (echo "Usage: make game GAME=<game_name> (see Games/)"; exit 1); \
	test -f "Games/$(GAME)/main.cpp" || (echo "No game '$(GAME)' - run 'make new-game GAME=$(GAME)' first"; exit 1); \
	mkdir -p build-games && cd build-games && cmake .. -DUMBRA_BUILD_EXAMPLES=OFF >/dev/null && \
	cmake --build . --target $(GAME)_game -j8

games:
	mkdir -p build-games && cd build-games && cmake .. -DUMBRA_BUILD_EXAMPLES=OFF >/dev/null && \
	cmake --build . -j8

# make check-games   run the game-registration gate locally: every shipped
#                    Games/ directory must appear in cmake/smoke_targets.txt
#                    (the canonical smoke list), otherwise it has no smoke /
#                    memcheck / browser CI coverage. Same check as the CI
#                    step and the CMake configure-time gate, but needs no
#                    build and no CMake - pure, fast, and safe to run before
#                    every commit.

check-games:
	@./tools/check_game_registration.sh

# make check-examples   run the example-registration gate locally: every
#                       Examples/ dir must be registered via add_umbra_example
#                       in CMakeLists.txt (and every *_example smoke entry must
#                       map to a registered target). Same check as the CI step
#                       and the CMake configure-time gate, but needs no build.

check-examples:
	@./tools/check_example_registration.sh

# make check-layout   run the vendored-layout gate locally: none of the seven
#                     vendored dependency trees (sdl, sdl-image, SDL_ttf,
#                     SDL2_mixer, box2d, rtaudio, libgamepad) may exist at the
#                     repo root - they live only under dependencies/ (the single
#                     canonical vendored location). Same check as the CI step in
#                     every job and the CMake configure-time gate, but needs no
#                     configure and no build - safe to run before every commit
#                     and before the first configure of a dirty checkout.

check-layout:
	@./tools/check_vendored_layout.sh

# make check-docs   verify the contributor-facing recipe can't drift from the
#                   build surface: every 'make <target>' command documented in
#                   CONTRIBUTING.md (inside code fences or inline code spans)
#                   must exist as a real target in this Makefile. A target
#                   renamed or removed without updating the docs fails here
#                   instead of shipping stale instructions. Only the docs ->
#                   Makefile direction is enforced - internal targets (clean,
#                   new-game, smoke, ...) are intentionally not required to be
#                   documented. Same check as the CI verify-all step, but
#                   needs no build.

check-docs:
	@./tools/check_docs_makefile.sh

# make check-github   verify the GitHub metadata can't drift from its sources
#                     of truth: every CI job name referenced in the
#                     .github/*.md templates must exist as a job in
#                     .github/workflows/ci.yml, and every @handle (individual
#                     or @org/team) in .github/CODEOWNERS must be declared in
#                     the canonical MAINTAINERS file at the repo root. Needs
#                     no build and no network - pure local checks, safe to run
#                     before every commit. Same check as the CI verify-all
#                     step.

check-github:
	@./tools/check_github_meta.sh

# make test-github   run the offline regression tests for the GitHub tooling:
#                    test_check_github_meta.sh exercises the check-github
#                    gate (a pristine fixture must pass; seven mutations -
#                    missing job, wrong count, set disagreement, job-shaped
#                    reference, unknown handle, owner-less rule - must each
#                    fail it), and test_apply_branch_protection.sh exercises
#                    the apply script against a stub gh (its two API calls
#                    must receive the parsed required checks and the approval
#                    flags, and --dry-run must not call the API). Both build
#                    scratch fixtures from the real metadata - no build, no
#                    network, no CMake - so they're safe to run before every
#                    commit.

test-github:
	@./tools/test_check_github_meta.sh
	@./tools/test_apply_branch_protection.sh

# make verify-all   run every fast local gate in one command - check-games,
#                   check-examples, check-layout, check-docs, check-github - so
#                   a single command gives the complete pre-commit sanity pass
#                   (no build, no CMake). Same checks as the first CI minutes,
#                   in the same order; make stops on the first failing gate.

verify-all: check-games check-examples check-layout check-docs check-github
	@echo "All gates passed: games registered, examples registered, vendored layout canonical, docs match the Makefile, GitHub metadata resolves."

# make hooks   install the git pre-commit hook in one command - the one-time
#              setup CONTRIBUTING.md documents, so a new contributor just runs
#              'make hooks' instead of './tools/install-hooks.sh'. The hook
#              runs 'make verify-all' before every commit (the exact combined
#              command CI runs). Idempotent - re-running overwrites our own
#              hook cleanly - and backs up a pre-existing foreign hook to
#              .git/hooks/pre-commit.bak before replacing it.

hooks:
	@./tools/install-hooks.sh

# make protect-branch   apply full branch protection to the default branch
#                       (main) via the GitHub API - require a pull request
#                       before merging with N approving reviews plus the
#                       required status checks - turning CONTRIBUTING.md's
#                       prose guidance into enforceable repo config. Wraps
#                       tools/apply_branch_protection.sh (needs the gh CLI
#                       authenticated with admin:repo_hook permission).
#                       Idempotent - re-running after adding/removing a job
#                       name in CONTRIBUTING.md updates the existing rule.
#                       Pass DRY_RUN=1 for a dry run and APPROVALS=N for the
#                       review count: make protect-branch DRY_RUN=1 APPROVALS=2

protect-branch:
	@./tools/apply_branch_protection.sh $(if $(DRY_RUN),--dry-run) $(if $(APPROVALS),--approvals $(APPROVALS))

# make check-branch-protection   verify the live GitHub branch protection on
#                                main matches CONTRIBUTING.md's required
#                                checks (read-only - the inverse of
#                                protect-branch, which writes). Needs gh with
#                                read access; hits the network so it is NOT
#                                part of the offline verify-all gates.

check-branch-protection:
	@./tools/check_branch_protection.sh

# make llm-demo   build and run the LLM-interface demo in one command - the
#                 exact two steps LLM_INTERFACE.md documents, so the demo
#                 needs no manual build first. llm_test_example is a
#                 console-only example (its own ConsoleSnake game, no SDL
#                 window), so it runs headless in any terminal. The build/
#                 cache is configured on first use if it doesn't exist yet.

llm-demo:
	@mkdir -p build && cd build && cmake .. >/dev/null && \
	cmake --build . --target llm_test_example -j8 && \
	./llm_test_example --demo

# --- Sanitizers ------------------------------------------------------------
# make sanitize   configure + build + run the ASan+UBSan unit suite and a
#                 Brick Breaker+ headless smoke in one command. The
#                 instrumented build lives in build-sanitize/ (separate from
#                 build/ and build-games/ so the -fsanitize objects never mix
#                 with the fast native build). The runtime half lives in
#                 cmake/sanitize.cmake (same pattern as cmake/memcheck.cmake);
#                 it enables LeakSanitizer on Linux only (macOS has no LSan).

sanitize:
	mkdir -p build-sanitize
	cd build-sanitize && cmake .. -DCMAKE_BUILD_TYPE=Debug -DUMBRA_SANITIZE=ON -DUMBRA_WERROR=ON -DUMBRA_BUILD_EXAMPLES=OFF >/dev/null
	cmake --build build-sanitize --parallel --target sdl_app_tests sdl_app_backend_tests BrickBreakerPlus_game
	cmake -DSANITIZE_BUILD_DIR=$(CURDIR)/build-sanitize -P cmake/sanitize.cmake

# make smoke   reproduce the linux-sanitize smoke pass locally: configure a
#              sanitizer build with examples ON (build-smoke/ - separate from
#              build-sanitize/ so the two targets never fight over flags),
#              build the sanitize_smoke_binaries target (all headless
#              example/game binaries, gated in CMakeLists on UMBRA_SANITIZE +
#              UMBRA_BUILD_EXAMPLES), then run the smoke_* ctest entries.
#              Each entry drives one binary for 5s under SDL dummy drivers +
#              PONG_SMOKE=1 autoplay via cmake/smoke.cmake, which encodes the
#              CI acceptance rule and sets the sanitizer env (detect_leaks=1
#              on Linux; omitted on macOS where ASan lacks LSan).

smoke:
	mkdir -p build-smoke
	cd build-smoke && cmake .. -DCMAKE_BUILD_TYPE=Debug -DUMBRA_SANITIZE=ON -DUMBRA_WERROR=ON -DUMBRA_BUILD_EXAMPLES=ON >/dev/null
	cmake --build build-smoke --parallel --target sanitize_smoke_binaries
	ctest --test-dir build-smoke -R '^smoke' --output-on-failure

clean:
	rm -rf build build-games build-sanitize build-smoke web/*.js web/*.wasm
