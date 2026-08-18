#!/usr/bin/env bash
# Apply branch protection rules to the default branch (main) via the GitHub
# API, turning the prose guidance in CONTRIBUTING.md's "Required checks before
# merge" section into enforceable repo config - in one command:
#
#   1. Require a pull request before merging, with a minimum number of
#      approving reviews (dismissing stale reviews when new commits land).
#   2. Require the status checks listed below to pass.
#
# Requires: gh CLI (https://cli.github.com/) authenticated with a token that
# has admin:repo_hook permission. The script is idempotent - re-running it
# after adding or removing a job name updates the existing rule in place.
#
# The required status checks are parsed from CONTRIBUTING.md's "Required
# checks before merge" section - that file is the single source of truth for
# apply (this script), check (tools/check_branch_protection.sh), and the meta
# gate (tools/check_github_meta.sh). Edit CONTRIBUTING.md, then re-run this
# script to push the change to GitHub.
#
# Usage:
#   tools/apply_branch_protection.sh                 # apply to main
#   tools/apply_branch_protection.sh --dry-run       # preview, apply nothing
#   tools/apply_branch_protection.sh --approvals 2   # require 2 approvals
#   tools/apply_branch_protection.sh --branch main   # protect another branch
#
# Options:
#   --dry-run        print the planned changes and exit without applying
#   --approvals N    required_approving_review_count (default 1)
#   --branch NAME    branch to protect (default main)
#   --code-owner     also require code-owner reviews (default off)
#   -h, --help       print this help and exit
#
# bash 3.2 compatible (macOS default bash).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Shared parse of CONTRIBUTING.md's required-check list (single implementation).
source "$ROOT/tools/lib_required_checks.sh"

usage() {
    awk 'NR == 1 { next } /^$/ { exit } { sub(/^# ?/, ""); print }' "$0"
    exit 0
}

DRY_RUN=0
APPROVALS=1
BRANCH="main"
CODE_OWNER=0

while (( $# )); do
    case "$1" in
        --dry-run) DRY_RUN=1 ;;
        --code-owner) CODE_OWNER=1 ;;
        --approvals)
            shift
            [[ $# -ge 1 ]] || { echo "::error::--approvals requires a number" >&2; exit 1; }
            APPROVALS="$1"
            ;;
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

if ! [[ "$APPROVALS" =~ ^[0-9]+$ ]] || ! [ "$APPROVALS" -ge 1 ]; then
    echo "::error::--approvals must be a positive integer (got '$APPROVALS')" >&2
    exit 1
fi

# Check for gh CLI
if ! command -v gh >/dev/null 2>&1; then
    echo "::error::gh CLI not found — install it from https://cli.github.com/" >&2
    exit 1
fi

# Determine the repo (owner/repo) from the git remote
REPO="$(gh repo view --json nameWithOwner -q .nameWithOwner 2>/dev/null || true)"
if [[ -z "$REPO" ]]; then
    echo "::error::Could not determine the GitHub repository — are you inside a git checkout with a gh-authenticated remote?" >&2
    exit 1
fi

# Parse the required status checks out of CONTRIBUTING.md's "Required checks
# before merge" section via the shared helper (single implementation for
# apply, check, and the meta gate).
CONTRIBUTING="$ROOT/CONTRIBUTING.md"
DOC_CHECKS="$(parse_required_checks "$CONTRIBUTING")" || exit 1

# bash 3.2 has no mapfile/readarray, so build the array with a read loop.
REQUIRED_CHECKS=()
while IFS= read -r line; do
    [[ -n "$line" ]] && REQUIRED_CHECKS+=("$line")
done <<<"$DOC_CHECKS"

if (( DRY_RUN )); then
    echo "[dry-run] Repository: $REPO, branch: $BRANCH"
    echo "[dry-run] 1. Require a pull request before merging:"
    echo "[dry-run]      - minimum approving reviews: $APPROVALS"
    echo "[dry-run]      - dismiss stale reviews: true"
    echo "[dry-run]      - require code-owner reviews: $([[ $CODE_OWNER -eq 1 ]] && echo true || echo false)"
    echo "[dry-run] 2. Require status checks:"
    for check in "${REQUIRED_CHECKS[@]}"; do
        echo "[dry-run]      - $check"
    done
    echo "[dry-run] Run without --dry-run to apply."
    exit 0
fi

# PUT a branch-protection sub-rule and turn gh's failure into a helpful error.
# gh exits non-zero on any 4xx/5xx and prints the HTTP error to stderr.
run_put() {
    local endpoint="$1" label="$2"
    shift 2
    local out
    if ! out="$(gh api --method PUT \
        "repos/$REPO/branches/$BRANCH/protection/$endpoint" "$@" 2>&1)"; then
        if printf '%s' "$out" | grep -q '404'; then
            echo "::error::$label: branch '$BRANCH' may not exist, or the token lacks admin permission." >&2
        elif printf '%s' "$out" | grep -q '403'; then
            echo "::error::$label: the token lacks permission to modify branch protection on $REPO." >&2
        else
            echo "::error::$label failed: $out" >&2
        fi
        exit 1
    fi
    echo "OK: $label"
}

# 1. Require a pull request before merging + approvals.
if (( CODE_OWNER )); then CODE_OWNER_VAL=true; else CODE_OWNER_VAL=false; fi
PR_ARGS=(
    -F "dismiss_stale_reviews=true"
    -F "require_code_owner_reviews=$CODE_OWNER_VAL"
    -F "required_approving_review_count=$APPROVALS"
)
run_put "required_pull_request_reviews" \
    "require a pull request before merging ($APPROVALS approval(s), dismiss stale reviews)" \
    "${PR_ARGS[@]}"

# 2. Required status checks.
STATUS_ARGS=(-F "strict_status_checks=true")
for check in "${REQUIRED_CHECKS[@]}"; do
    STATUS_ARGS+=(-f "contexts[]=$check")
done
run_put "required_status_checks" \
    "require ${#REQUIRED_CHECKS[@]} status checks" \
    "${STATUS_ARGS[@]}"

echo ""
echo "Done. Consider 'Do not allow bypassing the above settings' (enforce_admins)"
echo "in the GitHub UI if you want to prevent admins from bypassing these rules."
