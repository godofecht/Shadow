# cmake/sanitize.cmake - run the ASan+UBSan unit suite plus a Brick Breaker+
# headless smoke.
#
# Driven by the top-level Makefile's `sanitize` target (`make sanitize`),
# which configures build-sanitize/ with UMBRA_SANITIZE=ON and builds the
# binaries first. This script is the runtime half of the CI linux-sanitize
# job:
#   1. sdl_app_tests + sdl_app_backend_tests under ASan+UBSan (halt on the
#      first finding; the build's -fno-sanitize-recover=all already aborts on
#      UB, and ASan aborts on its own errors).
#   2. BrickBreakerPlus_game headless for SANITIZE_SMOKE_SECONDS with SDL's
#      dummy video/audio drivers and PONG_SMOKE=1 autoplay, so the smoke
#      window exercises real serve/rally/catch physics, not a wait-for-input
#      state.
#
# Platform note: macOS clang ships no LeakSanitizer (detect_leaks=1 aborts
# with an unsupported-option error there), so detect_leaks is only enabled on
# Linux. Memory errors and UB are still caught on both platforms.
#
# Can also be invoked directly after a sanitize build exists:
#   cmake -DSANITIZE_BUILD_DIR=build-sanitize -P cmake/sanitize.cmake
#
# Inputs (both optional except SANITIZE_BUILD_DIR):
#   SANITIZE_BUILD_DIR        build directory that holds the binaries
#   SANITIZE_SMOKE_SECONDS    smoke window in seconds (default 6)

if(NOT DEFINED SANITIZE_BUILD_DIR)
    message(FATAL_ERROR
        "SANITIZE_BUILD_DIR must point at the sanitize build directory "
        "(the Makefile `sanitize` target sets this automatically)")
endif()
if(NOT DEFINED SANITIZE_SMOKE_SECONDS)
    set(SANITIZE_SMOKE_SECONDS 6)
endif()

# Sanitizer runtime options, matching the CI linux-sanitize job's env. Linux
# enables LSan; macOS (no LSan) omits detect_leaks so the run doesn't abort
# on the unsupported option.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ENV{ASAN_OPTIONS} "detect_leaks=1:abort_on_error=1")
else()
    set(ENV{ASAN_OPTIONS} "abort_on_error=1")
endif()
set(ENV{UBSAN_OPTIONS} "halt_on_error=1:print_stacktrace=1")

# Resolve <build>/<target> with the same portability fallbacks as
# cmake/memcheck.cmake (the .exe variants are harmless on non-Windows).
function(sanitize_resolve_binary target out_var)
    foreach(candidate IN ITEMS
            "${SANITIZE_BUILD_DIR}/${target}"
            "${SANITIZE_BUILD_DIR}/${target}.exe"
            "${SANITIZE_BUILD_DIR}/Release/${target}.exe"
            "${SANITIZE_BUILD_DIR}/Debug/${target}.exe")
        if(EXISTS "${candidate}")
            set(${out_var} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR
        "Binary for '${target}' not found under ${SANITIZE_BUILD_DIR}. "
        "Build it first: cmake --build ${SANITIZE_BUILD_DIR} --target ${target}")
endfunction()

# 1. The unit suite. TIMEOUT is a generous hang guard (the suite runs in
# seconds normally); on timeout RESULT_VARIABLE becomes a non-numeric string,
# so the EQUAL 0 check correctly reports a failure.
foreach(target IN ITEMS sdl_app_tests sdl_app_backend_tests)
    sanitize_resolve_binary("${target}" binary)
    message(STATUS "== ASan+UBSan: ${target} ==")
    execute_process(COMMAND "${binary}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out ERROR_VARIABLE err
        TIMEOUT 600)
    if(NOT result EQUAL 0)
        message(STATUS "${out}${err}")
        message(FATAL_ERROR "ASan+UBSan FAILED on ${target} (exit ${result})")
    endif()
    message(STATUS "== ${target}: clean ==")
endforeach()

# 2. The headless smoke. Same acceptance rule as the CI linux-sanitize job:
# exit 124 (killed by GNU timeout) = ran the full window without crashing; a
# clean exit 0 is only accepted when the process demonstrably ran for at
# least 2s (so an immediate start() failure can't masquerade as coverage).
find_program(TIMEOUT_EXECUTABLE NAMES timeout)
if(NOT TIMEOUT_EXECUTABLE)
    message(FATAL_ERROR "GNU 'timeout' (coreutils) not found in PATH")
endif()
set(ENV{SDL_VIDEODRIVER} "dummy")
set(ENV{SDL_AUDIODRIVER} "dummy")
set(ENV{PONG_SMOKE} "1")

set(_smoke_target BrickBreakerPlus_game)
sanitize_resolve_binary("${_smoke_target}" _smoke_binary)
message(STATUS "== ASan+UBSan: ${_smoke_target} (headless, ${SANITIZE_SMOKE_SECONDS}s) ==")
string(TIMESTAMP _start "%s")
execute_process(COMMAND "${TIMEOUT_EXECUTABLE}" "${SANITIZE_SMOKE_SECONDS}" "${_smoke_binary}"
    RESULT_VARIABLE smoke_result
    OUTPUT_VARIABLE smoke_out ERROR_VARIABLE smoke_err)
string(TIMESTAMP _end "%s")
math(EXPR _elapsed "${_end} - ${_start}")

if(smoke_result EQUAL 124)
    message(STATUS "== ${_smoke_target}: OK (ran ${SANITIZE_SMOKE_SECONDS}s, no finding) ==")
elseif(smoke_result EQUAL 0 AND _elapsed GREATER_EQUAL 2)
    message(STATUS "== ${_smoke_target}: OK (clean exit 0 after ${_elapsed}s) ==")
else()
    message(STATUS "${smoke_out}${smoke_err}")
    message(FATAL_ERROR
        "ASan+UBSan smoke FAILED on ${_smoke_target} (exit ${smoke_result} after ${_elapsed}s)")
endif()

message(STATUS "sanitize: unit suite clean, ${_smoke_target} smoke clean")
