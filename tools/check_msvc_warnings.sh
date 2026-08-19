#!/usr/bin/env bash
# check_msvc_warnings.sh - local Clang mirror of the MSVC /W4 warnings that
# GCC/Clang's -Wall -Wextra can't see, so the Windows-only warning classes are
# caught on a dev machine before CI. Wired into `make verify-all` as
# `make check-msvc`.
#
# Warning-class mapping (MSVC -> Clang):
#   C4244  float/double -> int / double -> float narrowing  -> -Wfloat-conversion
#   C4267  size_t -> smaller int                            -> -Wshorten-64-to-32
#   C4458  parameter/member hides a class member            -> -Wshadow-field
#
# -Wimplicit-int-float-conversion is deliberately NOT enabled: Clang's
# int->float warning is a strict superset of MSVC's C4244. It flags the
# usual-arithmetic promotion inside `int * float` expressions (e.g.
# `ticks * 0.001f`) that MSVC /W4 intentionally ignores, so enabling it would
# report hundreds of false positives on a tree the Windows CI already accepts.
# The int->float *assignment/argument* form of C4244 (which MSVC does flag)
# remains covered by the Windows CI job itself.
#
# The flags are passed via CMAKE_CXX_FLAGS and escalated to errors by the
# project's own UMBRA_WERROR, so this uses the exact same compile commands the
# normal build does (correct include paths, no manual flag reconstruction).
# A persistent scratch dir under build/ keeps re-runs incremental and fast.
# Like the Windows job, vendored third-party code (box2d, SDL, ...) is also
# compiled with the flags and is warning-clean.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${UMBRA_MSVC_SWEEP_DIR:-$ROOT/build/msvc-sweep}"
MIRROR_FLAGS="-Wfloat-conversion -Wshorten-64-to-32 -Wshadow-field"

if ! command -v clang++ >/dev/null 2>&1; then
    echo "check-msvc: SKIP (clang++ not found; the MSVC-mirror flags are Clang-only)."
    exit 0
fi

JOBS="${UMBRA_MSVC_SWEEP_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 8)}"

cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="$MIRROR_FLAGS" \
    >/dev/null

cmake --build "$BUILD_DIR" --parallel "$JOBS"
