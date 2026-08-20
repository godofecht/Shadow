#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

find . -maxdepth 1 -type f -name '*.md' -exec cp {} docs/ \;

for license_file in LICENSE LICENSE.*; do
    if [[ -f "$license_file" ]]; then
        cp "$license_file" docs/
    fi
done

mkdir -p docs/Examples
cp Examples/README.md docs/Examples/README.md

printf 'Prepared canonical repository documentation under docs/.\n'
