#!/usr/bin/env bash
# Scaffold a new game from a Games/_* starter template.
#
# Usage:
#   tools/new_game.sh MyGame                  # default: collector template
#   tools/new_game.sh --twin-stick MyGame     # arena shooter starter
#   tools/new_game.sh --platformer MyGame     # jump-and-run starter
#   (or: make new-game GAME=MyGame [VARIANT=twin-stick|platformer])
#
# Creates Games/MyGame/ with a complete, playable, LLM-aware starting
# game. CMake auto-registers every Games/*/main.cpp as a <dir>_game
# target, so NO CMake edits are needed - and the game is appended to
# cmake/smoke_targets.txt automatically, so it gets smoke/memcheck/browser
# CI coverage from day one (the CMake configure gate enforces this).
#
#   cmake --build build --target MyGame_game
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# ---- Template selection ---------------------------------------------------
VARIANT=""
while [ $# -gt 0 ]; do
    case "$1" in
        --twin-stick)  VARIANT="twin-stick";  shift ;;
        --platformer)  VARIANT="platformer";  shift ;;
        --*) echo "error: unknown option '$1' (use --twin-stick or --platformer)" >&2; exit 1 ;;
        *) break ;;
    esac
done

case "$VARIANT" in
    ""|collector)
        TEMPLATE="$ROOT/Games/_template"
        TEMPLATE_CLASS="CoinCollector"
        TEMPLATE_TITLE="Coin Collector"
        ;;
    twin-stick)
        TEMPLATE="$ROOT/Games/_twin_stick"
        TEMPLATE_CLASS="TwinStickStarter"
        TEMPLATE_TITLE="Twin Stick Starter"
        ;;
    platformer)
        TEMPLATE="$ROOT/Games/_platformer"
        TEMPLATE_CLASS="PlatformerStarter"
        TEMPLATE_TITLE="Platformer Starter"
        ;;
    *) echo "error: unknown variant '$VARIANT' (use --twin-stick or --platformer)" >&2; exit 1 ;;
esac

NAME="${1:-${NAME:-}}"

if [ -z "$NAME" ]; then
    echo "usage: tools/new_game.sh [--twin-stick|--platformer] <GameName>" >&2
    echo "       (or: make new-game GAME=<GameName> [VARIANT=...])" >&2
    exit 1
fi

# Must start with a LETTER (not digit) so the derived C++ class name and
# window title are always valid identifiers. Dashes are NOT allowed because
# the name becomes the CMake target <name>_game, which must match the
# cmake/smoke_targets.txt read regex ("^[A-Za-z0-9_]+$") - a dashed name
# would silently drop out of the smoke/memcheck/browser CI chain.
if ! [[ "$NAME" =~ ^[A-Za-z][A-Za-z0-9_]*$ ]]; then
    echo "error: '$NAME' is not a valid game name." >&2
    echo "       Use letters, digits or underscores (must start with a letter)." >&2
    echo "       Dashes are not allowed: the name becomes the CMake target" >&2
    echo "       <name>_game, which must stay in cmake/smoke_targets.txt." >&2
    exit 1
fi

if [ ! -d "$TEMPLATE" ]; then
    echo "error: template not found at $TEMPLATE" >&2
    exit 1
fi

DEST="$ROOT/Games/$NAME"
if [ -e "$DEST" ]; then
    echo "error: $DEST already exists." >&2
    exit 1
fi

# my-game -> MyGame (C++ class name) and "My Game" (window title).
CLASS="$(printf '%s' "$NAME" | awk -F'[-_]' '{ for (i=1; i<=NF; i++) printf "%s%s", toupper(substr($i,1,1)), substr($i,2) }')"
TITLE="$(printf '%s' "$NAME" | tr '_-' ' ' | awk '{ for (i=1; i<=NF; i++) printf "%s%s%s", (i>1?" ":""), toupper(substr($i,1,1)), substr($i,2) }')"

cp -R "$TEMPLATE" "$DEST"
rm -f "$DEST/assets/.gitkeep"

# Rename the placeholder class and window title inside the copied main.cpp.
sed -i '' -e "s/$TEMPLATE_CLASS/$CLASS/g" \
          -e "s/$TEMPLATE_TITLE/$TITLE/g" \
          "$DEST/main.cpp" 2>/dev/null || \
sed -i -e "s/$TEMPLATE_CLASS/$CLASS/g" \
       -e "s/$TEMPLATE_TITLE/$TITLE/g" \
       "$DEST/main.cpp"

# Point the copied README at the catalog instead of the template text, with
# the action list extracted from the template's own registerAction() calls.
ACTIONS="$(grep -o 'registerAction("[^"]*"' "$DEST/main.cpp" | sed 's/registerAction("//; s/"//' | paste -sd ' / ' -)"

cat > "$DEST/README.md" <<EOF
# $TITLE

<!-- One paragraph: what is this game, and why is it fun? -->

## Controls
- Arrows / WASD — move
- R — restart

## How to build & run
\`\`\`bash
cmake --build build --target ${NAME}_game
./build/${NAME}_game
\`\`\`

## LLM interface
The game is LLM-playable via \`GameState\`/actions (see
[GAME_DEV_GUIDE.md](../GAME_DEV_GUIDE.md)); the actions registered in
\`main.cpp\` are \`$ACTIONS\`.

## Status
Scaffolded from a Games/$(basename "$TEMPLATE") starter. Replace the
placeholder rules in \`main.cpp\` with the real game. When it is done,
update [GAMES.md](../GAMES.md) and check off this game.
EOF

# Register the game in the canonical smoke list (cmake/smoke_targets.txt)
# so it gets smoke/memcheck/browser CI coverage from day one - the CMake
# configure gate fails without it. Appending is safe: every consumer skips
# '#' comments and reads names line-wise, and
# tools/check_game_registration.sh enforces the bijection both ways.
SMOKE_LIST="$ROOT/cmake/smoke_targets.txt"
TARGET="${NAME}_game"
if [ -f "$SMOKE_LIST" ]; then
    if grep -qxF "$TARGET" "$SMOKE_LIST"; then
        echo "note: $TARGET is already registered in cmake/smoke_targets.txt"
    else
        printf '%s\n' "$TARGET" >> "$SMOKE_LIST"
        echo "registered $TARGET in cmake/smoke_targets.txt"
    fi
else
    echo "warning: cmake/smoke_targets.txt not found - $TARGET has no smoke/memcheck/browser CI coverage" >&2
fi

echo ""
echo "Created $DEST"
echo ""
echo "Next steps:"
echo "  1. cmake -S . -B build            # re-glob registers ${NAME}_game"
echo "  2. cmake --build build --target ${NAME}_game"
echo "  3. ./build/${NAME}_game            # playable already (starter rules)"
echo "  4. Edit $DEST/main.cpp - the pattern comments mark what to change."
echo "  5. Build for the web: tools/build_web_game.sh $NAME"
echo ""
echo "Smoke/memcheck/browser CI coverage is wired automatically via cmake/smoke_targets.txt."
echo "Remember to update GAMES.md when your game is playable."
