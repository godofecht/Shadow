# cmake/smoke.cmake - headless smoke-runner for one example/game binary.
#
# Drives the linux-sanitize CI job's smoke step AND the `smoke_*` ctest
# entries registered in CMakeLists.txt (when UMBRA_SANITIZE=ON +
# UMBRA_BUILD_EXAMPLES=ON), so CI and developers share one implementation
# of the acceptance rule instead of a copy-pasted shell loop:
#
#   - the binary runs with SDL's dummy video/audio drivers (no display or
#     audio device needed) and PONG_SMOKE=1 autoplay (games exercise their
#     real physics instead of parking in a wait-for-input state);
#   - ASAN_OPTIONS / UBSAN_OPTIONS are set exactly as the CI job sets them;
#   - the binary is killed after 5 seconds by GNU `timeout` (or `gtimeout`
#     on macOS, where coreutils ships under that name);
#   - exit 124 (killed by the timeout) = the app looped through its
#     update/render paths without crashing - the expected pass for these
#     infinite-loop apps. An immediate exit 0 is treated as failure too: it
#     only happens when start() failed (e.g. renderer/audio init), i.e.
#     zero code paths covered, so exit 0 is accepted only when the process
#     demonstrably ran for at least 2 seconds. Any other non-zero exit is a
#     crash or sanitizer finding (ASan aborts non-zero) and fails the run.
#
# Usage (the smoke_* ctest entries set these automatically):
#   cmake -DSMOKE_BUILD_DIR=build -DSMOKE_TARGET=game_of_life_example \
#         -P cmake/smoke.cmake
#
# Inputs:
#   SMOKE_BUILD_DIR   build directory that holds the binary (required)
#   SMOKE_TARGET      binary name, e.g. game_of_life_example (required)

if(NOT DEFINED SMOKE_BUILD_DIR OR SMOKE_BUILD_DIR STREQUAL "")
    message(FATAL_ERROR "SMOKE_BUILD_DIR must point at the CMake build directory")
endif()
if(NOT DEFINED SMOKE_TARGET OR SMOKE_TARGET STREQUAL "")
    message(FATAL_ERROR "SMOKE_TARGET must name the binary to smoke-run")
endif()

# GNU timeout is `timeout` on Linux, `gtimeout` on macOS (coreutils).
find_program(TIMEOUT_EXECUTABLE NAMES timeout gtimeout)
if(NOT TIMEOUT_EXECUTABLE)
    message(FATAL_ERROR
        "GNU 'timeout' (coreutils) not found in PATH - needed to kill the "
        "infinite-loop smoke binaries after the 5s window")
endif()

# Resolve <build>/<target> with a few portability fallbacks.
set(_binary "")
foreach(candidate IN ITEMS
        "${SMOKE_BUILD_DIR}/${SMOKE_TARGET}"
        "${SMOKE_BUILD_DIR}/${SMOKE_TARGET}.exe"
        "${SMOKE_BUILD_DIR}/Release/${SMOKE_TARGET}.exe"
        "${SMOKE_BUILD_DIR}/Debug/${SMOKE_TARGET}.exe")
    if(EXISTS "${candidate}")
        set(_binary "${candidate}")
        break()
    endif()
endforeach()
if(NOT _binary)
    message(FATAL_ERROR
        "Binary for '${SMOKE_TARGET}' not found under ${SMOKE_BUILD_DIR}. "
        "Build it first: cmake --build ${SMOKE_BUILD_DIR} --target ${SMOKE_TARGET}")
endif()

# Same env as the CI linux-sanitize smoke step: SDL dummy drivers, autoplay
# for games, and the sanitizer options exactly as CI sets them. Linux
# enables LSan (detect_leaks=1) just like the CI job; macOS clang ships no
# LeakSanitizer, where detect_leaks=1 makes ASan abort every binary with
# "detect_leaks is not supported on this platform" - so it is omitted there
# (same platform note as cmake/sanitize.cmake). Memory errors and UB are
# still caught on both platforms.
set(ENV{SDL_VIDEODRIVER} "dummy")
set(ENV{SDL_AUDIODRIVER} "dummy")
set(ENV{PONG_SMOKE} "1")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ENV{ASAN_OPTIONS} "detect_leaks=1")
else()
    set(ENV{ASAN_OPTIONS} "abort_on_error=1")
endif()
set(ENV{UBSAN_OPTIONS} "halt_on_error=1:print_stacktrace=1")

message(STATUS "== headless smoke: ${SMOKE_TARGET} (5s) ==")
string(TIMESTAMP _start "%s")
execute_process(COMMAND "${TIMEOUT_EXECUTABLE}" 5 "${_binary}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_out ERROR_VARIABLE run_err)
string(TIMESTAMP _end "%s")
math(EXPR _elapsed "${_end} - ${_start}")

if(run_result EQUAL 124)
    # Clean kill after the full 5s window. Guard the kill/report race like
    # memcheck.cmake: a sanitizer report can land in the log even if the
    # exit raced with the timeout kill.
    set(_log "${run_out}${run_err}")
    if(_log MATCHES "AddressSanitizer|runtime error:|LeakSanitizer|UndefinedBehaviorSanitizer")
        message(STATUS "${_log}")
        message(FATAL_ERROR
            "sanitizer reported errors on ${SMOKE_TARGET} despite the timeout kill")
    endif()
    message(STATUS "== ${SMOKE_TARGET}: OK (ran 5s, no finding) ==")
elseif(run_result EQUAL 0 AND _elapsed GREATER_EQUAL 2)
    # Exit 0 only counts when the process demonstrably ran its loop (>= 2s);
    # an immediate exit 0 means start() failed = zero code paths covered.
    message(STATUS "== ${SMOKE_TARGET}: OK (exit 0 after ${_elapsed}s) ==")
else()
    message(STATUS "${run_out}${run_err}")
    message(FATAL_ERROR
        "smoke FAILED on ${SMOKE_TARGET} (exit ${run_result} after ${_elapsed}s) - "
        "crash, sanitizer finding, or no coverage")
endif()
