#!/usr/bin/env bash
# Verify the repo's GitHub metadata can't drift from its sources of truth:
#
#   1. Required checks: the merge-gate lists in CONTRIBUTING.md's "Required
#      checks before merge" section (bullets) and the PR template's
#      "Required CI checks before merge" table must all exist as job keys in
#      .github/workflows/ci.yml. Both are structural parses - not a
#      job-shape heuristic - so a typo'd name like 'lnux' or 'test-coevrage'
#      is caught even though it no longer looks like a CI job. Both lists
#      must also hold exactly EXPECTED_CHECK_COUNT entries (default-on), so
#      a deleted bullet or row fails even though the survivors all resolve,
#      and the two lists are compared as sets so they can't swap one job for
#      another while keeping the same count.
#   2. Job references: every job-shaped backtick token in the .github/*.md
#      templates and CONTRIBUTING.md (e.g. the memory checker matrix) must
#      exist as a job key too, so a renamed or removed job can't leave a
#      stale reference behind.
#   3. Handles: every @handle (individual or @org/team) referenced in
#      .github/CODEOWNERS must be declared in the canonical MAINTAINERS file
#      at the repo root, so review routing can never point at a misspelled or
#      made-up account.
#
# Both directions are one-way on purpose: a template may omit a job (not every
# job needs to appear in a checklist) and MAINTAINERS may list people not yet
# wired into CODEOWNERS - but a reference may never point at something that
# does not exist. No network and no build are needed, so this is safe to run
# before every commit.
#
# bash 3.2 compatible (macOS default bash).
#
# Usage:
#   tools/check_github_meta.sh                  # full metadata gate (offline)
#   tools/check_github_meta.sh --expect-count N # override the required-check
#                                               # count (default 7)
#   tools/check_github_meta.sh --help           # this help
# Exit 0 if every reference resolves, 1 otherwise.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKFLOW="$ROOT/.github/workflows/ci.yml"
TEMPLATES_DIR="$ROOT/.github"
CODEOWNERS="$ROOT/.github/CODEOWNERS"
MAINTAINERS="$ROOT/MAINTAINERS"
PR_TEMPLATE="$ROOT/.github/PULL_REQUEST_TEMPLATE.md"
EXTRA_DOCS=()
[[ -f "$ROOT/CONTRIBUTING.md" ]] && EXTRA_DOCS+=("$ROOT/CONTRIBUTING.md")

# Shared parse of CONTRIBUTING.md's required-check list (single implementation).
source "$ROOT/tools/lib_required_checks.sh"
# Shared set-difference helper (single implementation of the sort/grep compare).
source "$ROOT/tools/lib_set_diff.sh"

usage() {
    awk 'NR == 1 { next } /^$/ { exit } { sub(/^# ?/, ""); print }' "$0"
    exit 0
}

# The required-check lists must hold exactly this many entries, enforced on
# every run so make verify-all fails locally too, not just CI. A deleted
# bullet or table row still passes every per-name check above (the survivors
# all resolve), so the count is pinned here - the single source of truth for
# the number. Bump it when you intentionally add or remove a required job
# (and update both docs).
EXPECTED_CHECK_COUNT=7

