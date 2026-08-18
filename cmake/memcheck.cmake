# cmake/memcheck.cmake - local wrapper for the CI valgrind passes.
#
# Driven by the `memcheck` and `memcheck-examples` CMake custom targets
# (see CMakeLists.txt), so `make memcheck` / `ninja memcheck` reproduce the
# exact invocations from .github/workflows/ci.yml without copying flags:
#
#   MEMCHECK_MODE=unit (default) - the unit test binaries under memcheck
#     with --leak-check=full and the full leak policy (definite, indirect,
#     AND possible are fatal; valgrind.supp narrows the "possible" class to
#     known SDL2/box2d noise). Mirrors the linux-valgrind CI job.
#   MEMCHECK_MODE=examples       - the headless example binaries under
#     memcheck with --exit-on-first-error and SDL dummy drivers, killed
#     after 10s. Mirrors the linux-valgrind-examples CI job.
#
# Can also be invoked directly:
#   cmake -DMEMCHECK_BUILD_DIR=build -DMEMCHECK_MODE=examples \
#         -P cmake/memcheck.cmake
#
# Inputs (all optional except MEMCHECK_BUILD_DIR):
#   MEMCHECK_BUILD_DIR        build directory that holds the binaries
#   MEMCHECK_SUPPRESSIONS     path to valgrind.supp (default: ../valgrind.supp)
#   MEMCHECK_MODE             unit (default) | examples
#   MEMCHECK_TARGETS          list of unit-test binary names
#   MEMCHECK_EXAMPLE_TARGETS  list of example binary names
#   MEMCHECK_TEST_FILTER      (unit mode only) substring filter passed as
#                             argv[1] to each test binary, so a focused pass
#                             runs one code path (the memcheck_path_* ctest
#                             entries in CMakeLists.txt drive this)

if(NOT DEFINED MEMCHECK_BUILD_DIR)
    message(FATAL_ERROR
        "MEMCHECK_BUILD_DIR must point at the CMake build directory "
        "(the memcheck targets set this automatically)")
endif()
if(NOT DEFINED MEMCHECK_SUPPRESSIONS)
    set(MEMCHECK_SUPPRESSIONS "${CMAKE_CURRENT_LIST_DIR}/../valgrind.supp")
endif()
if(NOT DEFINED MEMCHECK_MODE)
    set(MEMCHECK_MODE "unit")
endif()
if(NOT DEFINED MEMCHECK_TARGETS)
    set(MEMCHECK_TARGETS sdl_app_tests sdl_app_backend_tests)
endif()
if(NOT DEFINED MEMCHECK_EXAMPLE_TARGETS)
    set(MEMCHECK_EXAMPLE_TARGETS game_of_life_example fixed_timestep_example tictactoe_example)
endif()

find_program(VALGRIND_EXECUTABLE NAMES valgrind)
if(NOT VALGRIND_EXECUTABLE)
    message(FATAL_ERROR
        "valgrind not found in PATH. Install it with e.g. 'sudo apt install valgrind'. "
        "Valgrind only runs on Linux (WSL counts).")
endif()

# Resolve <build>/<target> with a few portability fallbacks.
function(memcheck_resolve_binary target out_var)
    foreach(candidate IN ITEMS
            "${MEMCHECK_BUILD_DIR}/${target}"
            "${MEMCHECK_BUILD_DIR}/${target}.exe"
            "${MEMCHECK_BUILD_DIR}/Release/${target}.exe"
            "${MEMCHECK_BUILD_DIR}/Debug/${target}.exe")
        if(EXISTS "${candidate}")
            set(${out_var} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR
        "Binary for '${target}' not found under ${MEMCHECK_BUILD_DIR}. "
        "Build it first: cmake --build ${MEMCHECK_BUILD_DIR} --target ${target}")
endfunction()

# Same guard the CI linux-valgrind job runs: valgrind parses suppressions
# at startup, so a structurally broken valgrind.supp fails fast here too.
# Only unit mode uses the suppression file (the CI examples job runs with
# --leak-check=no and no suppressions), so only unit mode validates it -
# a broken valgrind.supp must never block `memcheck-examples`.
if(EXISTS "/bin/true")
    set(TRUE_PROGRAM "/bin/true")
elseif(EXISTS "/usr/bin/true")
    set(TRUE_PROGRAM "/usr/bin/true")
else()
    set(TRUE_PROGRAM "")
endif()

if(MEMCHECK_MODE STREQUAL "unit" AND EXISTS "${MEMCHECK_SUPPRESSIONS}" AND TRUE_PROGRAM)
    message(STATUS "Validating ${MEMCHECK_SUPPRESSIONS} ...")
    execute_process(COMMAND "${VALGRIND_EXECUTABLE}" --tool=memcheck
            --suppressions=${MEMCHECK_SUPPRESSIONS} --error-exitcode=1 "${TRUE_PROGRAM}"
        RESULT_VARIABLE supp_result
        OUTPUT_VARIABLE supp_out ERROR_VARIABLE supp_err)
    if(NOT supp_result EQUAL 0)
        message(FATAL_ERROR
            "valgrind rejected ${MEMCHECK_SUPPRESSIONS}:\n${supp_out}${supp_err}")
    endif()
elseif(NOT EXISTS "${MEMCHECK_SUPPRESSIONS}")
    message(WARNING
        "Suppression file not found at ${MEMCHECK_SUPPRESSIONS} - skipping syntax validation")
