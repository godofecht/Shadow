## What's the issue?

<!-- One or two sentences: the bug you hit, or the feature you'd like. -->

## Reproduction / context

<!--
For a bug: what did you run, and what happened vs. what you expected?
For a feature: what problem does it solve, and how would you use it?
-->

## Environment

<!-- OS, compiler, SDL2 source (system package / vendored), and whether this
is a native, WASM, or CI build. -->

---

Before opening a PR, please follow the recipe in
[CONTRIBUTING.md](../CONTRIBUTING.md): `make verify-all` → `make native`
→ `ctest --test-dir build --output-on-failure`, and install the pre-commit
hook with `make hooks` so those gates run automatically.
