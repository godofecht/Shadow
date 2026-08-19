# Contributing

Thanks for contributing to the Shadow Engine! There's one recipe to follow
for any change, plus a set of fast local gates that keep the tree healthy.

## The one-paragraph flow

Run the pre-commit gates, build everything, then run the tests — in that
order, so a failing gate stops you before a slow build, and a failing build
stops you before the test run:

```bash
make verify-all                 # pre-commit gates (five instant checks + the
                                 #   Clang MSVC-mirror build below)
make native                     # build engine + all examples + tests into build/
ctest --test-dir build --output-on-failure   # run the whole suite
```

If all three pass, you're ready to commit. Install the pre-commit hook once
so the gates run automatically before every commit:

```bash
make hooks                      # one-time setup (installs the pre-commit hook)
```

This is a thin wrapper over `./tools/install-hooks.sh`. Bypass a single
commit with `git commit --no-verify`; uninstall with
`rm .git/hooks/pre-commit`.

## The pre-commit gates

`make verify-all` chains the gates below, and make stops on the first
failure. Five of them are instant (no build); the last (`check-msvc`) is the
one that builds, mirroring the Windows job's warning set with Clang:

- **`make check-games`** — every shipped `Games/` directory must appear in
  `cmake/smoke_targets.txt` (the canonical smoke list), otherwise it ships
  with zero smoke/memcheck/browser CI coverage.
- **`make check-examples`** — every `Examples/` directory must be registered
  via `add_umbra_example` in `CMakeLists.txt`.
- **`make check-layout`** — `dependencies/` is the single canonical vendored
  location: no root-level vendored tree may exist, and no build file may
  reference a root-level vendored path.
- **`make check-docs`** — every `make <target>` command documented in this
  file's recipe must exist as a real target in the Makefile, so the docs
  can't drift from the build surface.
- **`make check-github`** — every CI job name referenced in the
  `.github/*.md` templates exists as a job in the workflow, every `@handle`
  in `.github/CODEOWNERS` is declared in the canonical `MAINTAINERS` file,
  and both required-check lists (CONTRIBUTING.md bullets + PR template table)
  hold the expected entry count (pinned in the script), so routing,
  checklists, and the merge-gate list can't drift or shrink.
- **`make check-msvc`** — builds the whole tree with Clang's MSVC-mirror
  flags (`-Wfloat-conversion`, `-Wshorten-64-to-32`, `-Wshadow-field`), so
  the Windows-only C4244/C4267/C4458 warning classes are caught on a dev
  machine before the Windows job. Reuses a persistent `build/msvc-sweep` dir
  (incremental) and skips if `clang++` is absent (the flags are Clang-only).

These are the same checks the first CI minutes run, so a failure here is a
failure CI would have caught — but in seconds, without leaving your terminal.

### The gates are regression-tested

`make test-github` runs the offline harnesses for the GitHub tooling. The
first (`tools/test_check_github_meta.sh`) exercises `make check-github`: it
rebuilds a scratch fixture from the real `.github/` metadata and asserts the
pristine tree passes while seven mutations (missing job, wrong count, set
disagreement, a job-shaped reference to a missing job, an unknown CODEOWNERS
handle, and an owner-less rule) each fail the gate — so a regression in the
gate itself, or accidental drift in the real files it reads, shows up as a
harness failure instead of a silently weakened gate. The second
(`tools/test_apply_branch_protection.sh`) runs `apply_branch_protection.sh`
against a stub `gh`, asserting its two API calls receive the parsed required
checks and the approval flags — and that `--dry-run` never touches the API.

The same harness is registered in CMake as the `gate_regression` ctest test,
so every `ctest` run in CI drives it too (`ctest -R gate_regression`, or
`ctest -L meta` to list it). CI's linux job runs `make test-github` directly
and asserts (via `ctest -N`) that the test stays registered, mirroring the
`sanitize_path_*` / `smoke_*` guards.

## The memory checker matrix

Beyond the fast gates, CI runs four memory passes. Each one catches a
different class of bug, so a change should ideally exercise the checker(s)
that cover the code paths it touches (CI runs all of them regardless):

- **ASan + UBSan** (`linux-sanitize`) — heap/stack memory errors and
  undefined behavior over the unit tests and headless examples/games.
- **UBSan only** (`linux-ubsan`) — signed-integer / float-cast overflow that
  the combined ASan+UBSan run can mask.
- **Valgrind memcheck** (`linux-valgrind` / `linux-valgrind-examples`) —
  uninitialized reads and definite / indirect / possibly-lost leaks.
- **WASM + ASan** (`emscripten-sanitize`) — the browser build path's memory
  errors, run headless in Node.

This is the same matrix the PR template asks contributors to self-certify
against, so a reviewer can see at a glance which memory job covers a change.

## Required checks before merge (branch protection)

Maintainers should require the following status checks on the default branch
(`main`) so a PR can't merge while its coverage, sanitizer, or memory checks
are failing. In GitHub: **Settings → Branches → Branch protection rules →
Add rule** for `main`, then tick each job under **Require status checks to
pass before merging** (job names must match the workflow exactly; run CI once
on `main` first so they appear in the picker):

- `test-coverage` — PR-only: engine or game code changes must touch a test
  file under `tests/`.
- `linux-sanitize` — ASan + UBSan over tests and headless examples/games.
- `linux-ubsan` — UBSan alone with `-fno-sanitize-recover=all`, catching the
  signed-integer / float-cast overflow the combined run can mask.
- `emscripten-sanitize` — ASan over the WASM build path, run in Node.
- `linux-valgrind` — memcheck over the unit tests (uninitialized reads and
  leaks).
- `linux-valgrind-examples` — memcheck over the headless example/game
  binaries.
- `linux` — the pre-commit gates + a warning-free build + the full test
  suite.

Consider `macos`, `windows`, and `emscripten` too so a Linux-passing change
can't silently break another platform — these are cross-platform coverage,
not merge gates.

### Enforce it in one command

Instead of clicking through the GitHub UI, apply the full rule via the API:

```bash
make protect-branch               # PR reviews + approvals + required status checks
make protect-branch DRY_RUN=1     # preview what would change, apply nothing
make protect-branch APPROVALS=2   # require two approving reviews instead of one
```

This wraps `./tools/apply_branch_protection.sh`, which needs the `gh` CLI
authenticated with `admin:repo_hook` permission. It is idempotent — re-run it
after adding or removing a job name above and the existing rule is updated.
In one command it enables **Require a pull request before merging** with at
least one approving review (dismissing stale reviews when new commits are
pushed) and the required status checks listed above. The required-check list
lives in the script and must match this section; the `make check-github` gate
validates that the job names in this section all exist in the workflow, and
the script applies exactly those names.

To verify the live GitHub protection still matches this section (read-only,
no mutation):

```bash
make check-branch-protection    # fails if GitHub drifts from this section
```

This wraps `./tools/check_branch_protection.sh` and needs `gh` with read
access. It is the inverse of `make protect-branch` — apply writes the rule,
check verifies it — and it checks both directions (a job missing from GitHub
and a job enforced only on GitHub are both drift). Because it hits the
network it is deliberately **not** part of the offline `make verify-all`.

## Notes

- Deeper build/test/valgrind/sanitizer instructions live in
  [BUILD_AND_TEST.md](BUILD_AND_TEST.md); the 100-game program's workflow is
  in [GAME_DEV_GUIDE.md](GAME_DEV_GUIDE.md).
- The engine is dual-licensed (GPL-3.0-or-later or a proprietary
  commercial license with royalties) — see the LICENSE files at the repo
  root. Free GPL games pay nothing; commercial games need a commercial
  license.
