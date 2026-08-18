#!/usr/bin/env bash
# Offline regression tests for tools/check_github_meta.sh.
#
# The gate itself is offline (no network, no build, no CMake), so its tests are
# too. Each scenario rebuilds a scratch fixture tree from the repo's real
# GitHub metadata, applies one mutation, and asserts the gate's exit code and
# diagnostic. Every check in tools/check_github_meta.sh has a scenario:
#
#   1. baseline         - pristine metadata passes (pins the gate AND the real
#                         files, so drift in either shows up here)
#   2. missing job      - a required check names a job absent from ci.yml
#   3. wrong count      - a required-check list silently shrinks
#   4. set disagreement - the two merge-gate lists differ while keeping the
#                         same count and only real job names
#   5. heuristic drift  - a job-shaped backtick reference (outside the
#                         required-check sections) names a job absent from
#                         ci.yml
#   6. unknown handle   - CODEOWNERS references a handle not in MAINTAINERS
#   7. owner-less rule  - a CODEOWNERS rule carries no @owner
#
# Usage:
#   tools/test_check_github_meta.sh   # run all scenarios (exit 0 iff all pass)
#
# bash 3.2 compatible (macOS default bash).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/check_github_meta_test.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
FIXTURE="$WORK/tree"

passes=0
failures=0

# Rebuild a pristine fixture tree from the real metadata. The gate resolves
# everything relative to its own location, so the fixture must carry the gate
# under tools/ and the metadata at its root.
make_fixture() {
    rm -rf "$FIXTURE"
    mkdir -p "$FIXTURE"
    cp -R "$ROOT/.github" "$FIXTURE/"
    cp -R "$ROOT/tools" "$FIXTURE/"
    cp "$ROOT/MAINTAINERS" "$ROOT/CONTRIBUTING.md" "$FIXTURE/"
}

# in_place <file> <sed-expr> — portable `sed -i` (macOS sed wants no inline
# arg, so write to a temp file and move it back).
in_place() {
    local file="$1" expr="$2"
    sed "$expr" "$file" > "$file.tmp" && mv "$file.tmp" "$file"
}

# Run the gate in the fixture and capture its exit code + combined output.
# The `if` form keeps `set -e` from aborting the harness on the expected
# exit-1 scenarios.
run_gate() {
    local out
    if out="$(bash "$FIXTURE/tools/check_github_meta.sh" 2>&1)"; then
        GATE_RC=0
    else
        GATE_RC=$?
    fi
    GATE_OUT="$out"
}

# assert <expected_exit> <needle> <description>
assert() {
    local expected_exit="$1" needle="$2" desc="$3"
    run_gate
    if (( GATE_RC != expected_exit )); then
        echo "FAIL: $desc - gate exited $GATE_RC, wanted $expected_exit" >&2
        printf '%s\n' "$GATE_OUT" | sed 's/^/    /' >&2
        failures=$((failures + 1))
        return
    fi
    if [[ -n "$needle" ]] && ! printf '%s\n' "$GATE_OUT" | grep -qF "$needle"; then
        echo "FAIL: $desc - output missing expected text '$needle'" >&2
        printf '%s\n' "$GATE_OUT" | sed 's/^/    /' >&2
        failures=$((failures + 1))
        return
    fi
    echo "PASS: $desc"
    passes=$((passes + 1))
}

# --- 1. Baseline: pristine metadata passes the gate -------------------------
make_fixture
assert 0 "" "pristine metadata passes the gate"

# --- 2. Missing job: a required check names a job absent from ci.yml --------
make_fixture
in_place "$FIXTURE/CONTRIBUTING.md" 's|^- `linux` |- `linux-nonexistent` |'
assert 1 "no such job exists" "a required check naming a missing job is caught"

# --- 3. Wrong count: a required-check list silently shrinks -----------------
# Deleting one bullet leaves every survivor resolving to a real job, so only
# the count pin (EXPECTED_CHECK_COUNT) can catch this.
make_fixture
in_place "$FIXTURE/CONTRIBUTING.md" '/^- `linux` /d'
assert 1 "expected exactly 7" "a shrunken required-check list is caught"

# --- 4. Set disagreement: same count, all-real jobs, but the lists differ ---
# `windows` is a real job, so per-name checks pass and the count stays 7 -
# only the set comparison can catch the swap.
make_fixture
in_place "$FIXTURE/.github/PULL_REQUEST_TEMPLATE.md" 's@^| `linux` |@| `windows` |@'
assert 1 "missing from the PR template table" "set disagreement between the two lists is caught"

# --- 5. Heuristic job reference: a job-shaped token names a missing job -----
# The memory checker matrix references `linux-sanitize` OUTSIDE the
# required-check sections, so the structural parses (2/2b) and the count/set
# checks (2c/2d) all stay green - only the job-shape heuristic (section 3)
# can see the fake `linux-fakejob`. Anchored on the closing paren so the
# required-checks bullet (`linux-sanitize` — ...) is not touched.
make_fixture
in_place "$FIXTURE/CONTRIBUTING.md" 's|`linux-sanitize`)|`linux-fakejob`)|'
assert 1 "references job 'linux-fakejob' but no such job exists" "a job-shaped reference to a missing job is caught"

# --- 6. CODEOWNERS handle: a rule references a handle not in MAINTAINERS ----
make_fixture
in_place "$FIXTURE/.github/CODEOWNERS" 's/@godofecht/@fakeowner/g'
assert 1 "is not listed in MAINTAINERS" "a CODEOWNERS handle missing from MAINTAINERS is caught"

# --- 7. Owner-less CODEOWNERS rule ------------------------------------------
# Every non-comment rule must carry at least one @owner; strip the owner off
# the catch-all line so only the no-owner check fires (the other rules still
# reference @godofecht, which IS in MAINTAINERS).
make_fixture
in_place "$FIXTURE/.github/CODEOWNERS" 's/^\(\* *\)@godofecht/\1/'
assert 1 "rule has no owner" "a CODEOWNERS rule without an owner is caught"

echo
if (( failures == 0 )); then
    echo "OK: all check_github_meta.sh scenarios passed ($passes)"
else
    echo "FAIL: $failures check_github_meta.sh scenario(s) failed" >&2
    exit 1
fi
