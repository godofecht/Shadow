## Summary

<!-- What does this change do, and why? One or two sentences. -->

## Gates

Every PR must pass the pre-commit gates before it can be merged. Run them
locally (each takes well under a second, no build needed):

- [ ] `make verify-all` — games registered, examples registered, vendored
      layout canonical, docs match the Makefile.
- [ ] `make native` — engine + all examples + tests build warning-free
      (`-Werror` / `/WX`).
- [ ] `ctest --test-dir build --output-on-failure` — the whole test suite
      passes.

If this PR changes engine (`Engine/`) or game (`Games/`) code, it must
also touch a test file under `tests/` — CI fails the PR otherwise, so new
behavior always lands with coverage.

CI runs the same gates (plus the sanitizer / valgrind / WASM jobs and the
test-coverage gate) — a failure here is a failure CI would have caught.
See [CONTRIBUTING.md](../CONTRIBUTING.md) for the full recipe; install the
pre-commit hook once with `make hooks` so the fast gates run automatically
before every commit.

### Required CI checks before merge

These jobs must all pass for the PR to merge (configured in branch protection
on `main`). See CONTRIBUTING.md's **Required checks before merge** section for
the full rationale:

| Job | What it catches |
|---|---|
| `test-coverage` | PR changes engine/game code without touching a test file |
| `linux-sanitize` | ASan + UBSan over tests and headless examples/games |
| `linux-ubsan` | UBSan alone — signed-integer / float-cast overflow |
| `emscripten-sanitize` | ASan over the WASM build path (Node) |
| `linux-valgrind` | Memcheck over the unit tests (uninit reads + leaks) |
| `linux-valgrind-examples` | Memcheck over headless example/game binaries |
| `linux` | Pre-commit gates + warning-free build + full test suite |

Also recommended: `macos`, `windows`, `emscripten` for cross-platform coverage.

## Memory passes

Check the memory job(s) this change exercises, so reviewers can see the
touched code paths got the right checker (CI still runs all of them):

- [ ] **ASan + UBSan** (`linux-sanitize`) — heap/stack memory errors and
      undefined behavior over the unit tests and headless examples/games.
- [ ] **UBSan only** (`linux-ubsan`) — signed-integer / float-cast overflow
      that the combined ASan+UBSan run can mask.
- [ ] **Valgrind memcheck** (`linux-valgrind` / `linux-valgrind-examples`) —
      uninitialized reads and definite / indirect / possibly-lost leaks.
- [ ] **WASM + ASan** (`emscripten-sanitize`) — the browser build path's
      memory errors, run headless in Node.

## Changes

<!-- Bullet list of the notable changes. -->

## Testing

<!-- What did you run to verify this? (targets, ctest filters, manual runs) -->
