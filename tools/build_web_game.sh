#!/usr/bin/env bash
# Export ONE game to the browser (WASM).
#
# Usage:  tools/build_web_game.sh MyGame
#
# Compiles Games/MyGame/main.cpp with Emscripten (SDL2, SDL2_image,
# SDL2_ttf, SDL2_mixer, box2d) and writes a self-contained page to
# web/games/MyGame/index.html - open it in any browser.
#
# Requires the Emscripten SDK: either `emcc` on PATH or
# ~/emsdk/emsdk_env.sh (the repo's build_wasm.sh sources it the same
# way). The flags mirror the Makefile's proven WASM incantation, so a
# game that runs natively builds here unchanged.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="${1:-}"

if [ -z "$NAME" ]; then
    echo "usage: tools/build_web_game.sh <GameName>" >&2
    exit 1
fi

if ! [[ "$NAME" =~ ^[A-Za-z0-9][A-Za-z0-9_-]*$ ]]; then
    echo "error: '$NAME' is not a valid game name." >&2
    exit 1
fi

SRC="$ROOT/Games/$NAME/main.cpp"
if [ ! -f "$SRC" ]; then
    echo "error: $SRC not found. Scaffold it first: tools/new_game.sh $NAME" >&2
    exit 1
fi

# --- Find emcc --------------------------------------------------------------
if ! command -v emcc >/dev/null 2>&1; then
    if [ -f "$HOME/emsdk/emsdk_env.sh" ]; then
        # shellcheck disable=SC1091
        source "$HOME/emsdk/emsdk_env.sh"
    else
        echo "error: emcc not found. Install the Emscripten SDK (https://emscripten.org) or" >&2
        echo "       source your emsdk: source ~/emsdk/emsdk_env.sh" >&2
        exit 1
    fi
fi

# --- Sources (the Makefile's proven set) -------------------------------------
ENGINE_SOURCES=(
    "$ROOT/Engine/Core/SDLApp.cpp"
    "$ROOT/Engine/Core/AssetManager.cpp"
    "$ROOT/Engine/Core/Game2D.cpp"
    "$ROOT/Engine/Core/UI.cpp"
    "$ROOT/Engine/Core/InputManager.cpp"
    "$ROOT/Engine/EntityAndScene/Sprite.cpp"
    "$ROOT/Engine/EntityAndScene/Scene.cpp"
    "$ROOT/Engine/EntityAndScene/Grid.cpp"
)
BOX2D_SRCS=(
    $(find "$ROOT/dependencies/box2d/src" -name "*.c" -o -name "*.cpp" | sort)
)

# --- Flags ------------------------------------------------------------------
EM_FLAGS=(
    -s USE_SDL=2
    -s USE_SDL_IMAGE=2
    -s SDL2_IMAGE_FORMATS=png
    -s USE_SDL_TTF=2
    -s USE_SDL_MIXER=2
    -s WASM=1
    -s ALLOW_MEMORY_GROWTH=1
    -s DISABLE_EXCEPTION_CATCHING=0
    -s ERROR_ON_UNDEFINED_SYMBOLS=0
)
# Default font used by the UI text helpers - preload when present.
if [ -f "$ROOT/web/default.ttf" ]; then
    EM_FLAGS+=(--preload-file "$ROOT/web/default.ttf@/default.ttf")
fi
# Game assets dir - every file is preloaded into the page.
if [ -d "$ROOT/Games/$NAME/assets" ] && [ -n "$(ls -A "$ROOT/Games/$NAME/assets")" ]; then
    EM_FLAGS+=(--preload-file "$ROOT/Games/$NAME/assets@/assets")
fi

OUT="$ROOT/web/games/$NAME"
mkdir -p "$OUT"

echo "=== Building $NAME for the web ==="
emcc -O2 \
    "${EM_FLAGS[@]}" \
    -I"$ROOT" \
    -I"$ROOT/dependencies/box2d/include" \
    -I"$ROOT/dependencies/box2d/src" \
    "$SRC" \
    "${ENGINE_SOURCES[@]}" \
    "${BOX2D_SRCS[@]}" \
    -o "$OUT/index.html"

echo ""
echo "=== Done: $OUT/index.html ==="
echo "Serve it (browsers block local file:// modules):"
echo "  python3 -m http.server -d web"
echo "then open http://localhost:8000/games/$NAME/"
