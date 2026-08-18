#!/usr/bin/env bash
# Offline tests for tools/apply_branch_protection.sh.
#
# The apply script's API path is normally impossible to exercise without a
# real GitHub token, so this harness runs it against a stub `gh` that logs
# every invocation. Scenarios:
#
#   1. Default apply makes exactly the two PUT calls - one to
#      required_pull_request_reviews (dismiss_stale_reviews=true,
#      require_code_owner_reviews=false, required_approving_review_count=1)
#      and one to required_status_checks (strict_status_checks=true plus one
#      contexts[]= arg per required check parsed from CONTRIBUTING.md).
#   2. --approvals N and --code-owner flow into the PR-reviews call.
#   3. --dry-run never calls the API (only gh repo view).
#   4. --approvals 0 is rejected before any gh call.
#
# CONTRIBUTING.md is copied into the fixture - it is the single source of
# truth for the required checks - so the parsed checks are the real ones.
# The expected context count (7) must stay in sync with EXPECTED_CHECK_COUNT
# in tools/check_github_meta.sh.
#
# Usage:
#   tools/test_apply_branch_protection.sh   # run all scenarios (exit 0 iff all pass)
#
# bash 3.2 compatible (macOS default bash).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/apply_branch_protection_test.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

# Fixture: the apply script resolves ROOT from its own location, so the
# scratch tree must carry the script + shared lib under tools/ and the real
# CONTRIBUTING.md at its root (the source of the parsed checks).
mkdir -p "$WORK/tree/tools" "$WORK/bin"
cp "$ROOT/tools/apply_branch_protection.sh" "$WORK/tree/tools/"
cp "$ROOT/tools/lib_required_checks.sh" "$WORK/tree/tools/"
cp "$ROOT/CONTRIBUTING.md" "$WORK/tree/"

# Stub gh: answers `repo view` with a fixed owner/repo, treats every `api`
# PUT as a success, and logs each invocation (args space-joined) to
# $GH_STUB_LOG so the test can assert the exact calls.
cat > "$WORK/bin/gh" <<'EOF'
#!/usr/bin/env bash
set -u
printf '%s\n' "$*" >> "${GH_STUB_LOG:?}"
case "$1" in
    repo) echo "stubowner/stubrepo" ;;
    api) : ;;
esac
exit 0
EOF
chmod +x "$WORK/bin/gh"

passes=0
failures=0

# run_apply <args...> — run the script against the stub gh, capture rc/out/log.
run_apply() {
    rm -f "$WORK/gh.log" "$WORK/out.log"
    if GH_STUB_LOG="$WORK/gh.log" PATH="$WORK/bin:$PATH" \
        bash "$WORK/tree/tools/apply_branch_protection.sh" "$@" >"$WORK/out.log" 2>&1; then
        APPLY_RC=0
    else
        APPLY_RC=$?
    fi
    APPLY_OUT="$(cat "$WORK/out.log")"
    APPLY_LOG="$(cat "$WORK/gh.log" 2>/dev/null || true)"
}

assert_rc() {
    local expected="$1" desc="$2"
    if (( APPLY_RC != expected )); then
        echo "FAIL: $desc - exit $APPLY_RC, wanted $expected" >&2
        echo "--- output ---" >&2
        printf '%s\n' "$APPLY_OUT" | sed 's/^/    /' >&2
        failures=$((failures + 1))
        return
    fi
    echo "PASS: $desc"
    passes=$((passes + 1))
}

# assert_contains <out|log> <needle> <desc>
assert_contains() {
    local stream="$1" needle="$2" desc="$3"
    local haystack
    if [[ "$stream" == out ]]; then haystack="$APPLY_OUT"; else haystack="$APPLY_LOG"; fi
    if ! printf '%s\n' "$haystack" | grep -qF -- "$needle"; then
        echo "FAIL: $desc (missing '$needle' in $stream)" >&2
        if [[ "$stream" == out ]]; then
            echo "--- output ---" >&2
            printf '%s\n' "$APPLY_OUT" | sed 's/^/    /' >&2
        else
            echo "--- gh log ---" >&2
            printf '%s\n' "$APPLY_LOG" | sed 's/^/    /' >&2
        fi
        failures=$((failures + 1))
        return
    fi
    echo "PASS: $desc"
    passes=$((passes + 1))
}

