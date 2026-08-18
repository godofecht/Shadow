#!/usr/bin/env bash
# Assert a PR that changes engine or game code also touches a test file, so
# new behavior always lands with coverage.
#
# Classification:
#   - "code"  = Engine/ and Games/ source files (c/c++ headers + impls).
#   - "test"  = any file under tests/ (the unit suite lives there; games are
#               covered by tests/test_games.cpp, which #includes each game's
#               main.cpp and drives it headlessly).
#
# Only added / modified / renamed paths count (--diff-filter=ACMR): a pure
# deletion needs no new coverage, so deleting dead code alone is allowed.
# Docs-only PRs (README, cmake, .github, ...) change no code and pass.
#
# The diff is three-dot (base...head): it compares against the merge-base of
# base and head, i.e. exactly the changes THIS PR introduces even if the
# target branch has advanced since the branch was cut.
#
# bash 3.2 compatible (macOS default bash).
#
# Usage: tools/check_test_coverage.sh <base> [head]
#        base = git ref/SHA of the PR target (e.g. the base branch's SHA)
#        head = the PR head (defaults to HEAD)
# Exit 0 if coverage is satisfied or no code changed, 1 otherwise.

set -euo pipefail

BASE="${1:?usage: tools/check_test_coverage.sh <base> [head]}"
HEAD="${2:-HEAD}"

# Fail loudly on a broken diff (e.g. the base ref isn't fetched) rather than
# silently passing - the same anti-regression posture as the ctest -N guards.
if ! CHANGED="$(git diff --name-only --diff-filter=ACMR "${BASE}...${HEAD}")"; then
    echo "::error::git diff failed for '${BASE}...${HEAD}' - is the base ref available? (checkout with fetch-depth: 0)" >&2
    exit 1
fi

if [[ -z "$CHANGED" ]]; then
    echo "OK: no changed files between ${BASE} and ${HEAD} - nothing to check."
    exit 0
fi

CODE="$(printf '%s\n' "$CHANGED" | grep -E '^(Engine|Games)/.*\.(c|cc|cpp|cxx|h|hh|hpp)$' || true)"
TESTS="$(printf '%s\n' "$CHANGED" | grep -E '^tests/' || true)"

if [[ -z "$CODE" ]]; then
    echo "OK: no engine/game code changed - test coverage not required."
    exit 0
fi

if [[ -n "$TESTS" ]]; then
    n="$(printf '%s\n' "$TESTS" | grep -c .)"
    echo "OK: engine/game code changed and ${n} test file(s) were touched."
    exit 0
fi

echo "::error::this PR changes engine/game code without touching any test file" >&2
echo "Changed code paths:" >&2
printf '%s\n' "$CODE" | sed 's/^/  - /' >&2
echo "Add or update a test under tests/ (games: tests/test_games.cpp)." >&2
echo "See CONTRIBUTING.md for the test-running recipe." >&2
exit 1
