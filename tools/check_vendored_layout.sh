#!/usr/bin/env bash
# Verify the single-canonical-vendored-layout invariant BEFORE configure.
#
# Two checks, mirroring the configure-time guard in CMakeLists.txt:
#   1. Directory check: none of the seven vendored dependency trees (sdl,
#      sdl-image, SDL_ttf, SDL2_mixer, box2d, rtaudio, libgamepad) may
#      exist at the repo root - they live only under dependencies/.
#   2. Reference scan: no file that builds the project (CMakeLists.txt,
#      Makefile, build_wasm.sh, tools/*.sh, tools/*.py, cmake/*.cmake) may
#      contain a root-level path reference to one of those trees. Canonical
#      paths under dependencies/ are masked out first, so anything left is
#      a root-level reference.
#
# The configure guard runs the same two checks as a FATAL_ERROR, but only
# when a job configures. This script runs right after checkout - before any
# configure - so a fresh checkout with a root-level tree or a stale path
# reference fails fast with a clear, named error, and it also covers jobs
# that configure from a stale cache.
#
# NOTE for future edits: never write a dir name followed by a trailing
# slash in this file - the reference scan (both this script and the CMake
# guard) flags any dir-name-plus-slash path segment found in tools/*.sh.
# Plain bare names are fine.
#
# bash 3.2 compatible (macOS default bash ships no associative arrays).
#
# Usage: tools/check_vendored_layout.sh
# Exit 0 if the layout is canonical, 1 otherwise.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VENDORED_DIRS=(sdl sdl-image SDL_ttf SDL2_mixer box2d rtaudio libgamepad)

bad=0

# --- 1. Directory check --------------------------------------------------
# EXISTS-style: fires on an empty dir or un-checked-out gitlink too, which
# is exactly the regression mode for the gitlink deps.
for d in "${VENDORED_DIRS[@]}"; do
    if [ -e "$ROOT/$d" ]; then
        echo "::error::root-level vendored dir '$ROOT/$d' exists - all vendored dependencies must live only under dependencies/ (delete it or merge it into dependencies/$d)" >&2
        bad=1
    fi
done

# --- 2. Reference scan ---------------------------------------------------
# Same file set, same canonical-path masking, and same three patterns as the
# CMake guard, so the two can never disagree:
#   a. a vendored dir name used as a path segment (an include flag, a
#      source list entry, a source-dir-anchored include dir):  <dir>/
#   b. a source-dir-anchored path with no trailing slash:
#      CMAKE_(CURRENT_)?SOURCE_DIR}/<dir>
#   c. a bare add_subdirectory(<dir>)
# Comments are scanned too - a documented stale path is just as likely to
# be copy-pasted back in. Masking canonical paths first keeps these
# patterns false-positive free on the real tree.
shopt -s nullglob
checked_files=(
    "$ROOT/CMakeLists.txt"
    "$ROOT/Makefile"
    "$ROOT/build_wasm.sh"
    "$ROOT"/tools/*.sh
    "$ROOT"/tools/*.py
    "$ROOT"/cmake/*.cmake
)
shopt -u nullglob

for f in "${checked_files[@]}"; do
    [ -f "$f" ] || continue
    content="$(cat "$f")"
    # Mask canonical paths so only root-level references remain. Order
    # matters: sdl is a prefix of sdl-image, so sdl is masked first -
    # identical to the CMake guard's loop order. The masking is done with
    # sed, NOT bash's ${var//pat/} substitution: that form is pathologically
    # slow on multi-KB strings in the macOS default bash 3.2 (it hangs on
    # CMakeLists.txt), while sed is linear. The '|' delimiter avoids
    # escaping the '/' in the path, and the -e expressions apply in order,
    # so the result is byte-identical to the CMake guard's masking.
    sed_args=()
    for d in "${VENDORED_DIRS[@]}"; do
        sed_args+=(-e "s|dependencies/$d||g")
    done
    content="$(printf '%s' "$content" | sed "${sed_args[@]}")"
    lineno=0
    while IFS= read -r line; do
        lineno=$((lineno + 1))
        for d in "${VENDORED_DIRS[@]}"; do
            re="${d}/|CMAKE_(CURRENT_)?SOURCE_DIR[}]/${d}|add_subdirectory\(${d}\)"
            if [[ "$line" =~ $re ]]; then
                trimmed="$(printf '%s' "$line" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
                [ -n "$trimmed" ] || continue
                echo "::error::root-level vendored path reference in ${f#"$ROOT"/}:${lineno}: ${trimmed}" >&2
                bad=1
                break
            fi
        done
    done < <(printf '%s\n' "$content")
done

if (( bad )); then
    echo "Vendored layout is not canonical - see errors above." >&2
    exit 1
fi

echo "OK: no root-level vendored dirs or stale path references - layout is canonical (dependencies/ only)"