# --expect-count N overrides EXPECTED_CHECK_COUNT (kept for edge cases/tests).
EXPECT_COUNT=$EXPECTED_CHECK_COUNT
while (( $# )); do
    case "$1" in
        --expect-count)
            shift
            [[ $# -ge 1 ]] || { echo "::error::--expect-count requires a number" >&2; exit 1; }
            EXPECT_COUNT="$1"
            ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done

if ! [[ "$EXPECT_COUNT" =~ ^[0-9]+$ ]]; then
    echo "::error::--expect-count must be a non-negative integer (got '$EXPECT_COUNT')" >&2
    exit 1
fi

bad=0

# --- 1. Real job keys: parse the workflow's jobs: mapping -----------------
if [[ ! -f "$WORKFLOW" ]]; then
    echo "::error::workflow not found (expected at $WORKFLOW)" >&2
    exit 1
fi

# Real job keys: the 2-space-indented keys under the top-level `jobs:` mapping.
# (`on:` also uses 2-space keys - push:/pull_request: - so only parse from the
# jobs: line onward, and stop at the next 0-indent top-level key.)
JOBS="$(awk '/^jobs:[[:space:]]*$/{j=1; next}
             j && /^[A-Za-z0-9_-]+:[[:space:]]*$/{exit}
             j && /^  [A-Za-z0-9_-]+:[[:space:]]*$/{sub(/^  /,""); sub(/:.*/,""); print}' "$WORKFLOW")"
if [[ -z "$JOBS" ]]; then
    echo "::error::no jobs parsed from $WORKFLOW - workflow malformed or the parser regressed?" >&2
    exit 1
fi

# --- 2. Required checks: structural parse -> real job keys ------------------
# CONTRIBUTING.md's "Required checks before merge" section lists the merge
# gates as bullets ("- `jobname` — description"). Parse those directly (not
# via the job-shape heuristic below) so a typo that no longer looks like a CI
# job is still caught. The parse is shared with the other branch-protection
# tools via tools/lib_required_checks.sh - CONTRIBUTING.md is the single
# source of truth for the list, the helper for the parse.
if [[ ! -f "$ROOT/CONTRIBUTING.md" ]]; then
    echo "::error::CONTRIBUTING.md not found (expected at $ROOT/CONTRIBUTING.md)" >&2
    exit 1
fi

REQ_CHECKS="$(parse_required_checks "$ROOT/CONTRIBUTING.md")" || exit 1

while IFS= read -r job; do
    [[ -n "$job" ]] || continue
    if ! grep -qxF "$job" <<<"$JOBS"; then
        echo "::error::CONTRIBUTING.md lists '$job' as a required merge check but no such job exists in $WORKFLOW - fix the typo or rename the job" >&2
        bad=1
    fi
done <<<"$REQ_CHECKS"

# --- 2b. PR template's required-checks table -> real job keys ---------------
# The PR template's "### Required CI checks before merge" section lists the
# same merge gates in a markdown table ("| `jobname` | description |"). Parse
# the job column structurally too: the table is what contributors see on every
# PR, and the job-shape heuristic below can't catch a typo there that no
# longer looks like a CI job (e.g. 'test-coevrage').
if [[ ! -f "$PR_TEMPLATE" ]]; then
    echo "::error::PR template not found (expected at $PR_TEMPLATE)" >&2
    exit 1
fi

PR_CHECKS="$(awk '
    /^### Required CI checks before merge/ { in_section=1; next }
    in_section && /^#/ { exit }
    in_section && /^\|/ && /`/ {
        line = $0
        sub(/^[^`]*`/, "", line)
        sub(/`.*/, "", line)
        print line
    }
' "$PR_TEMPLATE")"

if [[ -z "$PR_CHECKS" ]]; then
    echo "::error::could not parse the required-checks table from $PR_TEMPLATE's '### Required CI checks before merge' section - heading or table intact?" >&2
    exit 1
fi

while IFS= read -r job; do
    [[ -n "$job" ]] || continue
    if ! grep -qxF "$job" <<<"$JOBS"; then
        echo "::error::$PR_TEMPLATE lists '$job' as a required merge check but no such job exists in $WORKFLOW - fix the typo or rename the job" >&2
        bad=1
    fi
done <<<"$PR_CHECKS"

# --- 2c. Required-check count: the lists can never silently shrink ----------
# Every per-name check above passes when a bullet or table row is deleted
# (the survivors all resolve), so the count is pinned (default-on) to fail
# loudly on a silently shrunken list. EXPECTED_CHECK_COUNT (top of file) is
# the single source of truth for the number.
c_req="$(printf '%s\n' "$REQ_CHECKS" | grep -c .)"
c_pr="$(printf '%s\n' "$PR_CHECKS" | grep -c .)"
if (( c_req != EXPECT_COUNT )); then
    echo "::error::CONTRIBUTING.md lists $c_req required checks, expected exactly $EXPECT_COUNT - the list shrank or grew; fix the docs or update EXPECTED_CHECK_COUNT in tools/check_github_meta.sh" >&2
    bad=1
fi
if (( c_pr != EXPECT_COUNT )); then
    echo "::error::PR template lists $c_pr required checks, expected exactly $EXPECT_COUNT - the list shrank or grew; fix the docs or update EXPECTED_CHECK_COUNT in tools/check_github_meta.sh" >&2
    bad=1
fi

# --- 2d. Required-check set equality: the two lists can't disagree ----------
# Equal counts are necessary but not sufficient: one list could swap in a
# different (still-real) job and keep the same total. Compare the two parsed
# lists as SETS in both directions, so CONTRIBUTING.md's bullets and the PR
# template's table name the exact same jobs (order-insensitive).
ONLY_IN_CONTRIBUTING="$(set_difference "$REQ_CHECKS" "$PR_CHECKS")"
ONLY_IN_PR_TEMPLATE="$(set_difference "$PR_CHECKS" "$REQ_CHECKS")"

if [[ -n "$ONLY_IN_CONTRIBUTING" ]]; then
    echo "::error::required check(s) in CONTRIBUTING.md but missing from the PR template table:" >&2
    while IFS= read -r c; do
        [[ -n "$c" ]] && echo "::error::  - $c" >&2
    done <<<"$ONLY_IN_CONTRIBUTING"
    bad=1