endif()

if(MEMCHECK_MODE STREQUAL "unit")
    foreach(target IN LISTS MEMCHECK_TARGETS)
        memcheck_resolve_binary("${target}" binary)
        message(STATUS "== valgrind memcheck: ${target} ==")
        # Exact flag set from the CI linux-valgrind job. TIMEOUT is a generous
        # local hang guard (CI uses `timeout 600`; 3600s leaves room for a
        # slow interactive session) - a pathological hang fails the run
        # instead of blocking the terminal forever. On timeout, CMake sets
        # RESULT_VARIABLE to a non-numeric string, so the EQUAL 0 check below
        # correctly treats it as a failure.
        # MEMCHECK_TEST_FILTER (optional) appends a substring filter to the
        # binary so a focused pass runs one code path (the memcheck_path_*
        # ctest entries). Empty/absent = run the whole binary.
        set(_memcheck_cmd "${VALGRIND_EXECUTABLE}"
            --tool=memcheck
            --leak-check=full
            --show-leak-kinds=definite,indirect,possible
            --errors-for-leak-kinds=definite,indirect,possible
            --suppressions=${MEMCHECK_SUPPRESSIONS}
            --track-origins=yes
            --error-exitcode=1
            "${binary}")
        if(DEFINED MEMCHECK_TEST_FILTER AND NOT MEMCHECK_TEST_FILTER STREQUAL "")
            list(APPEND _memcheck_cmd "${MEMCHECK_TEST_FILTER}")
        endif()
        execute_process(COMMAND ${_memcheck_cmd}
            RESULT_VARIABLE vg_result
            OUTPUT_VARIABLE vg_out ERROR_VARIABLE vg_err
            TIMEOUT 3600)
        if(NOT vg_result EQUAL 0)
            message(STATUS "${vg_out}${vg_err}")
            message(FATAL_ERROR "memcheck FAILED on ${target} (exit ${vg_result})")
        endif()
        message(STATUS "== ${target}: clean ==")
    endforeach()
    message(STATUS "memcheck: all targets clean")

elseif(MEMCHECK_MODE STREQUAL "examples")
    find_program(TIMEOUT_EXECUTABLE NAMES timeout)
    if(NOT TIMEOUT_EXECUTABLE)
        message(FATAL_ERROR "GNU 'timeout' (coreutils) not found in PATH")
    endif()
    # Same env as the CI linux-valgrind-examples job: SDL's dummy drivers so
    # no display or audio device is needed. The examples loop forever
    # waiting for a quit event that never comes, so each is killed after 10s.
    set(ENV{SDL_VIDEODRIVER} "dummy")
    set(ENV{SDL_AUDIODRIVER} "dummy")
    # Game binaries (template_game, Cinderfall_game, *-game targets) enable
    # their autoplay hooks via PONG_SMOKE=1 so the valgrind window exercises
    # the real physics instead of a wait-for-input state; smoke auto-restart
    # keeps them live across win/lose (harmless for the plain examples).
    set(ENV{PONG_SMOKE} "1")
    foreach(target IN LISTS MEMCHECK_EXAMPLE_TARGETS)
        memcheck_resolve_binary("${target}" binary)
        message(STATUS "== valgrind memcheck: ${target} (headless, 10s) ==")
        # Same `timeout 10` guard as CI, with one local hardening: GNU timeout
        # escalates to SIGKILL after --kill-after=30 if valgrind somehow
        # ignores SIGTERM (real valgrind handles SIGTERM, so this is pure
        # insurance for an interactive tool; the valgrind flags are identical
        # to CI either way). GNU timeout requires options BEFORE the
        # duration, so --kill-after=30 comes first.
        execute_process(COMMAND "${TIMEOUT_EXECUTABLE}" --kill-after=30 10 "${VALGRIND_EXECUTABLE}"
                --tool=memcheck
                --leak-check=no
                --error-exitcode=1
                --exit-on-first-error=yes
                --track-origins=yes
                "${binary}"
            RESULT_VARIABLE vg_result
            OUTPUT_VARIABLE vg_out ERROR_VARIABLE vg_err)
        if(vg_result EQUAL 124)
            # Clean kill after 10s. Guard the kill/error race like CI: a
            # memcheck report can land in the log even if the exit raced
            # with the timeout, and the exit-time "ERROR SUMMARY" line is
            # not reliably printed on SIGTERM death.
            set(_log "${vg_out}${vg_err}")
            if(_log MATCHES "Invalid (read|write)|uninitialised|uninitialized|is not addressable|Invalid free|Mismatched free|ERROR SUMMARY: [1-9]")
                message(STATUS "${_log}")
                message(FATAL_ERROR
                    "memcheck reported errors on ${target} despite the timeout kill")
            endif()
            message(STATUS "== ${target}: OK (ran 10s, no memcheck error) ==")
        else()
            # Non-124 = memcheck error (exit 1), guest crash, or a start()
            # failure with exit 0 (zero code paths covered) - all failures,
            # mirroring the CI job's strict acceptance rules.
            message(STATUS "${vg_out}${vg_err}")
            message(FATAL_ERROR "memcheck FAILED on ${target} (exit ${vg_result})")
        endif()
    endforeach()
    message(STATUS "memcheck-examples: all examples clean")

else()
    message(FATAL_ERROR "Unknown MEMCHECK_MODE '${MEMCHECK_MODE}' (use 'unit' or 'examples')")
endif()
