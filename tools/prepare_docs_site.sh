#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

mkdir -p docs/generated

cp DOCS.md docs/generated/api.md
cp GAME_DEV_GUIDE.md docs/generated/game-development.md
cp LLM_INTERFACE.md docs/generated/llm-interface.md
cp BUILD_AND_TEST.md docs/generated/build-and-test.md
cp GAMES.md docs/generated/games.md
cp Examples/README.md docs/generated/examples.md

printf 'Prepared canonical documentation under docs/generated/.\n'
