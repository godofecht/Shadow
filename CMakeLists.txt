cmake_minimum_required(VERSION 3.22)
project(sdl_app VERSION 1.0.0 LANGUAGES C CXX)

# Single source of truth for the engine version. Bump it here and
# reconfigure — Engine/Version.h is regenerated automatically.
# NOTE: also update the version badge in README.md and CHANGELOG.md.
set(UMBRA_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(UMBRA_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(UMBRA_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(UMBRA_VERSION_STRING "${PROJECT_VERSION}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Engine/Version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/Engine/Version.h"
    @ONLY
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Options
option(MYGAME_VENDORED "Use vendored SDL2 libraries (Windows)" OFF)
# AudioEngine uses SDL_mixer; RtAudio is optional legacy support.
option(USE_VENDORED_RTAUDIO "Use vendored RtAudio library" OFF)
option(UMBRA_BUILD_TESTS "Build the unit test suite" ON)
option(UMBRA_BUILD_EXAMPLES "Build the example games" ON)
option(UMBRA_BUILD_GAMES "Build the 100-game program in Games/ (independent of UMBRA_BUILD_EXAMPLES)" ON)
option(UMBRA_WERROR "Treat compiler warnings as errors (GCC/Clang -Werror, MSVC /WX)" ON)
option(UMBRA_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang only)" OFF)
# Opt-in: register the valgrind memcheck passes (Linux) as ctest tests so
# `ctest -R memcheck` drives valgrind too. Default OFF so a plain `ctest`
# run (unit_tests / backend_tests only) stays fast.
option(UMBRA_MEMCHECK_CTEST "Register valgrind memcheck passes as ctest tests (Linux; requires valgrind)" OFF)

# RtAudio backends (Windows only)
option(ENABLE_RTAUDIO_ASIO "Enable ASIO support for RtAudio" ON)
option(ENABLE_RTAUDIO_WASAPI "Enable WASAPI support for RtAudio" ON)
option(ENABLE_RTAUDIO_DS "Enable DirectSound support for RtAudio" ON)

# Single shared source for the headless example/game target list (see the
# header of cmake/smoke_targets.txt). Consumed by the smoke_* ctest entries
# and sanitize_smoke_binaries below, the memcheck-examples target and
# memcheck_example_* ctest entries, and the linux-sanitize / linux-ubsan CI
# jobs' smoke_* ctest -N assertion steps (which grep the same file), so the
# names can never drift between what CMake registers and what CI asserts.
file(STRINGS "${CMAKE_SOURCE_DIR}/cmake/smoke_targets.txt" SMOKE_TARGETS
    REGEX "^[A-Za-z0-9_]+$")

# Vendored dependencies live ONLY under dependencies/ - the single canonical
# vendored location. Root-level copies of these trees were consolidated away
# (sdl and sdl-image were byte-identical duplicates of dependencies/sdl and
# dependencies/sdl-image; SDL_ttf, SDL2_mixer, rtaudio and libgamepad were
# stale gitlinks; box2d was a duplicate of dependencies/box2d). A reappearing
# root tree means a botched merge or accidental re-add, and the duplicate
# would silently drift from the canonical copy - fail the configure instead.
# EXISTS() also fires on an empty dir or un-checked-out gitlink, which is
# exactly the regression mode for the four gitlink deps.
foreach(_shadow_root_vendored_dir IN ITEMS
        sdl sdl-image SDL_ttf SDL2_mixer box2d rtaudio libgamepad)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${_shadow_root_vendored_dir}")
        message(FATAL_ERROR
            "Vendored dependency '${_shadow_root_vendored_dir}/' must live only "
            "under dependencies/ (the single canonical vendored location). "
            "A root-level '${CMAKE_SOURCE_DIR}/${_shadow_root_vendored_dir}/' "
            "was found - delete it or merge it into "
            "dependencies/${_shadow_root_vendored_dir}, then reconfigure.")
    endif()
endforeach()
unset(_shadow_root_vendored_dir)

# Reference scan: no file may point at a root-level vendored tree by path.
# The directory check above catches reappearing trees; this scan catches
# stale references to them - a vendored dir name used as a root-level path
# (an include flag, a source list, a source-dir-anchored include dir, or a
# bare add_subdirectory). Canonical paths under dependencies/ are masked out
# first, so anything left is a root-level reference and fails the configure.
# Comments are scanned too - a documented stale path is just as likely to be
# copy-pasted back in. Scanning the project's own build files makes the check
# self-validating: the guard can never ship a stale reference itself.
set(_guard_checked_files
    "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_SOURCE_DIR}/Makefile"
    "${CMAKE_SOURCE_DIR}/build_wasm.sh")
file(GLOB _guard_script_files
    "${CMAKE_SOURCE_DIR}/tools/*.sh"
    "${CMAKE_SOURCE_DIR}/tools/*.py"
    "${CMAKE_SOURCE_DIR}/cmake/*.cmake")
list(APPEND _guard_checked_files ${_guard_script_files})

set(_guard_vendored_dirs sdl sdl-image SDL_ttf SDL2_mixer box2d rtaudio libgamepad)

unset(_guard_violations)
foreach(_guard_file IN LISTS _guard_checked_files)
    if(NOT EXISTS "${_guard_file}")
        continue()
    endif()
    file(READ "${_guard_file}" _guard_content)
    # Mask canonical paths so only root-level references remain.
    foreach(_guard_dir IN LISTS _guard_vendored_dirs)
        string(REPLACE "dependencies/${_guard_dir}" "" _guard_content "${_guard_content}")
    endforeach()
    # Line-by-line scan for residual root-level path references.
    string(REPLACE ";" "\\;" _guard_content "${_guard_content}")
    string(REPLACE "\n" ";" _guard_lines "${_guard_content}")
    set(_guard_lineno 0)
    foreach(_guard_line IN LISTS _guard_lines)
        math(EXPR _guard_lineno "${_guard_lineno} + 1")
        foreach(_guard_dir IN LISTS _guard_vendored_dirs)
            if(_guard_line MATCHES "${_guard_dir}/"
               OR _guard_line MATCHES "CMAKE_(CURRENT_)?SOURCE_DIR[}]/${_guard_dir}"
               OR _guard_line MATCHES "add_subdirectory\\(${_guard_dir}\\)")
                string(STRIP "${_guard_line}" _guard_trimmed)
                if(NOT _guard_trimmed STREQUAL "")
                    list(APPEND _guard_violations
                        "${_guard_file}:${_guard_lineno}: ${_guard_trimmed}")
                endif()
                break()
            endif()
        endforeach()
    endforeach()
endforeach()
unset(_guard_dir)
unset(_guard_file)
unset(_guard_lineno)
unset(_guard_line)
unset(_guard_trimmed)
unset(_guard_lines)
unset(_guard_content)
unset(_guard_script_files)

if(_guard_violations)
    string(JOIN "\n  " _guard_violation_text ${_guard_violations})
    message(FATAL_ERROR
        "Root-level vendored path reference(s) found - all vendored paths "
        "must live under dependencies/ (the single canonical vendored "
        "location):\n  ${_guard_violation_text}\n"
        "Point the reference(s) at dependencies/, then reconfigure.")
endif()
unset(_guard_violations)
unset(_guard_violation_text)
unset(_guard_checked_files)
unset(_guard_vendored_dirs)

# ============================================
# SDL2 dependency discovery
# ============================================
if(EMSCRIPTEN)
    # Emscripten provides SDL2 ports - no find_package needed
    set(SDL2_INCLUDE_DIRS "")
    set(SDL2_IMAGE_INCLUDE_DIRS "")
    set(SDL2_TTF_INCLUDE_DIRS "")
    set(SDL2_MIXER_INCLUDE_DIRS "")
elseif(MYGAME_VENDORED)
    # Vendored SDL2 (dependencies/ is the single canonical vendored location
    # for all four SDL libraries: sdl, sdl-image, SDL_ttf, SDL2_mixer)
    set(SDL2_IMAGE_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/dependencies/sdl-image/include")
    set(SDL2_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/dependencies/sdl/include")
    set(SDL2_TTF_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/dependencies/SDL_ttf")
    set(SDL2_MIXER_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/dependencies/SDL2_mixer/include")
    set(SDL2_mixer_DIR "${CMAKE_SOURCE_DIR}/dependencies/SDL2_mixer/cmake")
else()
    # System packages (Homebrew / apt / vcpkg) via CMake config
    find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2)
    find_package(SDL2_image CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)
    find_package(SDL2_mixer CONFIG REQUIRED)

    # The config packages export -isystem .../include/SDL2, but engine headers
    # use #include <SDL2/SDL_*.h>, so resolve the parent include dirs too.
    # (Only used if found - avoids injecting -I<VAR>-NOTFOUND on other platforms.)
    find_path(SDL2_INCLUDE_DIRS SDL2/SDL.h)
    find_path(SDL2_IMAGE_INCLUDE_DIRS SDL2/SDL_image.h)
    find_path(SDL2_TTF_INCLUDE_DIRS SDL2/SDL_ttf.h)
    find_path(SDL2_MIXER_INCLUDE_DIRS SDL2/SDL_mixer.h)
endif()

# Normalize -I flags so NOTFOUND paths never leak into compile commands.
if(NOT SDL2_INCLUDE_DIRS)
    set(SDL2_INCLUDE_DIRS "")
endif()
if(NOT SDL2_IMAGE_INCLUDE_DIRS)
    set(SDL2_IMAGE_INCLUDE_DIRS "")
endif()
if(NOT SDL2_TTF_INCLUDE_DIRS)
    set(SDL2_TTF_INCLUDE_DIRS "")
endif()
if(NOT SDL2_MIXER_INCLUDE_DIRS)
    set(SDL2_MIXER_INCLUDE_DIRS "")
endif()

# Box2D (disable samples/tests to keep the build fast and hermetic)
set(BOX2D_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(dependencies/box2d)

# RtAudio (optional - AudioEngine prefers SDL_mixer)
if(EMSCRIPTEN)
    set(RTAUDIO_LIB "")
elseif(USE_VENDORED_RTAUDIO)
    if(ENABLE_RTAUDIO_ASIO)
        set(RTAUDIO_WINDOWS_ASIO ON CACHE BOOL "Enable ASIO support for RtAudio" FORCE)
    endif()
    if(ENABLE_RTAUDIO_WASAPI)
        set(RTAUDIO_WINDOWS_WASAPI ON CACHE BOOL "Enable WASAPI support for RtAudio" FORCE)
    endif()
    if(ENABLE_RTAUDIO_DS)
        set(RTAUDIO_WINDOWS_DS ON CACHE BOOL "Enable DirectSound support for RtAudio" FORCE)
    endif()

    add_subdirectory(dependencies/rtaudio ${CMAKE_BINARY_DIR}/rtaudio_build EXCLUDE_FROM_ALL)
    set(RTAUDIO_LIB rtaudio)
else()
    find_package(RtAudio QUIET)
    if(RtAudio_FOUND)
        set(RTAUDIO_LIB RtAudio::RtAudio)
    else()
        set(RTAUDIO_LIB "")
    endif()
endif()

# ============================================
# Sanitizers (ASan + UBSan) - optional
# ============================================
# UMBRA_SANITIZE=ON instruments all first-party targets (engine, app,
# tests, examples) with the sanitizers listed in UMBRA_SANITIZER_SET so
# memory errors and UB that warnings cannot catch fail the build/test run.
# The flags are added AFTER vendored dependencies (box2d, RtAudio) are
# wired up, so third-party code is never instrumented.
#
# UMBRA_SANITIZER_SET selects the sanitizers:
#   - "address,undefined" (default): ASan + UBSan together (CI linux-sanitize)
#   - "undefined": UBSan alone (CI linux-ubsan) - catches integer-overflow
#     and float-cast-overflow bugs that ASan+UBSan's halt_on_error can mask
#     (e.g. when an earlier ASan-recoverable fault is reported first).
#   - "address": ASan alone.
#
# -fno-sanitize-recover=all makes UBSan abort on the FIRST violation
# (non-zero exit) instead of printing and continuing, so CI catches it.
# signed-integer-overflow and float-cast-overflow are part of the default
# "undefined" group, so no extra checks are needed for those.
#
# Emscripten: emcc supports ASan (-fsanitize=address) and UBSan
# (-fsanitize=undefined); the WASM CI job uses ASan to catch memory errors
# in the browser build path. Native GCC/Clang is the other supported path;
# MSVC has its own tooling (no support here - the option warns and no-ops).
set(UMBRA_SANITIZER_SET "address,undefined" CACHE STRING
    "Sanitizers for UMBRA_SANITIZE: address, undefined, or address,undefined")

if(UMBRA_SANITIZE)
    if(EMSCRIPTEN)
        # emcc: ASan aborts on the first memory error; keep stack frames.
        # (UBSan is available too but ASan is the memory-error workhorse.)
        set(UMBRA_SANITIZE_FLAGS
            -fsanitize=address
            -fno-omit-frame-pointer
        )
        add_compile_options(${UMBRA_SANITIZE_FLAGS})
        add_link_options(${UMBRA_SANITIZE_FLAGS})
        message(STATUS "UMBRA_SANITIZE=ON: building with Emscripten ASan")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(UMBRA_SANITIZE_FLAGS
            -fsanitize=${UMBRA_SANITIZER_SET}
            -fno-omit-frame-pointer
            -fno-sanitize-recover=all
        )
        add_compile_options(${UMBRA_SANITIZE_FLAGS})
        add_link_options(${UMBRA_SANITIZE_FLAGS})
        message(STATUS "UMBRA_SANITIZE=ON: building with ${UMBRA_SANITIZER_SET}")
    else()
        message(WARNING "UMBRA_SANITIZE=ON but sanitizers are only supported for GCC/Clang/Emscripten - ignoring")
    endif()
endif()

# ============================================
# Core engine library (shared by app, examples, tests)
# ============================================
set(ENGINE_SOURCES
    Engine/Core/SDLApp.cpp
    Engine/Core/AssetManager.cpp
    Engine/Core/InputManager.cpp
    Engine/Core/Game2D.cpp
    Engine/Core/UI.cpp
    Engine/EntityAndScene/Sprite.cpp
    Engine/EntityAndScene/Scene.cpp
    Engine/EntityAndScene/Grid.cpp
)

add_library(umbra_engine STATIC ${ENGINE_SOURCES})

target_include_directories(umbra_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/box2d/include"
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)

target_link_libraries(umbra_engine PUBLIC box2d)
if(NOT EMSCRIPTEN)
    target_link_libraries(umbra_engine PUBLIC SDL2::SDL2)
    if(TARGET SDL2_image::SDL2_image)
        target_link_libraries(umbra_engine PUBLIC SDL2_image::SDL2_image)
    elseif(TARGET SDL2::SDL2_image)
        target_link_libraries(umbra_engine PUBLIC SDL2::SDL2_image)
    endif()
    if(TARGET SDL2_ttf::SDL2_ttf)
        target_link_libraries(umbra_engine PUBLIC SDL2_ttf::SDL2_ttf)
    elseif(TARGET SDL2::SDL2_ttf)
        target_link_libraries(umbra_engine PUBLIC SDL2::SDL2_ttf)
    endif()
    if(TARGET SDL2_mixer::SDL2_mixer)
        target_link_libraries(umbra_engine PUBLIC SDL2_mixer::SDL2_mixer)
    elseif(TARGET SDL2::SDL2_mixer)
        target_link_libraries(umbra_engine PUBLIC SDL2::SDL2_mixer)
    endif()
        if(WIN32)
        target_link_libraries(umbra_engine PUBLIC d2d1 dwrite)
    endif()
endif()

# Windows.h min/max macros break <algorithm> - disable them everywhere.
if(WIN32)
    target_compile_definitions(umbra_engine PUBLIC NOMINMAX)
endif()

# Compiler warnings - applied to engine, app, tests, and examples so the
# whole tree builds warning-free. (Vendored third-party code is excluded.)
# UMBRA_WERROR (ON by default) escalates warnings to errors so new warnings
# can never silently creep back in; CI pins it explicitly too. The MSVC /wd
# suppressions are the standard non-actionable /W4 noise set (inlining
# decisions, unreferenced local functions, struct padding) that Chromium/LLVM
# also disable - real-bug warnings like C4244/C4267/C4996/C445x stay enabled
# so they still fail CI. NOTE: if the Windows CI job ever turns red on a
# /W4-only noise warning the clang sweep can't predict (e.g. C4127, C4702,
# C4624), extend this /wd list - that is the documented escape hatch.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(UMBRA_WARN_FLAGS -Wall -Wextra)
    if(UMBRA_WERROR)
        list(APPEND UMBRA_WARN_FLAGS -Werror)
    endif()
elseif(MSVC)
    set(UMBRA_WARN_FLAGS /W4 /wd4514 /wd4710 /wd4711 /wd4820 /wd4505)
    if(UMBRA_WERROR)
        list(APPEND UMBRA_WARN_FLAGS /WX)
    endif()
endif()

# NOTE: target_compile_options() only works on targets that already exist, so
# the flags are applied to each target right after it is created below (an
# early `if(TARGET sdl_app)` guard here would silently skip the app/tests).
target_compile_options(umbra_engine PRIVATE ${UMBRA_WARN_FLAGS})

# ============================================
# Main application
# ============================================
add_executable(sdl_app main.cpp)
target_link_libraries(sdl_app PRIVATE umbra_engine)
target_compile_options(sdl_app PRIVATE ${UMBRA_WARN_FLAGS})
if(EMSCRIPTEN)
    target_link_libraries(sdl_app PRIVATE box2d SDL2 SDL2_image SDL2_ttf SDL2_mixer freetype harfbuzz z)
endif()
if(RTAUDIO_LIB)
    target_link_libraries(sdl_app PRIVATE ${RTAUDIO_LIB})
endif()
if(TARGET SDL2::SDL2main)
    target_link_libraries(sdl_app PRIVATE SDL2::SDL2main)
endif()

# ============================================
# Tests
# ============================================
# Built natively on all OSes AND for Emscripten (where the WASM test
# binaries run headless in Node - see the Emscripten branch below). Tests
# are headless pure-unit tests: they never call SDL_Init or open a window,
# so they are safe to run in CI everywhere, including under sanitizers.
if(UMBRA_BUILD_TESTS)
    enable_testing()

    # Shell-level gate regression: tools/test_check_github_meta.sh is the
    # offline test harness for the check-github gate
    # (tools/check_github_meta.sh). It rebuilds a scratch fixture from the
    # real .github/ metadata and asserts the pristine tree passes while three
    # mutations (missing job, wrong count, set disagreement) each fail the
    # gate, so a regression in the gate itself - or drift in the real files it
    # reads - fails `ctest` too, alongside the linux job's `make test-github`
    # step. No build artifacts or network needed. Skipped on Windows: ctest
    # execs `bash` directly (not through a shell), and the windows job's
    # Git-Bash path handling is not worth the risk for a platform-independent
    # bash meta-test - Linux, macOS, and the Emscripten configs all run it.
    if(NOT WIN32)
        add_test(NAME gate_regression
            COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_check_github_meta.sh")
        # The harness copies a few metadata dirs and runs the gate 4x; a
        # couple of seconds locally, so 120s is a very generous guard.
        set_tests_properties(gate_regression PROPERTIES
            LABELS "meta"
            TIMEOUT 120)
    endif()

    # Main test runner + unit tests (one main() in test_main.cpp)
    add_executable(sdl_app_tests
        tests/test_main.cpp
        tests/test_geometry.cpp
        tests/test_matrix.cpp
        tests/test_helpers.cpp
        tests/test_perlin_noise.cpp
        tests/test_grid.cpp
        tests/test_gamestate.cpp
        tests/test_game2d.cpp
        tests/test_assetmanager.cpp
        tests/test_particles.cpp
        tests/test_input.cpp
        tests/test_ui.cpp
        tests/test_scene.cpp
        tests/test_audio.cpp
        tests/test_physics.cpp
        tests/test_roguelike_state.cpp
        tests/test_cinderfall.cpp
        tests/test_games.cpp
    )
    target_link_libraries(sdl_app_tests PRIVATE umbra_engine)
    target_include_directories(sdl_app_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(sdl_app_tests PRIVATE ${UMBRA_WARN_FLAGS})

    # Backend tests (has its own main())
    add_executable(sdl_app_backend_tests tests/test_backend.cpp)
    target_link_libraries(sdl_app_backend_tests PRIVATE umbra_engine)
    target_compile_options(sdl_app_backend_tests PRIVATE ${UMBRA_WARN_FLAGS})
    target_include_directories(sdl_app_backend_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    if(EMSCRIPTEN)
        # WASM tests run headless in Node (single-threaded: no pthreads - the
        # std::thread concurrency tests in test_assetmanager are guarded out
        # for __EMSCRIPTEN__ and stay covered by the native sanitizer jobs,
        # which run them with real threads). sdl_app_backend_tests reads
        # Examples/*/main.cpp from the preloaded virtual FS (Examples/ is
        # mounted at /Examples, so SHADOW_SOURCE_DIR="/").
        # The test framework is built on C++ exceptions (every test runs
        # inside try/catch and some tests throw deliberately, e.g. the
        # AssetManager null-renderer test). Emscripten compiles with throwing
        # supported but exception CATCHING disabled by default, so a throw
        # aborts. -fexceptions on every linked target adds the unwind info,
        # and at link time it re-enables catching.
        target_compile_options(umbra_engine PRIVATE -fexceptions)
        target_compile_options(box2d PRIVATE -fexceptions)
        target_compile_options(sdl_app_tests PRIVATE -fexceptions)
        target_compile_options(sdl_app_backend_tests PRIVATE -fexceptions)
        target_compile_definitions(sdl_app_tests PRIVATE SHADOW_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_definitions(sdl_app_backend_tests PRIVATE SHADOW_SOURCE_DIR="/")
        target_link_libraries(sdl_app_tests PRIVATE box2d SDL2 SDL2_image SDL2_ttf SDL2_mixer freetype harfbuzz z)
        target_link_libraries(sdl_app_backend_tests PRIVATE box2d SDL2 SDL2_image SDL2_ttf SDL2_mixer freetype harfbuzz z)
        set(EMSCRIPTEN_TEST_LINK_FLAGS
            "-fexceptions -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS=png -s USE_SDL_TTF=2 -s USE_SDL_MIXER=2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=268435456 -s MAXIMUM_MEMORY=536870912 --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/Examples@/Examples")
        set_target_properties(sdl_app_tests PROPERTIES
            LINK_FLAGS "${EMSCRIPTEN_TEST_LINK_FLAGS}"
            SUFFIX ".js"
        )
        set_target_properties(sdl_app_backend_tests PROPERTIES
            LINK_FLAGS "${EMSCRIPTEN_TEST_LINK_FLAGS}"
            SUFFIX ".js"
        )
        # Explicit working directory: Node resolves the preloaded .data files
        # relative to cwd, so tests must run from the build dir.
        add_test(NAME unit_tests COMMAND node "${CMAKE_CURRENT_BINARY_DIR}/sdl_app_tests.js")
        set_tests_properties(unit_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
        add_test(NAME backend_tests COMMAND node "${CMAKE_CURRENT_BINARY_DIR}/sdl_app_backend_tests.js")
        set_tests_properties(backend_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

        # Focused named entries mirroring the native sanitize_path_* set:
        # the same four regression tests are compiled into the WASM suite
        # (test_games.cpp is one of sdl_app_tests' sources), and these
        # entries pin them by name so the emscripten-sanitize job can assert
        # (ctest -N) and run them individually under WASM ASan. The runner's
        # argv[1] filter runs exactly one test; a non-matching filter exits
        # 1, so a test silently dropped from the WASM suite is a loud
        # failure instead of hiding inside the full unit_tests run.
        if(UMBRA_SANITIZE)
            foreach(san_path IN ITEMS
                    "gamejuice_floating_text:test_gamejuice_floating_text_lifecycle"
                    "invaders_bombs:test_invaders_bomb_hits_player"
                    "pacman_ghost_target:test_pacman_ghost_target_stopped"
                    "input_mouse_bounds:test_InputManager2D_mouse_button_bounds")
                string(REPLACE ":" ";" _san_parts "${san_path}")
                list(GET _san_parts 0 _san_slug)
                list(GET _san_parts 1 _san_filter)
                add_test(NAME "sanitize_path_${_san_slug}"
                    COMMAND node "${CMAKE_CURRENT_BINARY_DIR}/sdl_app_tests.js" "${_san_filter}")
                set_tests_properties("sanitize_path_${_san_slug}" PROPERTIES
                    LABELS "sanitize"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
                    TIMEOUT 300)
            endforeach()
        endif()
    else()
        target_compile_definitions(sdl_app_tests PRIVATE SHADOW_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        target_compile_definitions(sdl_app_backend_tests PRIVATE SHADOW_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME unit_tests COMMAND sdl_app_tests)
        add_test(NAME backend_tests COMMAND sdl_app_backend_tests)

        # Focused named entries pinning the four code paths whose bugs the
        # sanitizers found and fixed (GameJuice floating-text alpha, Space
        # Invaders bomb-hit clear, Pac-Man no-heading ghost targeting,
        # InputManager mouse-button bounds).
        # Mirroring the memcheck_path_* valgrind entries: each runs ONLY its
        # test via the runner's argv[1] filter, so a regression in one path
        # fails BY NAME instead of being buried in the full unit_tests suite.
        # Registered only when UMBRA_SANITIZE=ON so the sanitizer CI jobs'
        # `ctest --output-on-failure` pick them up automatically while a
        # plain `ctest` run stays at the two full-suite tests.
        if(UMBRA_SANITIZE)
            foreach(san_path IN ITEMS
                    "gamejuice_floating_text:test_gamejuice_floating_text_lifecycle"
                    "invaders_bombs:test_invaders_bomb_hits_player"
                    "pacman_ghost_target:test_pacman_ghost_target_stopped"
                    "input_mouse_bounds:test_InputManager2D_mouse_button_bounds")
                string(REPLACE ":" ";" _san_parts "${san_path}")
                list(GET _san_parts 0 _san_slug)
                list(GET _san_parts 1 _san_filter)
                add_test(NAME "sanitize_path_${_san_slug}"
                    COMMAND sdl_app_tests "${_san_filter}")
                # A single test runs in milliseconds, so this is a generous
                # guard (the full unit_tests suite has no explicit timeout).
                set_tests_properties("sanitize_path_${_san_slug}" PROPERTIES
                    LABELS "sanitize"
                    TIMEOUT 300)
            endforeach()

            # Headless smoke entries: one ctest test per example/game binary
            # that runs without a display, mirroring the linux-sanitize CI
            # job's smoke step and the memcheck_example_* pattern. Each runs
            # its binary for 5s under SDL dummy drivers + PONG_SMOKE=1
            # autoplay via cmake/smoke.cmake, which encodes the acceptance
            # rule (exit 124 = full window clean; immediate exit 0 = start()
            # failure = fail) so CI and developers share one implementation
            # instead of a copy-pasted shell loop. Gated on
            # UMBRA_BUILD_EXAMPLES so the linux-ubsan job (examples OFF)
            # never registers binaries it does not build. The
            # sanitize_smoke_binaries custom target lets the CI build step
            # depend on the whole set without listing each name.
            if(UMBRA_BUILD_EXAMPLES)
                # SMOKE_TARGETS is read from cmake/smoke_targets.txt at the
                # top of this file (single source of truth, shared with the
                # memcheck-examples pass and the CI ctest -N assertions).
                add_custom_target(sanitize_smoke_binaries DEPENDS ${SMOKE_TARGETS})
                foreach(smoke_t IN LISTS SMOKE_TARGETS)
                    add_test(NAME "smoke_${smoke_t}"
                        COMMAND ${CMAKE_COMMAND}
                            "-DSMOKE_BUILD_DIR=${CMAKE_BINARY_DIR}"
                            "-DSMOKE_TARGET=${smoke_t}"
                            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/smoke.cmake")
                    # Each runs one 5s window; 120s is a generous guard.
                    set_tests_properties("smoke_${smoke_t}" PROPERTIES
                        LABELS "smoke"
                        TIMEOUT 120)
                endforeach()
            endif()
        endif()
    endif()
endif()

# ============================================
# Windows: copy runtime DLLs next to the executable
# ============================================
# Only copies DLLs that actually exist in the tree (e.g. a local vendored
# SDL2 build under dependencies/, the single canonical vendored location).
# When using system packages (vcpkg/Chocolatey, which CI does), the DLLs
# live next to the packages and are copied by the linker instead - so every
# copy step is guarded with EXISTS to never fail a build.
if(WIN32 AND NOT MYGAME_VENDORED)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(SDL2_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl/build/Debug/SDL2d.dll")
        set(SDL2_IMAGE_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl-image/build/Debug/SDL2_imaged.dll")
        set(SDL2_TTF_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/SDL_ttf/build/Debug/SDL2_ttfd.dll")
        set(SDL2_MIXER_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/SDL2_mixer/cmake/Debug/SDL2_mixer.dll")
    else()
        set(SDL2_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl/build/Release/SDL2.dll")
        set(SDL2_IMAGE_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/sdl-image/build/Release/SDL2_image.dll")
        set(SDL2_TTF_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/SDL_ttf/build/Release/SDL2_ttf.dll")
        set(SDL2_MIXER_DLL_PATH "${CMAKE_SOURCE_DIR}/dependencies/SDL2_mixer/cmake/Release/SDL2_mixer.dll")
    endif()

    set(OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}")

    foreach(dll IN ITEMS SDL2_DLL_PATH SDL2_IMAGE_DLL_PATH SDL2_TTF_DLL_PATH SDL2_MIXER_DLL_PATH)
        if(EXISTS "${${dll}}")
            add_custom_command(TARGET sdl_app POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${${dll}}" "${OUTPUT_DIRECTORY}/"
            )
        endif()
    endforeach()
endif()

# ============================================
# Example games and 2D rendering examples
# ============================================
if(UMBRA_BUILD_EXAMPLES)
    # Every registered example target, collected by add_umbra_example() so
    # the registration gate below can verify the smoke list against it
    # (mirror of the games gate).
    set(UMBRA_EXAMPLE_TARGETS "")
    function(add_umbra_example TARGET_NAME SOURCE_PATH)
        add_executable(${TARGET_NAME} ${SOURCE_PATH})
        target_link_libraries(${TARGET_NAME} PRIVATE umbra_engine)
        # Every example is demo code, but it must still compile warning-free.
        # UMBRA_WARN_FLAGS is defined before the examples section, so applying
        # it here keeps new examples covered with zero list maintenance.
        target_compile_options(${TARGET_NAME} PRIVATE ${UMBRA_WARN_FLAGS})
        if(EMSCRIPTEN)
            target_link_libraries(${TARGET_NAME} PRIVATE
                box2d
                SDL2
                SDL2_image
                SDL2_ttf
                SDL2_mixer
                freetype
                harfbuzz
                z
            )
        endif()
        list(APPEND UMBRA_EXAMPLE_TARGETS "${TARGET_NAME}")
        set(UMBRA_EXAMPLE_TARGETS "${UMBRA_EXAMPLE_TARGETS}" PARENT_SCOPE)
    endfunction()

    # Classic games
    add_umbra_example(snake_example Examples/Snake/main.cpp)
    add_umbra_example(minesweeper_example Examples/Minesweeper/main.cpp)
    add_umbra_example(tictactoe_example Examples/TicTacToe/main.cpp)
    add_umbra_example(roguelike_example Examples/Roguelike/main.cpp)

    # Procedural examples
    add_umbra_example(procedural3d_example Examples/Procedural3D/main.cpp)
    add_umbra_example(procedural_terrain_example Examples/ProceduralTerrain/main.cpp)
    add_umbra_example(perlin_noise_example Examples/PerlinNoise/main.cpp)

    # Visual & Effects
    add_umbra_example(particles_example Examples/ParticleShowcase/main.cpp)
    add_umbra_example(bloom_example Examples/BloomShowcase/main.cpp)
    add_umbra_example(lighting_example Examples/Lighting/main.cpp)
    add_umbra_example(skeletal_example Examples/SkeletalAnim/main.cpp)
    add_umbra_example(parenting_example Examples/Parenting/main.cpp)
    add_umbra_example(tweening_example Examples/Tweening/main.cpp)
    add_umbra_example(bunnymark_example Examples/Bunnymark/main.cpp)
    add_umbra_example(physics_reactive_example Examples/PhysicsReactive/main.cpp)
    add_umbra_example(navigation_example Examples/Navigation/main.cpp)
    add_umbra_example(ai_states_example Examples/AIStates/main.cpp)
    add_umbra_example(compute_sim_example Examples/ComputeSim/main.cpp)
    add_umbra_example(spatial_audio_example Examples/SpatialAudio/main.cpp)
    add_umbra_example(fixed_timestep_example Examples/FixedTimestep/main.cpp)
    add_umbra_example(game_of_life_example Examples/GameOfLife/main.cpp)
    add_umbra_example(slime_mold_example Examples/SlimeMold/main.cpp)

    # 2D Rendering Basics
    add_umbra_example(shapes2d_example Examples/2DShapes/main.cpp)
    add_umbra_example(sprite_example Examples/Sprite/main.cpp)
    add_umbra_example(mesh2d_example Examples/Mesh2D/main.cpp)
    add_umbra_example(text2d_example Examples/Text2D/main.cpp)

    # Sprite Techniques
    add_umbra_example(sprite_animation_example Examples/SpriteAnimation/main.cpp)
    add_umbra_example(sprite_sheet_example Examples/SpriteSheet/main.cpp)
    add_umbra_example(sprite_flipping_example Examples/SpriteFlipping/main.cpp)
    add_umbra_example(sprite_scale_example Examples/SpriteScale/main.cpp)
    add_umbra_example(sprite_slice_example Examples/SpriteSlice/main.cpp)
    add_umbra_example(sprite_tile_example Examples/SpriteTile/main.cpp)
    add_umbra_example(move_sprite_example Examples/MoveSprite/main.cpp)

    # Mesh & Texture
    add_umbra_example(mesh2d_vertex_colors_example Examples/Mesh2DVertexColors/main.cpp)
    add_umbra_example(mesh2d_alpha_example Examples/Mesh2dAlphaMode/main.cpp)
    add_umbra_example(mesh2d_repeated_texture_example Examples/Mesh2dRepeatedTexture/main.cpp)
    add_umbra_example(arc2d_example Examples/Arc2DMeshes/main.cpp)
    add_umbra_example(manual_mesh2d_example Examples/ManualMesh2D/main.cpp)
    add_umbra_example(cpudrawing_example Examples/CPUDrawing/main.cpp)
    add_umbra_example(pixelgrid_example Examples/PixelGridSnapping/main.cpp)

    # Rotation & Transforms
    add_umbra_example(rotation_cursor_example Examples/RotationToCursor/main.cpp)
    add_umbra_example(generic2d_rotation_example Examples/Generic2DRotation/main.cpp)
    add_umbra_example(viewport2world_example Examples/2DViewportToWorld/main.cpp)

    # GPU & Effects
    add_umbra_example(gpu_drawing_example Examples/GPUDrawing/main.cpp)
    add_umbra_example(bloom2d_example Examples/2DBloom/main.cpp)
    add_umbra_example(transparency2d_example Examples/Transparency2D/main.cpp)
    add_umbra_example(tilemap_chunk_example Examples/TilemapChunk/main.cpp)

    # Multi-window (native only)
    add_umbra_example(multiwindow_text_example Examples/MultiWindowText/main.cpp)

    # Tank arena (assets: tankbody.png / tankhead.png / player.png)
    add_umbra_example(tank_and_bullet_example Examples/TankAndBullet/main.cpp)

    # LLM interface demo (console-only - no window): run
    # <build>/llm_test_example --demo to exercise the GameState/
    # ActionResult interface headlessly. Documented in LLM_INTERFACE.md.
    add_umbra_example(llm_test_example Examples/llm_test.cpp)

    # Every *_example in cmake/smoke_targets.txt must be a registered example
    # target (mirror of the games gate's reverse direction): a stale smoke
    # entry would otherwise fail only later, at build time, in a subset of
    # jobs. The forward direction (every shipped Examples/ dir registered) is
    # enforced by tools/check_example_registration.sh, which is git-aware so
    # untracked scratch dirs are not mistaken for shipped examples.
    foreach(smoke IN LISTS SMOKE_TARGETS)
        if(smoke MATCHES "_example$")
            list(FIND UMBRA_EXAMPLE_TARGETS "${smoke}" _ex_idx)
            if(_ex_idx EQUAL -1)
                message(FATAL_ERROR
                    "cmake/smoke_targets.txt lists '${smoke}' but no "
                    "add_umbra_example() registers it - stale entry or "
                    "renamed example directory.")
            endif()
        endif()
    endforeach()
endif()

# ============================================
# Games/ - the 100-game program (independent of UMBRA_BUILD_EXAMPLES)
# ============================================
# Every subdirectory of Games/ with a main.cpp becomes a `<dir>_game`
# target automatically - creating a new game needs ZERO CMake edits:
#
#   tools/new_game.sh MyGame        # scaffolds Games/MyGame/
#   cmake -S . -B build-games       # picks it up (CONFIGURE_DEPENDS
#                                   # re-globs on every build)
#   cmake --build build-games --target MyGame_game
#
# Directories starting with '_' are skipped so internal folders never
# become targets - EXCEPT _template, which is registered as
# `template_game` so the starter game is always built and CI-tested
# (a broken template would break every future `make new-game`).
function(add_umbra_game TARGET_NAME SOURCE_PATH)
    add_executable(${TARGET_NAME} ${SOURCE_PATH})
    target_link_libraries(${TARGET_NAME} PRIVATE umbra_engine)
    target_compile_options(${TARGET_NAME} PRIVATE ${UMBRA_WARN_FLAGS})
    if(EMSCRIPTEN)
        target_link_libraries(${TARGET_NAME} PRIVATE
            box2d
            SDL2
            SDL2_image
            SDL2_ttf
            SDL2_mixer
            freetype
            harfbuzz
            z
        )
    endif()
endfunction()

if(UMBRA_BUILD_GAMES)
    file(GLOB GAME_MAINS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Games/*/main.cpp")
    set(UMBRA_GAME_TARGETS "")
    foreach(GAME_MAIN IN LISTS GAME_MAINS)
        get_filename_component(GAME_DIR "${GAME_MAIN}" DIRECTORY)
        get_filename_component(GAME_NAME "${GAME_DIR}" NAME)
        if(GAME_NAME MATCHES "^_" AND NOT GAME_NAME STREQUAL "_template")
            continue()
        endif()
        if(GAME_NAME STREQUAL "_template")
            set(GAME_NAME "template")
        endif()
        add_umbra_game("${GAME_NAME}_game" "${GAME_MAIN}")
        list(APPEND UMBRA_GAME_TARGETS "${GAME_NAME}_game")
        message(STATUS "Registered game: ${GAME_NAME}_game")
    endforeach()

    # Every registered game must appear in cmake/smoke_targets.txt (the
    # canonical smoke list), so a new game is forced into smoke / memcheck /
    # browser CI coverage on day one. This is a configure-time error: it
    # fires for the developer locally AND fails every CI job's configure
    # step (games build by default). The reverse direction catches stale
    # entries (renamed or deleted games) that would otherwise only fail
    # later, at build time, in a subset of jobs.
    foreach(game IN LISTS UMBRA_GAME_TARGETS)
        list(FIND SMOKE_TARGETS "${game}" _smoke_idx)
        if(_smoke_idx EQUAL -1)
            message(FATAL_ERROR
                "Game '${game}' is not in cmake/smoke_targets.txt - add it "
                "so the new game gets smoke/memcheck/browser CI coverage "
                "(see the file header).")
        endif()
    endforeach()
    foreach(smoke IN LISTS SMOKE_TARGETS)
        if(smoke MATCHES "_game$")
            list(FIND UMBRA_GAME_TARGETS "${smoke}" _game_idx)
            if(_game_idx EQUAL -1)
                message(FATAL_ERROR
                    "cmake/smoke_targets.txt lists '${smoke}' but no "
                    "Games/*/main.cpp registers it - stale entry or renamed "
                    "game directory.")
            endif()
        endif()
    endforeach()
endif()

# ============================================
# Emscripten WASM build support
# ============================================
if(EMSCRIPTEN)
    set(EMSCRIPTEN_LINK_FLAGS "-s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS=png -s USE_SDL_TTF=2 -s USE_SDL_MIXER=2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=268435456 -s MAXIMUM_MEMORY=536870912 --preload-file ${CMAKE_SOURCE_DIR}/web/default.ttf@/default.ttf")

    if(TARGET sdl_app)
        set_target_properties(sdl_app PROPERTIES
            LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}"
            SUFFIX ".js"
        )
    endif()

    foreach(example IN ITEMS
        snake_example minesweeper_example tictactoe_example roguelike_example
        procedural3d_example procedural_terrain_example particles_example
        bloom_example lighting_example skeletal_example parenting_example
        tweening_example bunnymark_example physics_reactive_example
        navigation_example ai_states_example compute_sim_example
        spatial_audio_example fixed_timestep_example game_of_life_example
        slime_mold_example shapes2d_example sprite_example mesh2d_example
        text2d_example sprite_animation_example sprite_sheet_example
        sprite_flipping_example sprite_scale_example sprite_slice_example
        sprite_tile_example move_sprite_example mesh2d_vertex_colors_example
        mesh2d_alpha_example mesh2d_repeated_texture_example arc2d_example
        manual_mesh2d_example cpudrawing_example pixelgrid_example
        rotation_cursor_example generic2d_rotation_example viewport2world_example
        gpu_drawing_example bloom2d_example transparency2d_example
        tilemap_chunk_example perlin_noise_example tank_and_bullet_example
        llm_test_example)
        if(TARGET ${example})
            set_target_properties(${example} PROPERTIES
                LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}"
                SUFFIX ".js"
            )
        endif()
    endforeach()

    # The 100-game program's shipped games (template_game, Pong_game, and any
    # future Games/*/main.cpp) use the same WASM link flags and .js suffix as
    # the examples, so `make wasm` / build_wasm.sh and the emscripten CI job
    # build them for the browser too.
    foreach(game IN LISTS UMBRA_GAME_TARGETS)
        if(TARGET ${game})
            set_target_properties(${game} PROPERTIES
                LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}"
                SUFFIX ".js"
            )
        endif()
    endforeach()
endif()

# ============================================
# Valgrind memcheck targets (Linux developer tooling)
# ============================================
# `make memcheck` (Makefiles) / `ninja memcheck` (Ninja) runs the exact CI
# linux-valgrind pass locally: the unit test binaries under valgrind
# memcheck with the full leak policy (definite + indirect + possible are
# fatal; valgrind.supp narrows the "possible" class to known SDL2/box2d
# noise). `make memcheck-examples` mirrors the linux-valgrind-examples job
# (headless examples under memcheck with SDL dummy drivers). Both are
# driven by cmake/memcheck.cmake, which locates valgrind, validates
# valgrind.supp, and applies the CI flag sets exactly.
#
# Valgrind is Linux-only (WSL counts), so the targets only exist there.
# They build their binaries first (DEPENDS), so no separate build step is
# needed. Each target is only created for what was configured:
# memcheck needs UMBRA_BUILD_TESTS (default ON), memcheck-examples needs
# UMBRA_BUILD_EXAMPLES (default ON).
#
# With UMBRA_MEMCHECK_CTEST=ON the same passes are ALSO registered as ctest
# tests (memcheck_unit_* / memcheck_example_*), so `ctest -R memcheck
# --output-on-failure` drives valgrind too - CI and developers run the
# exact same path. Opt-in (default OFF) so a plain `ctest` run stays fast.
# Note ctest does not build: build the binaries first (the custom targets
# do that for you). enable_testing() is idempotent; it is called here too
# so add_test() works even when UMBRA_BUILD_TESTS is OFF (examples-only
# configs).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MEMCHECK_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/memcheck.cmake")

    if(UMBRA_MEMCHECK_CTEST)
        enable_testing()
    endif()

    if(UMBRA_BUILD_TESTS)
        add_custom_target(memcheck
            COMMAND ${CMAKE_COMMAND}
                "-DMEMCHECK_BUILD_DIR=${CMAKE_BINARY_DIR}"
                "-DMEMCHECK_SUPPRESSIONS=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp"
                -P "${MEMCHECK_SCRIPT}"
            DEPENDS sdl_app_tests sdl_app_backend_tests
            COMMENT "Running valgrind memcheck over the unit tests (CI linux-valgrind flags)"
            USES_TERMINAL
        )

        # One ctest test per unit binary so failures name the exact binary.
        if(UMBRA_MEMCHECK_CTEST)
            foreach(mc_target IN ITEMS sdl_app_tests sdl_app_backend_tests)
                add_test(NAME "memcheck_unit_${mc_target}"
                    COMMAND ${CMAKE_COMMAND}
                        "-DMEMCHECK_BUILD_DIR=${CMAKE_BINARY_DIR}"
                        "-DMEMCHECK_SUPPRESSIONS=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp"
                        "-DMEMCHECK_TARGETS=${mc_target}"
                        -P "${MEMCHECK_SCRIPT}"
                )
                # 600s per unit binary preserves the CI linux-valgrind job's
                # documented `timeout 600` hang guard exactly (the script's
                # own execute_process TIMEOUT of 3600s is the generous backstop
                # for the `make memcheck` custom target).
                set_tests_properties("memcheck_unit_${mc_target}" PROPERTIES
                    TIMEOUT 600
                    LABELS "memcheck"
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                )
            endforeach()

            # Focused entries that explicitly cover the four code paths whose
            # bugs the sanitizers found and fixed (GameJuice floating-text
            # alpha, Space Invaders bomb-hit clear, Pac-Man no-heading ghost
            # targeting, InputManager mouse-button bounds). Each runs ONLY
            # its test under memcheck via
            # MEMCHECK_TEST_FILTER, so the valgrind job names the exact path
            # instead of relying on the full suite or autopilot smoke to hit
            # it by chance.
            foreach(mc_path IN ITEMS
                    "gamejuice_floating_text:test_gamejuice_floating_text_lifecycle"
                    "invaders_bombs:test_invaders_bomb_hits_player"
                    "pacman_ghost_target:test_pacman_ghost_target_stopped"
                    "input_mouse_bounds:test_InputManager2D_mouse_button_bounds")
                string(REPLACE ":" ";" _parts "${mc_path}")
                list(GET _parts 0 _slug)
                list(GET _parts 1 _filter)
                add_test(NAME "memcheck_path_${_slug}"
                    COMMAND ${CMAKE_COMMAND}
                        "-DMEMCHECK_BUILD_DIR=${CMAKE_BINARY_DIR}"
                        "-DMEMCHECK_SUPPRESSIONS=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp"
                        "-DMEMCHECK_TARGETS=sdl_app_tests"
                        "-DMEMCHECK_TEST_FILTER=${_filter}"
                        -P "${MEMCHECK_SCRIPT}"
                )
                # 600s per path preserves the unit-binary timeout; each entry
                # runs a single test, so this is a very generous guard.
                set_tests_properties("memcheck_path_${_slug}" PROPERTIES
                    TIMEOUT 600
                    LABELS "memcheck"
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                )
            endforeach()
        endif()
    endif()

    if(UMBRA_BUILD_EXAMPLES)
        add_custom_target(memcheck-examples
            COMMAND ${CMAKE_COMMAND}
                "-DMEMCHECK_BUILD_DIR=${CMAKE_BINARY_DIR}"
                "-DMEMCHECK_SUPPRESSIONS=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp"
                "-DMEMCHECK_MODE=examples"
                -P "${MEMCHECK_SCRIPT}"
            # The 100-game program's shipped games ride the same memcheck
            # pass: they run with PONG_SMOKE=1 autoplay (see memcheck.cmake)
            # so the valgrind window exercises their physics too.
            # Same single shared list as the smoke pass: cmake/smoke_targets.txt.
            DEPENDS ${SMOKE_TARGETS}
            COMMENT "Running valgrind memcheck over the headless examples + games (CI linux-valgrind-examples flags)"
            USES_TERMINAL
        )

        # One ctest test per example/game binary (each runs its own 10s window).
        if(UMBRA_MEMCHECK_CTEST)
            foreach(mc_example IN LISTS SMOKE_TARGETS)
                add_test(NAME "memcheck_example_${mc_example}"
                    COMMAND ${CMAKE_COMMAND}
                        "-DMEMCHECK_BUILD_DIR=${CMAKE_BINARY_DIR}"
                        "-DMEMCHECK_SUPPRESSIONS=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp"
                        "-DMEMCHECK_MODE=examples"
                        "-DMEMCHECK_EXAMPLE_TARGETS=${mc_example}"
                        -P "${MEMCHECK_SCRIPT}"
                )
                # 300s per example: each runs one 10s window plus valgrind
                # startup overhead, so this is a very generous guard.
                set_tests_properties("memcheck_example_${mc_example}" PROPERTIES
                    TIMEOUT 300
                    LABELS "memcheck"
                    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
                )
            endforeach()
        endif()
    endif()

    # Refinement loop for valgrind.supp: run the unit tests with
    # --gen-suppressions=all and append any NEW possible-only leak blocks to
    # valgrind.supp (automates the manual command in that file's header and
    # the linux-valgrind CI job's first-run refinement). cmake/memcheck-
    # suppressions.cmake enforces the trial policy: definite/indirect leaks
    # and non-leak error blocks are never auto-suppressed, blocks are
    # deduped against existing entries, and the updated file is validated
    # with valgrind's own parse before replacing the original.
    if(UMBRA_BUILD_TESTS)
        add_custom_target(memcheck-suppressions
            COMMAND ${CMAKE_COMMAND}
                "-DMEMCHECK_BUILD_DIR=${CMAKE_BINARY_DIR}"
                "-DMEMCHECK_SUPPRESSIONS=${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/memcheck-suppressions.cmake"
            DEPENDS sdl_app_tests sdl_app_backend_tests
            COMMENT "Harvesting new valgrind.supp suppression blocks from the unit tests"
            USES_TERMINAL
        )
    endif()
elseif(UMBRA_MEMCHECK_CTEST)
    # Fail loudly instead of silently registering nothing: the option only
    # takes effect on Linux (WSL counts), where valgrind exists. (On Linux
    # the targets and ctest tests above are registered; here we only warn.)
    message(WARNING
        "UMBRA_MEMCHECK_CTEST=ON but the memcheck ctest tests are Linux-only "
        "(valgrind does not run on ${CMAKE_SYSTEM_NAME}) - nothing registered")
endif()

# ============================================
# Callgrind profiling targets (Linux developer tooling)
# ============================================
# `make callgrind` (Makefiles) / `ninja callgrind` (Ninja) profiles a binary
# under valgrind's callgrind tool and emits a hotspot report - pure local
# tooling, deliberately NOT wired into CI (profiling noise, and the run is
# bounded by a SIGTERM kill, not a pass/fail signal). `callgrind` profiles
# bunnymark_example (a CPU hotspot demo - hundreds of sprites per frame),
# `callgrind-app` profiles sdl_app itself.
#
# Driven by cmake/callgrind.cmake, which runs the binary under callgrind
# for CALLGRIND_SECONDS (default 10), kills it with SIGTERM via GNU timeout
# (callgrind flushes its profile on SIGTERM; SIGKILL via --kill-after would
# lose it, so that flag is deliberately NOT used), then runs
# callgrind_annotate. Outputs: <build>/callgrind.out.<target> (raw
# callgraph data, kcachegrind format) and
# <build>/callgrind.annotate.<target>.txt (hotspot report).
#
# Valgrind is Linux-only (WSL counts), so the targets only exist there.
# Each builds its binary first (DEPENDS) and runs from the source root so
# cwd-relative assets (fly.png) resolve.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CALLGRIND_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/callgrind.cmake")
    # Tune the profile window at configure time (default 10s), e.g.
    # cmake -DCALLGRIND_SECONDS=30. Passed through to the script.
    set(CALLGRIND_SECONDS 10 CACHE STRING "Callgrind profile window in seconds")

    if(UMBRA_BUILD_EXAMPLES AND TARGET bunnymark_example)
        add_custom_target(callgrind
            COMMAND ${CMAKE_COMMAND}
                "-DCALLGRIND_BUILD_DIR=${CMAKE_BINARY_DIR}"
                "-DCALLGRIND_TARGET=bunnymark_example"
                "-DCALLGRIND_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DCALLGRIND_SECONDS=${CALLGRIND_SECONDS}"
                -P "${CALLGRIND_SCRIPT}"
            DEPENDS bunnymark_example
            COMMENT "Profiling bunnymark_example under callgrind (${CALLGRIND_SECONDS}s)"
            USES_TERMINAL
        )
    endif()

    if(TARGET sdl_app)
        add_custom_target(callgrind-app
            COMMAND ${CMAKE_COMMAND}
                "-DCALLGRIND_BUILD_DIR=${CMAKE_BINARY_DIR}"
                "-DCALLGRIND_TARGET=sdl_app"
                "-DCALLGRIND_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DCALLGRIND_SECONDS=${CALLGRIND_SECONDS}"
                -P "${CALLGRIND_SCRIPT}"
            DEPENDS sdl_app
            COMMENT "Profiling sdl_app under callgrind (${CALLGRIND_SECONDS}s)"
            USES_TERMINAL
        )
    endif()
endif()