fi
if [[ -n "$ONLY_IN_PR_TEMPLATE" ]]; then
    echo "::error::required check(s) in the PR template table but missing from CONTRIBUTING.md:" >&2
    while IFS= read -r c; do
        [[ -n "$c" ]] && echo "::error::  - $c" >&2
    done <<<"$ONLY_IN_PR_TEMPLATE"
    bad=1
fi

# --- 3. Job references: job-shape heuristic -> real job keys ----------------
# Every backtick token in the .github/*.md templates. A token is treated as a
# job reference only if it is shaped like a CI job (bare lowercase-hyphen with
# a platform prefix or a job-type suffix); other tokens (make targets, paths,
# commands) are ignored so plain prose never trips the check.
shopt -s nullglob
TEMPLATES=("$TEMPLATES_DIR"/*.md)
shopt -u nullglob
if (( ${#TEMPLATES[@]} == 0 )); then
    echo "::error::no .github/*.md templates found - parse failed or the files were removed?" >&2
    exit 1
fi

# All doc files to scan for job references: .github templates + CONTRIBUTING.md
# (whose branch-protection section names required status checks). BUILD_AND_TEST.md
# is excluded because it references cmake/ctest targets (like memcheck-examples)
# that happen to match the job-name pattern but are not CI jobs.
ALL_DOCS=("${TEMPLATES[@]}" "${EXTRA_DOCS[@]}")

for tpl in "${ALL_DOCS[@]}"; do
    TOKENS="$(grep -oE '`[^`]*`' "$tpl" | sed -e 's/^`//' -e 's/`$//' | sort -u)"
    while IFS= read -r tok; do
        [[ -n "$tok" ]] || continue
        if printf '%s\n' "$tok" | grep -qE '^[a-z][a-z0-9-]*$' \
           && printf '%s\n' "$tok" | grep -qE '^(linux|macos|windows|emscripten)(-|$)|-(sanitize|ubsan|valgrind|examples|coverage)$'; then
            if ! grep -qxF "$tok" <<<"$JOBS"; then
                echo "::error::$tpl references job '$tok' but no such job exists in $WORKFLOW - rename the job or fix the doc" >&2
                bad=1
            fi
        fi
    done <<<"$TOKENS"
done

# --- 4. Handles: CODEOWNERS references -> MAINTAINERS -----------------------
if [[ ! -f "$CODEOWNERS" ]]; then
    echo "::error::CODEOWNERS not found (expected at $CODEOWNERS)" >&2
    exit 1
fi
if [[ ! -f "$MAINTAINERS" ]]; then
    echo "::error::MAINTAINERS not found (expected at $MAINTAINERS) - the canonical GitHub handle list CODEOWNERS must draw from" >&2
    exit 1
fi

ALLOWED="$(grep -vE '^[[:space:]]*(#|$)' "$MAINTAINERS" | awk '{print $1}' | sort -u)"
if [[ -z "$ALLOWED" ]]; then
    echo "::error::MAINTAINERS is empty - it must list at least one real GitHub handle" >&2
    exit 1
fi

# Non-comment CODEOWNERS lines, inline comments stripped. Every line must carry
# a pattern AND at least one @owner; then every owner must resolve.
LINES="$(grep -vE '^[[:space:]]*#' "$CODEOWNERS" | sed -e 's/#.*//' -e '/^[[:space:]]*$/d')"
if [[ -z "$LINES" ]]; then
    echo "::error::no routing rules found in $CODEOWNERS - parse failed?" >&2
    exit 1
fi

while IFS= read -r line; do
    if ! printf '%s\n' "$line" | grep -qE '@[A-Za-z0-9_-]+(/[A-Za-z0-9_-]+)?'; then
        echo "::error::$CODEOWNERS rule has no owner: $line" >&2
        bad=1
    fi
done <<<"$LINES"

REFS="$(printf '%s\n' "$LINES" | grep -oE '@[A-Za-z0-9_-]+(/[A-Za-z0-9_-]+)?' | sort -u)"
while IFS= read -r ref; do
    org="${ref#@}"          # strip the leading @
    org="${org%%/*}"        # individual, or the org half of an @org/team
    if ! grep -qxF "$org" <<<"$ALLOWED"; then
        echo "::error::$CODEOWNERS references '$ref' but '$org' is not listed in MAINTAINERS - add the real handle there or fix CODEOWNERS" >&2
        bad=1
    fi
done <<<"$REFS"

if (( bad )); then
    echo "GitHub metadata drifts from its sources of truth - see errors above." >&2
    exit 1
fi

echo "OK: required checks resolve to real jobs (CONTRIBUTING.md bullets + PR template table), every job reference in the docs is real, and every CODEOWNERS handle is in MAINTAINERS"
