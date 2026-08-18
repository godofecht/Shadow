#!/bin/bash
# Build ALL examples AND every shipped game (Games/*) for WASM.
# Examples land in web/<name>.html; games land in web/games/<name>/index.html.
# Generator templates (_template, _twin_stick, _platformer) are skipped.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_wasm"
WEB_DIR="$SCRIPT_DIR/web"

echo "=== Umbra Engine WASM Build ==="
echo "Source: $SCRIPT_DIR"
echo "Build: $BUILD_DIR"
echo "Web: $WEB_DIR"

# Clean and create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Source emsdk
source ~/emsdk/emsdk_env.sh 2>/dev/null

# Common Emscripten flags
EM_FLAGS="-s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS=png -s USE_SDL_TTF=2 -s USE_SDL_MIXER=2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=0 -s ERROR_ON_UNDEFINED_SYMBOLS=0 --preload-file $WEB_DIR/default.ttf@/default.ttf --preload-file $WEB_DIR/default.ttf@default.ttf"

echo ""
echo "=== Building core examples for WASM ==="

# Build key examples
EXAMPLES=(
    "bloom:Examples/BloomShowcase/main.cpp"
    "particles:Examples/ParticleShowcase/main.cpp"
    "lighting:Examples/Lighting/main.cpp"
    "text2d:Examples/Text2D/main.cpp"
    "snake:Examples/Snake/main.cpp"
    "minesweeper:Examples/Minesweeper/main.cpp"
    "tictactoe:Examples/TicTacToe/main.cpp"
    "roguelike:Examples/Roguelike/main.cpp"
    "procedural3d:Examples/Procedural3D/main.cpp"
    "terrain:Examples/ProceduralTerrain/main.cpp"
    "skeletal:Examples/SkeletalAnim/main.cpp"
    "parenting:Examples/Parenting/main.cpp"
    "tweening:Examples/Tweening/main.cpp"
    "bunnymark:Examples/Bunnymark/main.cpp"
    "physics_reactive:Examples/PhysicsReactive/main.cpp"
    "navigation:Examples/Navigation/main.cpp"
    "ai_states:Examples/AIStates/main.cpp"
    "compute_sim:Examples/ComputeSim/main.cpp"
    "spatial_audio:Examples/SpatialAudio/main.cpp"
    "fixed_timestep:Examples/FixedTimestep/main.cpp"
    "game_of_life:Examples/GameOfLife/main.cpp"
    "slime_mold:Examples/SlimeMold/main.cpp"
)

# Common sources
ENGINE_SOURCES=(
    "$SCRIPT_DIR/Engine/Core/SDLApp.cpp"
    "$SCRIPT_DIR/Engine/Core/AssetManager.cpp"
    "$SCRIPT_DIR/Engine/Core/InputManager.cpp"
    "$SCRIPT_DIR/Engine/Core/Game2D.cpp"
    "$SCRIPT_DIR/Engine/Core/UI.cpp"
    "$SCRIPT_DIR/Engine/EntityAndScene/Sprite.cpp"
    "$SCRIPT_DIR/Engine/EntityAndScene/Scene.cpp"
    "$SCRIPT_DIR/Engine/EntityAndScene/Grid.cpp"
)

