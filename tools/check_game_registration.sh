#!/usr/bin/env bash
# Verify every shipped Games/ directory is registered in cmake/smoke_targets.txt.
#
# cmake/smoke_targets.txt is the canonical smoke list: it drives the smoke_*
# ctest entries, the sanitize/memcheck build targets, the browser smoke
# (tools/browser_smoke.py --targets-file), and the CI assertion steps. A new
# game that is not added to it silently ships without any of that coverage.
# This script makes that impossible.
#
# It mirrors CMake's registration rules exactly (see the UMBRA_BUILD_GAMES
# block in CMakeLists.txt):
#   - every Games/<name>/main.cpp registers <name>_game
#   - '_'-prefixed dirs are internal starters and are skipped, EXCEPT
#     Games/_template/ which registers template_game
#
# CMake also enforces this at configure time (same invariant, FATAL_ERROR);
# this script is the fast, named CI check with a full diagnosis, and is safe
# to run without configuring the project. It is bash 3.2 compatible (macOS
# default bash ships no associative arrays).
#
# Usage: tools/check_game_registration.sh
# Exit 0 if the bijection holds, 1 otherwise.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LIST="$ROOT/cmake/smoke_targets.txt"

if [[ ! -f "$LIST" ]]; then
    echo "::error::cmake/smoke_targets.txt not found (expected at $LIST)" >&2
    exit 1
fi

# The filtered target list (comments/blank lines stripped), one per line.
FILTERED="$(grep -v '^#' "$LIST" || true)"

in_list() { # $1 = exact target name
    grep -qxF "$1" <<<"$FILTERED"
}

bad=0

# Forward: every registered game target must be in the smoke list.
shopt -s nullglob
for main in "$ROOT"/Games/*/main.cpp; do
    dir="$(basename "$(dirname "$main")")"
    if [[ "$dir" == _* && "$dir" != "_template" ]]; then
        continue  # internal starter dir (e.g. _twin_stick), not a shipped game
    fi
    [[ "$dir" == "_template" ]] && dir="template"
    target="${dir}_game"
    if ! in_list "$target"; then
        echo "::error::game '${target}' (Games/${dir}/) is missing from cmake/smoke_targets.txt - add it so the new game gets smoke/memcheck/browser CI coverage" >&2
        bad=1
    fi
done
shopt -u nullglob

# Reverse: every *_game entry in the file must map to a real Games/ dir
# (catches stale entries and renamed games early instead of at build time).
while IFS= read -r t; do
    [[ "$t" == *_game ]] || continue
    dir="${t%_game}"
    [[ "$dir" == "template" ]] && dir="_template"
    if [[ ! -f "$ROOT/Games/$dir/main.cpp" ]]; then
        echo "::error::cmake/smoke_targets.txt lists '${t}' but Games/${dir}/main.cpp does not exist - stale entry or renamed game directory" >&2
        bad=1
    fi
done <<<"$FILTERED"

if (( bad )); then
    echo "Game registration mismatch with cmake/smoke_targets.txt - see errors above." >&2
    exit 1
fi

echo "OK: all $(grep -c '_game$' <<<"$FILTERED") *_game entries in cmake/smoke_targets.txt match a shipped Games/ directory"
