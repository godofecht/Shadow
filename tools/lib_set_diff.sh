#!/usr/bin/env bash
# Shared set-difference helper for the branch-protection tools.
#
# The single canonical implementation of the sort/grep -Fxv -f pattern used to
# compare two newline-delimited lists as sets. Both callers previously carried
# their own copy:
#
#   tools/check_branch_protection.sh   - CONTRIBUTING.md checks vs live GitHub API
#   tools/check_github_meta.sh         - CONTRIBUTING.md bullets vs PR template table
#
# Keeping one implementation means the comparison semantics (order-insensitive,
# deduplicated, exact-line match, empty-safe) can never drift between them.
#
# Usage (from a sourcing script):
#
#   source "$ROOT/tools/lib_set_diff.sh"
#   ONLY_IN_LEFT="$(set_difference "$LEFT" "$RIGHT")"
#
# set_difference prints each line present in LEFT but not in RIGHT, one per
# line, sorted and deduplicated. It never fails on an empty result (grep -v
# returns 1 when nothing differs, which would otherwise abort a script under
# `set -euo pipefail`), so a clean match yields empty stdout and exit 0.
#
# bash 3.2 compatible (macOS default bash).

# Idempotent sourcing guard: re-sourcing the file is a no-op.
if [[ -z "${UMBRA_LIB_SET_DIFF_SOURCED:-}" ]]; then
    UMBRA_LIB_SET_DIFF_SOURCED=1
else
    return 0
fi

# Print lines in LEFT that are not in RIGHT. Both arguments are
# newline-delimited strings; ordering and duplicate lines in the input are
# irrelevant (both sides are sorted -u before the comparison).
set_difference() {
    local left="$1" right="$2"
    # `grep -Fxv -f` selects exact whole lines (-x -F) that are NOT (-v) in the
    # pattern file. An empty pattern file matches nothing, so -v prints every
    # line in LEFT - correct, since nothing is "in RIGHT".
    printf '%s\n' "$left" | sort -u | grep -Fxv -f <(printf '%s\n' "$right" | sort -u) || true
}
