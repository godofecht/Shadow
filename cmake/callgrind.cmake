# cmake/callgrind.cmake - local wrapper for profiling a binary with
# valgrind's callgrind tool.
#
# Driven by the `callgrind` and `callgrind-app` CMake custom targets (see
# CMakeLists.txt), so `make callgrind` / `ninja callgrind` profile a binary
# and emit a callgraph report without remembering valgrind flags:
#
#   make callgrind      # profiles bunnymark_example (a CPU hotspot demo)
#   make callgrind-app  # profiles sdl_app
#
# Both run the chosen binary under `valgrind --tool=callgrind` for a bounded
# number of seconds (killed with SIGTERM via GNU timeout - callgrind flushes
# its profile data on SIGTERM; SIGKILL would lose it, so --kill-after is
# deliberately NOT used), then run `callgrind_annotate` to produce a
# human-readable hotspot report. Outputs land in the build dir:
#
#   <build>/callgrind.out.<target>                 raw profile (callgraph
#                                                  data, kcachegrind format)
#   <build>/callgrind.annotate.<target>.txt        annotated hotspot report
#
# Can also be invoked directly:
#   cmake -DCALLGRIND_BUILD_DIR=build \
#         -DCALLGRIND_TARGET=bunnymark_example \
#         -P cmake/callgrind.cmake
#
# Inputs:
#   CALLGRIND_BUILD_DIR      build directory holding the binary (required)
#   CALLGRIND_TARGET         binary name to profile (default bunnymark_example)
#   CALLGRIND_SECONDS        bounded run time in seconds (default 10)
#   CALLGRIND_SOURCE_DIR     source root; used as the working directory so
#                            assets loaded relative to cwd (e.g. fly.png for
#                            bunnymark) resolve. Default: repo root.
#   CALLGRIND_DUMMY_DRIVERS  ON (default): set SDL_VIDEODRIVER/SDL_AUDIODRIVER
#                            to dummy so the run works headless and profiles
#                            the CPU game loop deterministically (software
#                            rendering). OFF: use the real drivers (a display
#                            is required). Mirror of the memcheck-examples job.

if(NOT DEFINED CALLGRIND_BUILD_DIR)
    message(FATAL_ERROR
        "CALLGRIND_BUILD_DIR must point at the CMake build directory "
        "(the callgrind targets set this automatically)")
endif()
if(NOT DEFINED CALLGRIND_TARGET)
    set(CALLGRIND_TARGET "bunnymark_example")
endif()
if(NOT DEFINED CALLGRIND_SECONDS)
    set(CALLGRIND_SECONDS 10)
