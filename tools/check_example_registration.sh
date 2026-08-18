#!/usr/bin/env bash
# Verify every shipped Examples/ directory is registered as an example
# target in CMakeLists.txt, and that cmake/smoke_targets.txt's *_example
# entries all map to registered targets.
#
# Examples are registered explicitly (add_umbra_example(<name> <path>)) and
# the directory->target name mapping is hand-curated (2DBloom ->
# bloom2d_example, ParticleShowcase -> particles_example, ...), so unlike
# the games gate there is no glob-to-target rule to check against. Instead
# this script:
#   - forward:  every Examples/<dir>/main.cpp must be referenced by an
#               add_umbra_example() call (or be a known exception below). An
#               example with no registration ships with zero build/CI
#               coverage.
#   - reverse:  every *_example entry in cmake/smoke_targets.txt must be a
#               registered example target (stale entries fail loudly here
#               instead of at build time).
#
# CMake also enforces the reverse direction at configure time (see the
# UMBRA_BUILD_EXAMPLES block in CMakeLists.txt); this script is the fast,
# named CI check with a full diagnosis, and runs without configuring.
#
# KNOWN EXCEPTIONS (directories with a main.cpp that intentionally have no
# add_umbra_example registration). Keep this list EMPTY: prefer registering
# the example (it must compile warning-free) or deleting the directory over
# adding to this list. All three historic exceptions (DemoRunner scratch
# dir, a non-compiling PerlinNoise, a stale TankAndBullet main.cpp) were
# resolved: DemoRunner deleted, PerlinNoise rewritten on the engine's
# PerlinNoiseGenerator, TankAndBullet rebuilt as a modern Game2D arena.
#
# Usage: tools/check_example_registration.sh
# Exit 0 if the invariants hold, 1 otherwise.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CMAKE="$ROOT/CMakeLists.txt"
LIST="$ROOT/cmake/smoke_targets.txt"

EXCEPTIONS=""

if [[ ! -f "$CMAKE" ]]; then
    echo "::error::CMakeLists.txt not found (expected at $CMAKE)" >&2
    exit 1
fi
if [[ ! -f "$LIST" ]]; then
    echo "::error::cmake/smoke_targets.txt not found (expected at $LIST)" >&2
    exit 1
fi

# add_umbra_example(<target> <path>) -> "target path" lines. Comment
# lines are stripped first so a commented-out registration cannot satisfy
# the forward check.
PAIRS="$(grep -vE '^[[:space:]]*#' "$CMAKE" | grep -oE 'add_umbra_example\([A-Za-z0-9_]+ [^ )]+' | sed 's/^add_umbra_example(//')"
if [[ -z "$PAIRS" ]]; then
    echo "::error::no add_umbra_example() calls found in $CMAKE - parse failed?" >&2
    exit 1
fi
REGISTERED_TARGETS="$(printf '%s\n' "$PAIRS" | awk '{print $1}' | sort -u)"
FILTERED="$(grep -v '^#' "$LIST" || true)"

bad=0

# --- Forward: every Examples/<dir>/main.cpp must be registered (or excepted) ---
registered_path() { # $1 = directory name; true if an add_umbra_example path matches
    printf '%s\n' "$PAIRS" | grep -qF "Examples/$1/main.cpp"
}
in_exceptions() { # $1 = directory name
    printf '%s\n' "$EXCEPTIONS" | grep -qxF "$1"
}

shopt -s nullglob
for main in "$ROOT"/Examples/*/main.cpp; do
    dir="$(basename "$(dirname "$main")")"
    if registered_path "$dir"; then
        continue
    fi
    if in_exceptions "$dir"; then
        echo "note: Examples/$dir/ is a known unregistered exception - see the header of this script"
        continue
    fi
    echo "::error::example Examples/${dir}/main.cpp is not registered in CMakeLists.txt (no add_umbra_example) - add it so the example is built and CI-covered" >&2
    bad=1
done
shopt -u nullglob

# --- Reverse: every *_example in the smoke list must be a registered target ---
while IFS= read -r t; do
    [[ "$t" == *_example ]] || continue
    if ! printf '%s\n' "$REGISTERED_TARGETS" | grep -qxF "$t"; then
        echo "::error::cmake/smoke_targets.txt lists '${t}' but no add_umbra_example() registers it - stale entry or renamed example directory" >&2
        bad=1
    fi
done <<<"$FILTERED"

if (( bad )); then
    echo "Example registration mismatch - see errors above." >&2
    exit 1
fi

echo "OK: every Examples/ dir is registered, and all $(grep -c '_example$' <<<"$FILTERED") *_example smoke entries map to a registered target"