# --- 1. Default apply: both API calls with the parsed checks and defaults ---
run_apply
assert_rc 0 "default apply exits 0"
assert_contains log "repos/stubowner/stubrepo/branches/main/protection/required_pull_request_reviews" \
    "PR-reviews endpoint is called"
assert_contains log "-F dismiss_stale_reviews=true" "dismiss stale reviews is sent"
assert_contains log "-F require_code_owner_reviews=false" "code-owner reviews defaults to off"
assert_contains log "-F required_approving_review_count=1" "one approving review by default"
assert_contains log "repos/stubowner/stubrepo/branches/main/protection/required_status_checks" \
    "status-checks endpoint is called"
assert_contains log "-F strict_status_checks=true" "strict checks is sent"

# Every required check parsed from CONTRIBUTING.md flows as a contexts[]= arg.
ctx_count="$(printf '%s\n' "$APPLY_LOG" | grep -oF 'contexts[]=' | wc -l | tr -d ' ')"
if [[ "$ctx_count" != "7" ]]; then
    echo "FAIL: status call carries $ctx_count contexts[]= args, expected 7 (the required checks in CONTRIBUTING.md)" >&2
    failures=$((failures + 1))
else
    echo "PASS: status call carries exactly 7 contexts[]= args (the parsed required checks)"
    passes=$((passes + 1))
fi
assert_contains log "-f contexts[]=test-coverage" "first parsed check (test-coverage) is sent"
# Anchored at end-of-line: `linux` is a prefix of `linux-sanitize`, so a
# bare substring match would not prove the real `linux` entry arrived.
if printf '%s\n' "$APPLY_LOG" | grep -q 'contexts\[\]=linux$'; then
    echo "PASS: last parsed check (linux) is sent"
    passes=$((passes + 1))
else
    echo "FAIL: status call is missing the final '-f contexts[]=linux' arg" >&2
    echo "--- gh log ---" >&2
    printf '%s\n' "$APPLY_LOG" | sed 's/^/    /' >&2
    failures=$((failures + 1))
fi

# --- 2. --approvals N and --code-owner flow into the PR-reviews call --------
run_apply --approvals 2 --code-owner
assert_rc 0 "approvals/code-owner apply exits 0"
assert_contains log "-F required_approving_review_count=2" "--approvals 2 reaches the API"
assert_contains log "-F require_code_owner_reviews=true" "--code-owner reaches the API"

# --- 3. --dry-run never calls the API ----------------------------------------
run_apply --dry-run
assert_rc 0 "--dry-run exits 0"
assert_contains out "[dry-run]" "dry-run plan is printed"
if printf '%s\n' "$APPLY_LOG" | grep -q '^api '; then
    echo "FAIL: --dry-run still called the API (gh log: $APPLY_LOG)" >&2
    failures=$((failures + 1))
else
    echo "PASS: --dry-run makes no API calls"
    passes=$((passes + 1))
fi

# --- 4. Invalid --approvals is rejected before any gh call -------------------
run_apply --approvals 0
assert_rc 1 "--approvals 0 is rejected"
assert_contains out "positive integer" "validation error names the constraint"
if [[ -n "$APPLY_LOG" ]]; then
    echo "FAIL: invalid approvals still called gh ($APPLY_LOG)" >&2
    failures=$((failures + 1))
else
    echo "PASS: invalid approvals make no gh calls"
    passes=$((passes + 1))
fi

echo
if (( failures == 0 )); then
    echo "OK: all apply_branch_protection.sh scenarios passed ($passes)"
else
    echo "FAIL: $failures apply_branch_protection.sh scenario(s) failed" >&2
    exit 1
fi