endif()
if(NOT DEFINED CALLGRIND_SOURCE_DIR)
    set(CALLGRIND_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
endif()
if(NOT DEFINED CALLGRIND_DUMMY_DRIVERS)
    set(CALLGRIND_DUMMY_DRIVERS ON)
endif()

find_program(VALGRIND_EXECUTABLE NAMES valgrind)
if(NOT VALGRIND_EXECUTABLE)
    message(FATAL_ERROR
        "valgrind not found in PATH. Install it with e.g. 'sudo apt install valgrind'. "
        "Valgrind only runs on Linux (WSL counts).")
endif()
find_program(TIMEOUT_EXECUTABLE NAMES timeout)
if(NOT TIMEOUT_EXECUTABLE)
    message(FATAL_ERROR "GNU 'timeout' (coreutils) not found in PATH")
endif()
find_program(CALLGRIND_ANNOTATE NAMES callgrind_annotate)
if(NOT CALLGRIND_ANNOTATE)
    message(FATAL_ERROR
        "callgrind_annotate not found in PATH (it ships with valgrind - "
        "check your valgrind installation)")
endif()

# Resolve <build>/<target> with a few portability fallbacks.
function(callgrind_resolve_binary target out_var)
    foreach(candidate IN ITEMS
            "${CALLGRIND_BUILD_DIR}/${target}"
            "${CALLGRIND_BUILD_DIR}/${target}.exe"
            "${CALLGRIND_BUILD_DIR}/Release/${target}.exe"
            "${CALLGRIND_BUILD_DIR}/Debug/${target}.exe")
        if(EXISTS "${candidate}")
            set(${out_var} "${candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR
        "Binary for '${target}' not found under ${CALLGRIND_BUILD_DIR}. "
        "Build it first: cmake --build ${CALLGRIND_BUILD_DIR} --target ${target}")
endfunction()

callgrind_resolve_binary("${CALLGRIND_TARGET}" binary)

set(out_file "${CALLGRIND_BUILD_DIR}/callgrind.out.${CALLGRIND_TARGET}")
set(annotate_file "${CALLGRIND_BUILD_DIR}/callgrind.annotate.${CALLGRIND_TARGET}.txt")

if(CALLGRIND_DUMMY_DRIVERS)
    set(ENV{SDL_VIDEODRIVER} "dummy")
    set(ENV{SDL_AUDIODRIVER} "dummy")
endif()

message(STATUS "== callgrind: ${CALLGRIND_TARGET} (${CALLGRIND_SECONDS}s, killed with SIGTERM) ==")

# Run the binary under callgrind for CALLGRIND_SECONDS, then SIGTERM it.
# Callgrind flushes the profile on SIGTERM (valgrind's signal handler), so
# the resulting callgrind.out.<target> is complete - but only if we do NOT
# escalate to SIGKILL (--kill-after would lose the data). Working directory
# is the source root so cwd-relative assets (fly.png) resolve.
#
# The execute_process TIMEOUT (seconds + 60s) is a backstop: if valgrind
# somehow ignored SIGTERM, the script regains control with a clear message
# instead of hanging forever (real valgrind handles SIGTERM, so this should
# never fire; the profile may be partial if it does).
math(EXPR cg_backstop "${CALLGRIND_SECONDS} + 60")
execute_process(
    COMMAND "${TIMEOUT_EXECUTABLE}" "${CALLGRIND_SECONDS}"
            "${VALGRIND_EXECUTABLE}" --tool=callgrind
            --callgrind-out-file=${out_file}
            "${binary}"
    WORKING_DIRECTORY "${CALLGRIND_SOURCE_DIR}"
    RESULT_VARIABLE cg_result
    OUTPUT_VARIABLE cg_out ERROR_VARIABLE cg_err
    TIMEOUT "${cg_backstop}")

# timeout returns 124 when it had to kill the process (the normal, expected
# outcome for an infinite-loop app) and the child's exit code if the binary
# exited on its own first. A non-124 exit is suspicious: for bunnymark /
# sdl_app (which loop until killed), exiting early means start() failed
# (e.g. no display with CALLGRIND_DUMMY_DRIVERS=OFF), and the profile would
# cover only init - useless for hotspot finding. Warn loudly but keep the
# profile so the dev can inspect it.
if(NOT cg_result EQUAL 124)
    message(WARNING
        "${CALLGRIND_TARGET} exited with code ${cg_result} instead of being "
        "killed after ${CALLGRIND_SECONDS}s. For an infinite-loop app this "
        "usually means start() failed (e.g. no display with "
        "CALLGRIND_DUMMY_DRIVERS=OFF) - the profile may cover only init.")
endif()

if(NOT EXISTS "${out_file}" OR IS_DIRECTORY "${out_file}")
    message(STATUS "${cg_out}${cg_err}")
    message(FATAL_ERROR
        "callgrind did not produce ${out_file} (result ${cg_result}). "
        "The binary may have failed before the profile flush - run it "
        "directly under valgrind to see why.")
endif()

message(STATUS "== callgrind_annotate: ${annotate_file} ==")
execute_process(
    COMMAND "${CALLGRIND_ANNOTATE}" "${out_file}"
    RESULT_VARIABLE ann_result
    OUTPUT_VARIABLE ann_out ERROR_VARIABLE ann_err
    TIMEOUT 300)
if(NOT ann_result EQUAL 0)
    message(STATUS "${ann_out}${ann_err}")
    message(FATAL_ERROR "callgrind_annotate failed (exit ${ann_result})")
endif()

file(WRITE "${annotate_file}" "${ann_out}")
message(STATUS "== Profile written: ${out_file} ==")
message(STATUS "== Report written:  ${annotate_file} ==")
message(STATUS "== Tip: open the raw profile in kcachegrind for the interactive call graph ==")

# Show the top of the report (the biggest hotspots) so the terminal run is
# useful without opening a file.
message(STATUS "== Top of report: ==")
message(STATUS "--------------------------------------------------------------------------------")
if(ann_out MATCHES "([^\n]+\n){0,40}")
    string(REGEX MATCH "([^\n]+\n){0,40}" top_snippet "${ann_out}")
    message(STATUS "${top_snippet}")
endif()
message(STATUS "--------------------------------------------------------------------------------")
