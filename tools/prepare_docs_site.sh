#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

rm -rf .docs-build
mkdir -p .docs-build
cp -R docs/. .docs-build/

find . -maxdepth 1 -type f -name '*.md' -exec cp {} .docs-build/ \;

for license_file in LICENSE LICENSE.*; do
    if [[ -f "$license_file" ]]; then
        cp "$license_file" .docs-build/
    fi
done

mkdir -p .docs-build/Examples
cp Examples/README.md .docs-build/Examples/README.md

printf 'Prepared documentation staging tree at .docs-build/.\n'