# Box2D sources (all .c files)
BOX2D_SRCS=(
    "$SCRIPT_DIR/dependencies/box2d/src/aabb.c"
    "$SCRIPT_DIR/dependencies/box2d/src/array.c"
    "$SCRIPT_DIR/dependencies/box2d/src/bitset.c"
    "$SCRIPT_DIR/dependencies/box2d/src/body.c"
    "$SCRIPT_DIR/dependencies/box2d/src/broad_phase.c"
    "$SCRIPT_DIR/dependencies/box2d/src/constraint_graph.c"
    "$SCRIPT_DIR/dependencies/box2d/src/contact_solver.c"
    "$SCRIPT_DIR/dependencies/box2d/src/contact.c"
    "$SCRIPT_DIR/dependencies/box2d/src/core.c"
    "$SCRIPT_DIR/dependencies/box2d/src/distance_joint.c"
    "$SCRIPT_DIR/dependencies/box2d/src/distance.c"
    "$SCRIPT_DIR/dependencies/box2d/src/dynamic_tree.c"
    "$SCRIPT_DIR/dependencies/box2d/src/geometry.c"
    "$SCRIPT_DIR/dependencies/box2d/src/hull.c"
    "$SCRIPT_DIR/dependencies/box2d/src/id_pool.c"
    "$SCRIPT_DIR/dependencies/box2d/src/island.c"
    "$SCRIPT_DIR/dependencies/box2d/src/joint.c"
    "$SCRIPT_DIR/dependencies/box2d/src/manifold.c"
    "$SCRIPT_DIR/dependencies/box2d/src/math_functions.c"
    "$SCRIPT_DIR/dependencies/box2d/src/motor_joint.c"
    "$SCRIPT_DIR/dependencies/box2d/src/mouse_joint.c"
    "$SCRIPT_DIR/dependencies/box2d/src/prismatic_joint.c"
    "$SCRIPT_DIR/dependencies/box2d/src/revolute_joint.c"
    "$SCRIPT_DIR/dependencies/box2d/src/shape.c"
    "$SCRIPT_DIR/dependencies/box2d/src/solver_set.c"
    "$SCRIPT_DIR/dependencies/box2d/src/solver.c"
    "$SCRIPT_DIR/dependencies/box2d/src/stack_allocator.c"
    "$SCRIPT_DIR/dependencies/box2d/src/table.c"
    "$SCRIPT_DIR/dependencies/box2d/src/timer.c"
    "$SCRIPT_DIR/dependencies/box2d/src/types.c"
    "$SCRIPT_DIR/dependencies/box2d/src/weld_joint.c"
    "$SCRIPT_DIR/dependencies/box2d/src/wheel_joint.c"
    "$SCRIPT_DIR/dependencies/box2d/src/world.c"
)

echo ""
echo "=== Building core examples for WASM ==="

for example in "${EXAMPLES[@]}"; do
    name="${example%%:*}"
    src="${example##*:}"
    abs_src="$SCRIPT_DIR/$src"

    echo "Building $name..."

    emcc -O2 \
        $EM_FLAGS \
        -I"$SCRIPT_DIR" \
        -I"$SCRIPT_DIR/dependencies/box2d/include" \
        "$abs_src" \
        "${ENGINE_SOURCES[@]}" \
        "${BOX2D_SRCS[@]}" \
        -o "$WEB_DIR/${name}.html" \
        2>&1 | tail -3 || echo "Warning: $name had issues"
done

echo ""
echo "=== Building games for WASM ==="

# Every game in Games/ ships to the browser too. Underscore-prefixed
# directories (_template, _twin_stick, _platformer) are generators, not
# shipped games - skip them.
mkdir -p "$WEB_DIR/games"
for main in "$SCRIPT_DIR"/Games/*/main.cpp; do
    [ -e "$main" ] || continue
    dir="$(dirname "$main")"
    name="$(basename "$dir")"
    case "$name" in
        _*) echo "Skipping generator template: $name"; continue ;;
    esac

    echo "Building game $name..."

    GAME_FLAGS=(
        -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS=png
        -s USE_SDL_TTF=2 -s USE_SDL_MIXER=2 -s WASM=1
        -s ALLOW_MEMORY_GROWTH=1 -s DISABLE_EXCEPTION_CATCHING=0
        -s ERROR_ON_UNDEFINED_SYMBOLS=0
    )
    # Default font used by the UI text helpers - preload when present.
    if [ -f "$WEB_DIR/default.ttf" ]; then
        GAME_FLAGS+=(--preload-file "$WEB_DIR/default.ttf@/default.ttf")
    fi
    # Game assets dir - every file is preloaded into the page.
    if [ -d "$dir/assets" ] && [ -n "$(ls -A "$dir/assets" 2>/dev/null)" ]; then
        GAME_FLAGS+=(--preload-file "$dir/assets@/assets")
    fi

    mkdir -p "$WEB_DIR/games/$name"
    emcc -O2 \
        "${GAME_FLAGS[@]}" \
        -I"$SCRIPT_DIR" \
        -I"$SCRIPT_DIR/dependencies/box2d/include" \
        -I"$SCRIPT_DIR/dependencies/box2d/src" \
        "$main" \
        "${ENGINE_SOURCES[@]}" \
        "${BOX2D_SRCS[@]}" \
        -o "$WEB_DIR/games/$name/index.html" \
        2>&1 | tail -3 || echo "Warning: game $name had issues"
done

echo ""
echo "=== WASM build complete ==="
echo "Output directory: $WEB_DIR"
echo "Examples: $WEB_DIR/*.html"
echo "Games: $WEB_DIR/games/*/index.html"
ls -la "$WEB_DIR"/*.js "$WEB_DIR"/*.wasm 2>/dev/null | head -10 || echo "No files found"
