#!/usr/bin/env bash
# Verify the live GitHub branch protection on the default branch (main)
# matches CONTRIBUTING.md's "Required checks before merge" section, so the
# repo and GitHub itself can't drift apart. This is the read-only inverse of
# tools/apply_branch_protection.sh: apply WRITES the rule, this script only
# READS it back and fails if it no longer matches.
#
# Checks (all read-only):
#   1. Branch protection exists on the branch.
#   2. The required status checks enforced on GitHub equal the bulleted
#      backtick jobs in CONTRIBUTING.md's section, in both directions - a job
#      missing from GitHub or an extra one enforced only on GitHub is drift.
#   3. "Require a pull request before merging" is enabled with at least one
#      approving review.
#
# Requires: gh CLI (https://cli.github.com/) authenticated with read access
# to the repo. Because it hits the network it is deliberately NOT part of the
# offline `make verify-all` gates.
#
# Usage:
#   tools/check_branch_protection.sh                 # check main
#   tools/check_branch_protection.sh --branch main   # check another branch
#   tools/check_branch_protection.sh --help          # this help
#
# bash 3.2 compatible (macOS default bash).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONTRIBUTING="$ROOT/CONTRIBUTING.md"

# Shared parse of CONTRIBUTING.md's required-check list (single implementation).
source "$ROOT/tools/lib_required_checks.sh"
# Shared set-difference helper (single implementation of the sort/grep compare).
source "$ROOT/tools/lib_set_diff.sh"

usage() {
    awk 'NR == 1 { next } /^$/ { exit } { sub(/^# ?/, ""); print }' "$0"
    exit 0
}

BRANCH="main"
while (( $# )); do
    case "$1" in
        --branch)
            shift
            [[ $# -ge 1 ]] || { echo "::error::--branch requires a name" >&2; exit 1; }
            BRANCH="$1"
            ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done

if ! command -v gh >/dev/null 2>&1; then
    echo "::error::gh CLI not found — install it from https://cli.github.com/" >&2
    exit 1
fi

# --- 1. Parse the required checks out of CONTRIBUTING.md --------------------
# Shared with apply_branch_protection.sh and check_github_meta.sh via
# tools/lib_required_checks.sh - CONTRIBUTING.md is the single source of truth
# for the list, and the helper is the single source of truth for the parse.
DOC_CHECKS="$(parse_required_checks "$CONTRIBUTING")" || exit 1

# --- 2. Resolve the repository ----------------------------------------------
REPO="$(gh repo view --json nameWithOwner -q .nameWithOwner 2>/dev/null || true)"
if [[ -z "$REPO" ]]; then
    echo "::error::Could not determine the GitHub repository — are you inside a git checkout with a gh-authenticated remote?" >&2
    exit 1
fi

bad=0

# --- 3. Branch protection must exist ----------------------------------------
if ! gh api "repos/$REPO/branches/$BRANCH/protection" >/dev/null 2>&1; then
    echo "::error::No branch protection on '$BRANCH' of $REPO (or no API access) — run 'make protect-branch' first." >&2
    exit 1
fi

# --- 4. Required status checks: exact match in both directions ---------------
# Live contexts, one per line, sorted and deduplicated. `// []` turns a null
# `required_status_checks` into an empty list instead of an error.
LIVE="$(gh api "repos/$REPO/branches/$BRANCH/protection" \
    --jq '(.required_status_checks.contexts // [])[]' 2>/dev/null | sort -u || true)"

MISSING="$(set_difference "$DOC_CHECKS" "$LIVE")"
EXTRA="$(set_difference "$LIVE" "$DOC_CHECKS")"

if [[ -n "$MISSING" ]]; then
    echo "::error::Required checks in CONTRIBUTING.md but NOT enforced on GitHub:" >&2
    while IFS= read -r c; do
        [[ -n "$c" ]] && echo "::error::  - $c" >&2
    done <<<"$MISSING"
    bad=1
fi
if [[ -n "$EXTRA" ]]; then
    echo "::error::Status checks enforced on GitHub but NOT listed in CONTRIBUTING.md:" >&2
    while IFS= read -r c; do
        [[ -n "$c" ]] && echo "::error::  - $c" >&2
    done <<<"$EXTRA"
    bad=1
fi

# --- 5. "Require a pull request before merging" with >= 1 approval ----------
PR_COUNT="$(gh api "repos/$REPO/branches/$BRANCH/protection" \
    --jq '(.required_pull_request_reviews.required_approving_review_count // 0)' 2>/dev/null || true)"

if [[ "$PR_COUNT" =~ ^[0-9]+$ ]]; then
    if (( PR_COUNT < 1 )); then
        echo "::error::'Require a pull request before merging' is not enabled with at least one approving review (required_approving_review_count=$PR_COUNT)." >&2
        bad=1
    fi
else
    echo "::error::Could not read required_pull_request_reviews from GitHub (got '$PR_COUNT')." >&2
    bad=1
fi

DOC_COUNT="$(printf '%s\n' "$DOC_CHECKS" | wc -l | tr -d ' ')"
LIVE_COUNT="$(printf '%s\n' "$LIVE" | grep -c . || true)"

if (( bad )); then
    echo "Live branch protection on '$BRANCH' drifts from CONTRIBUTING.md — see errors above." >&2
    echo "Run 'make protect-branch' to re-apply, or update CONTRIBUTING.md." >&2
    exit 1
fi

echo "OK: live branch protection on '$BRANCH' matches CONTRIBUTING.md ($DOC_COUNT required checks, $LIVE_COUNT enforced, PR reviews required)."
