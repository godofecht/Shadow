#!/usr/bin/env bash
# Verify every `make <target>` command documented in CONTRIBUTING.md's
# recipe actually exists as a target in the Makefile, so the docs can't
# drift from the real build surface.
#
# The recipe is the one-paragraph flow contributors are told to run
# (make verify-all / make native / ctest ... / make hooks) plus the gate
# names (make check-games / check-examples / check-layout). If a documented
# `make <target>` stops existing in the Makefile - a target renamed or
# removed - the docs are stale and this check fails.
#
# Only `make <target>` tokens inside fenced code blocks and inline backtick
# code spans are checked, so prose like "make stops on the first failure"
# is never mistaken for a command. The reverse direction (every Makefile
# target must be documented) is intentionally NOT enforced - internal
# targets (clean, new-game, smoke, ...) are not part of the
# contributor-facing recipe.
#
# bash 3.2 compatible (macOS default bash).
#
# Usage: tools/check_docs_makefile.sh [DOC.md ...]
#        (defaults to CONTRIBUTING.md)
# Exit 0 if every documented target exists, 1 otherwise.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKEFILE="$ROOT/Makefile"

if [[ ! -f "$MAKEFILE" ]]; then
    echo "::error::Makefile not found (expected at $MAKEFILE)" >&2
    exit 1
fi

if (( $# == 0 )); then
    DOCS=("$ROOT/CONTRIBUTING.md")
else
    DOCS=("$@")
fi

# Real Makefile targets: lines whose first non-space chars are `name:`
# (definitions). Comment lines and recipe lines never match.
TARGETS="$(grep -E '^[A-Za-z0-9_.-]+:' "$MAKEFILE" | sed 's/:.*//' | sort -u)"

bad=0

for DOC in "${DOCS[@]}"; do
    if [[ ! -f "$DOC" ]]; then
        echo "::error::document not found: $DOC" >&2
        bad=1
        continue
    fi

    # make <target> tokens from fenced code blocks (whole lines are code)...
    FENCE="$(awk 'BEGIN{f=0} /^```/{f=!f; next} f' "$DOC" \
        | grep -oE '\bmake[[:space:]]+[A-Za-z0-9_.-]+' \
        | awk '{print $2}' | sort -u)"

    # ...and from inline backtick code spans (prose is otherwise ignored).
    INLINE="$(awk 'BEGIN{f=0} /^```/{f=!f; next} !f{while(match($0, /`[^`]*`/)){print substr($0, RSTART+1, RLENGTH-2); $0=substr($0, RSTART+RLENGTH)}}' "$DOC" \
        | grep -oE '\bmake[[:space:]]+[A-Za-z0-9_.-]+' \
        | awk '{print $2}' | sort -u)"

    DOCUMENTED="$(printf '%s\n%s\n' "$FENCE" "$INLINE" | grep -v '^$' | sort -u)"

    if [[ -z "$DOCUMENTED" ]]; then
        echo "::error::no 'make <target>' commands found in $DOC - parse failed or recipe removed?" >&2
        bad=1
        continue
    fi

    while IFS= read -r t; do
        if ! grep -qxF "$t" <<<"$TARGETS"; then
            echo "::error::$DOC documents 'make $t' but no such target exists in the Makefile - rename the target or fix the docs" >&2
            bad=1
        fi
    done <<<"$DOCUMENTED"
done

if (( bad )); then
    echo "Documented recipe drifts from the Makefile - see errors above." >&2
    exit 1
fi

echo "OK: every 'make <target>' documented in ${#DOCS[@]} doc file(s) maps to a real Makefile target"
