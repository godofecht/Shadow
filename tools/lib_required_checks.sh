#!/usr/bin/env bash
# Shared parser for CONTRIBUTING.md's "Required checks before merge" section.
#
# The single canonical implementation of the structural parse used by all
# three branch-protection tools:
#
#   tools/apply_branch_protection.sh   - applies the checks to GitHub
#   tools/check_branch_protection.sh   - verifies them against the live API
#   tools/check_github_meta.sh         - verifies them against ci.yml jobs
#
# CONTRIBUTING.md is the single source of truth for the LIST of required
# checks; this file is the single source of truth for the PARSE. Before this
# existed, each tool carried its own copy of the awk block and the parsers
# could drift apart even while the list itself stayed correct.
#
# Usage (from a sourcing script):
#
#   source "$ROOT/tools/lib_required_checks.sh"
#   DOC_CHECKS="$(parse_required_checks "$CONTRIBUTING")" || exit 1
#
# parse_required_checks prints one job name per line to stdout and returns 0.
# On failure (missing file, missing section, no bullets) it prints an
# ::error:: line to stderr and returns 1.
#
# bash 3.2 compatible (macOS default bash).

# Idempotent sourcing guard: re-sourcing the file is a no-op.
if [[ -z "${UMBRA_LIB_REQUIRED_CHECKS_SOURCED:-}" ]]; then
    UMBRA_LIB_REQUIRED_CHECKS_SOURCED=1
else
    return 0
fi

# Extract backtick job names from the bulleted list under the
# "Required checks before merge" section. Only lines shaped like
# "- `jobname` — description" count; the optional "consider macos/windows/
# emscripten" prose line is not a bullet and is ignored.
parse_required_checks() {
    local file="$1"

    if [[ ! -f "$file" ]]; then
        echo "::error::$file not found - cannot parse the required checks" >&2
        return 1
    fi

    local out
    out="$(awk '
        /^## Required checks before merge/ { in_section=1; next }
        in_section && /^## / { exit }
        in_section && /^- `[^`]+`/ {
            line = $0
            sub(/^- `/, "", line)
            sub(/`.*/, "", line)
            print line
        }
    ' "$file")"

    if [[ -z "$out" ]]; then
        echo "::error::Could not parse the required checks from $file's 'Required checks before merge' section - is the heading or its bullet list intact?" >&2
        return 1
    fi

    printf '%s\n' "$out"
}