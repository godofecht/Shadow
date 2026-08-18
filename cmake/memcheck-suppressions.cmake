# cmake/memcheck-suppressions.cmake - harvest new valgrind.supp entries.
#
# Driven by the `memcheck-suppressions` CMake custom target (see
# CMakeLists.txt), so `make memcheck-suppressions` / `ninja
# memcheck-suppressions` automates the refinement loop documented in
# valgrind.supp's header and the linux-valgrind CI job: run the unit tests
# under valgrind with --gen-suppressions=all, capture the suppression
# blocks valgrind suggests for NEW third-party noise, and append them to
# valgrind.supp.
#
# POLICY (the invariant that keeps this safe):
#   * Only `Memcheck:Leak` blocks whose match-leak-kinds is exactly
#     "possible" are auto-added. Definite and indirect leaks, and every
#     non-leak error block (Memcheck:Addr, Memcheck:Cond, ...) are NEVER
#     auto-suppressed - those are real defects that must be fixed, not
#     papered over. This matches the trial policy in the existing file.
#   * The current valgrind.supp is passed to the harvest run, so
#     already-known noise stays suppressed in the run's output (cleaner
#     logs). Note that --gen-suppressions=all (vs yes) generates blocks
#     for suppressed errors too - the DEDUPE below, not the suppressions
#     file, is what stops known sites from being re-added every run.
#   * Blocks are deduplicated against existing entries by frame prefix:
#     a generated block whose frames start with an existing block's frames
#     is the same site and is skipped (generated blocks are full stacks;
#     the hand-written ones are trimmed to the library frames).
#   * The updated file is validated with valgrind's own suppression parse
#     (valgrind --suppressions=<new> /bin/true) BEFORE it replaces the
#     original - a structurally broken file never lands on disk.
#
# Inputs (mirror memcheck.cmake):
#   MEMCHECK_BUILD_DIR      build directory holding the binaries (required)
#   MEMCHECK_SUPPRESSIONS   path to valgrind.supp (default ../valgrind.supp)
#   MEMCHECK_TARGETS        unit-test binary names (default both test targets)
#   MEMCHECK_CHECK_ONLY     ON = dry run used by the linux-valgrind CI job:
#                           report what WOULD be added and fail, without
#                           ever touching valgrind.supp. `make
#                           memcheck-suppressions` must be a no-op on a
#                           clean tree, so a would-add result means
#                           suppression drift that should have been
#                           committed.

if(NOT DEFINED MEMCHECK_BUILD_DIR)
    message(FATAL_ERROR
        "MEMCHECK_BUILD_DIR must point at the CMake build directory "
        "(the memcheck-suppressions target sets this automatically)")
endif()
if(NOT DEFINED MEMCHECK_SUPPRESSIONS)
    set(MEMCHECK_SUPPRESSIONS "${CMAKE_CURRENT_LIST_DIR}/../valgrind.supp")
endif()
if(NOT DEFINED MEMCHECK_TARGETS)
    set(MEMCHECK_TARGETS sdl_app_tests sdl_app_backend_tests)
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

# Split text into a list of suppression blocks. Each block is captured as a
# multi-line string starting after the opening "{" (including the name line,
# which is dropped later when emitting).
function(harvest_parse_blocks text out_blocks)
    string(REPLACE "\n" ";" _lines "${text}")
    set(_blocks "")
    set(_in FALSE)
    set(_cur "")
    foreach(_line IN LISTS _lines)
        string(STRIP "${_line}" _s)
        if(_s STREQUAL "{")
            set(_in TRUE)
            set(_cur "")
        elseif(_s STREQUAL "}")
            if(_in)
                list(APPEND _blocks "${_cur}")
                set(_in FALSE)
            endif()
        elseif(_in)
            if(NOT _cur STREQUAL "")
                string(APPEND _cur "\n")
            endif()
            string(APPEND _cur "${_line}")
        endif()
    endforeach()
    set(${out_blocks} "${_blocks}" PARENT_SCOPE)
endfunction()

# Extract the ordered list of fun:/obj: frame lines from a block body.
# Frame lines are indented ("   fun:malloc"), so strip each line first.
# Returns the frames joined with "\n": a frame is a single line by
# construction, so newline can never appear inside one (unlike "|", which
# demangled C++ names such as std::operator| can contain). The result is
# also ONE list element - appending it to a list of frame-sets must not
# flatten (CMake lists split on ";", which fun: lines contain).
function(harvest_frames block out_frames)
    string(REPLACE "\n" ";" _lines "${block}")
    set(_frames "")
    foreach(_line IN LISTS _lines)
        string(STRIP "${_line}" _s)
        if(_s MATCHES "^(fun|obj):")
            if(NOT _frames STREQUAL "")
                string(APPEND _frames "\n")
            endif()
            string(APPEND _frames "${_s}")
        endif()
    endforeach()
    set(${out_frames} "${_frames}" PARENT_SCOPE)
endfunction()

# Policy gate: is this block a Memcheck:Leak whose match-leak-kinds is
# exactly "possible"? Everything else is rejected.
function(harvest_is_possible_leak block out_var)
    string(REPLACE "\n" ";" _lines "${block}")
    set(_is_leak FALSE)
    set(_kinds "")
    foreach(_line IN LISTS _lines)
        string(STRIP "${_line}" _s)
        if(_s STREQUAL "Memcheck:Leak")
            set(_is_leak TRUE)
        elseif(_s MATCHES "^match-leak-kinds:")
            string(REGEX REPLACE "^match-leak-kinds:[ \t]*" "" _k "${_s}")
            set(_kinds "${_k}")
        endif()
    endforeach()
    if(NOT _is_leak OR _kinds STREQUAL "")
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()
    set(_ok TRUE)
    string(REPLACE "," ";" _kind_list "${_kinds}")
    foreach(_k IN LISTS _kind_list)
        string(STRIP "${_k}" _k)
        if(NOT _k STREQUAL "possible")
            set(_ok FALSE)
        endif()
    endforeach()
    set(${out_var} ${_ok} PARENT_SCOPE)
endfunction()

# True if short_set ("\n"-joined frames) is a prefix of long_set, in order.
# Both are single strings; split back to elements for the comparison.
function(harvest_is_prefix long_set short_set out_var)
    string(REPLACE "\n" ";" _long_list "${long_set}")
    string(REPLACE "\n" ";" _short_list "${short_set}")
    list(LENGTH _short_list _short_len)
    list(LENGTH _long_list _long_len)
    if(_short_len GREATER _long_len)
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()
    set(_i 0)
    set(_res TRUE)
    while(_i LESS _short_len)
        list(GET _short_list ${_i} _s)
        list(GET _long_list ${_i} _l)
        if(NOT _s STREQUAL _l)
            set(_res FALSE)
            break()
        endif()
        math(EXPR _i "${_i} + 1")
    endwhile()
    set(${out_var} ${_res} PARENT_SCOPE)
endfunction()

# ---- 1. Harvest generated blocks from the unit tests ---------------------
set(all_blocks "")
foreach(target IN LISTS MEMCHECK_TARGETS)
    memcheck_resolve_binary("${target}" binary)
    message(STATUS "== harvesting suppressions: ${target} ==")
    execute_process(COMMAND "${VALGRIND_EXECUTABLE}"
            --tool=memcheck
            --leak-check=full
            --show-leak-kinds=definite,indirect,possible
            --errors-for-leak-kinds=definite,indirect,possible
            --suppressions=${MEMCHECK_SUPPRESSIONS}
            --gen-suppressions=all
            --error-exitcode=1
            "${binary}"
        RESULT_VARIABLE vg_result
        OUTPUT_VARIABLE vg_out ERROR_VARIABLE vg_err
        TIMEOUT 3600)
    if(NOT vg_result EQUAL 0)
        # Non-zero is EXPECTED when new noise was found (with the current
        # suppressions applied, a clean run exits 0). Not an error here.
        message(STATUS "valgrind exit ${vg_result} on ${target} (expected when new noise is found)")
    endif()
    harvest_parse_blocks("${vg_out}${vg_err}" blocks)
    list(APPEND all_blocks ${blocks})
endforeach()

# ---- 2. Existing blocks + their frame prefixes ---------------------------
set(existing_frames "")
if(EXISTS "${MEMCHECK_SUPPRESSIONS}")
    file(READ "${MEMCHECK_SUPPRESSIONS}" existing_text)
    harvest_parse_blocks("${existing_text}" existing_blocks)
    foreach(eb IN LISTS existing_blocks)
        harvest_frames("${eb}" eb_frames)
        list(APPEND existing_frames "${eb_frames}")
    endforeach()
endif()

# ---- 3. Policy-filter, dedupe, and collect new blocks ---------------------
set(new_blocks "")
set(n_added 0)
set(n_dup 0)
set(n_rejected_leak 0)  # definite/indirect/reachable leaks - REAL bugs to fix
set(n_rejected_error 0)  # Memcheck:Addr/Cond/... error blocks - also real bugs
foreach(nb IN LISTS all_blocks)
    harvest_is_possible_leak("${nb}" ok)
    if(NOT ok)
        # Separate the two rejected classes so the summary can tell the
        # user "you have a real leak/bug to fix", not just "something was
        # skipped". A block is a leak if it names Memcheck:Leak at all.
        if("${nb}" MATCHES "Memcheck:Leak")
            math(EXPR n_rejected_leak "${n_rejected_leak} + 1")
        else()
            math(EXPR n_rejected_error "${n_rejected_error} + 1")
        endif()
        continue()
    endif()
    harvest_frames("${nb}" nb_frames)
    # A possible-kind leak with no frames would be emitted as a catch-all
    # suppression (matches ANY possible leak). Valgrind always emits at
    # least one allocating frame in practice, but refuse to add a block
    # that cannot be scoped to a site.
    if(nb_frames STREQUAL "")
        math(EXPR n_rejected_error "${n_rejected_error} + 1")
        continue()
    endif()
    set(is_dup FALSE)
    foreach(ef IN LISTS existing_frames)
        harvest_is_prefix("${nb_frames}" "${ef}" _prefix_hit)
        if(_prefix_hit)
            set(is_dup TRUE)
            break()
        endif()
    endforeach()
    if(is_dup)
        math(EXPR n_dup "${n_dup} + 1")
        continue()
    endif()
    list(APPEND new_blocks "${nb}")
    list(APPEND existing_frames "${nb_frames}")
    math(EXPR n_added "${n_added} + 1")
endforeach()

if(n_added EQUAL 0)
    message(STATUS "No new policy-safe suppression blocks found - ${MEMCHECK_SUPPRESSIONS} unchanged "
        "(${n_dup} duplicate, ${n_rejected_leak} definite/indirect leak, ${n_rejected_error} error block)")
    return()
endif()

# ---- 3.5. Check-only mode: report + fail, never touch the file ------------
# CI fail-fast (linux-valgrind job): on a clean tree `make
# memcheck-suppressions` is a no-op, so a run that WOULD add blocks means
# valgrind noise has drifted and the refined valgrind.supp was never
# committed. The rejected-leak/error warnings still fire - they are real
# bugs, though they would also fail the memcheck tests themselves.
if(MEMCHECK_CHECK_ONLY)
    if(n_rejected_leak GREATER 0)
        message(WARNING
            "${n_rejected_leak} definite/indirect leak block(s) were NOT suppressed - "
            "those are real leaks and must be FIXED in code, not papered over")
    endif()
    if(n_rejected_error GREATER 0)
        message(WARNING
            "${n_rejected_error} error-kind block(s) (Memcheck:Addr/Cond/...) were NOT "
            "suppressed - those are real defects and must be FIXED in code")
    endif()
    message(FATAL_ERROR
        "${n_added} new possible-only suppression block(s) WOULD be added to "
        "${MEMCHECK_SUPPRESSIONS} - valgrind noise has drifted. Run "
        "`make memcheck-suppressions` locally, review the proposed blocks "
        "(possible-only policy, deduped, validated), and commit the refined "
        "file.")
endif()

# ---- 4. Build the updated file, validate, then replace --------------------
set(new_content "${existing_text}")
if(NOT new_content MATCHES "\n$")
    string(APPEND new_content "\n")
endif()
string(APPEND new_content
    "\n# Blocks auto-added by `make memcheck-suppressions` (harvested with\n"
    "# --gen-suppressions=all). Policy: match-leak-kinds: possible only -\n"
    "# definite/indirect leaks and error kinds are never auto-suppressed.\n")
set(_n 1)
foreach(nb IN LISTS new_blocks)
    # Drop the captured name line (first line after the opening brace).
    # string(FIND/SUBSTRING rather than a regex: CMake's regex engine does
    # not reliably treat \n inside a character class, so a pattern like
    # "^[^\n]*\n" would match past the first newline and truncate.
    string(FIND "${nb}" "\n" _nl_idx)
    if(_nl_idx GREATER -1)
        math(EXPR _body_start "${_nl_idx} + 1")
        string(SUBSTRING "${nb}" ${_body_start} -1 body)
    else()
        set(body "${nb}")
    endif()
    string(APPEND new_content "{\n   generated_possible_leak_${_n}\n${body}\n}\n\n")
    math(EXPR _n "${_n} + 1")
endforeach()

set(temp_file "${MEMCHECK_SUPPRESSIONS}.new")
file(WRITE "${temp_file}" "${new_content}")

if(EXISTS "/bin/true")
    set(TRUE_PROGRAM "/bin/true")
elseif(EXISTS "/usr/bin/true")
    set(TRUE_PROGRAM "/usr/bin/true")
else()
    set(TRUE_PROGRAM "")
endif()
if(TRUE_PROGRAM)
    execute_process(COMMAND "${VALGRIND_EXECUTABLE}" --tool=memcheck
            --suppressions=${temp_file} --error-exitcode=1 "${TRUE_PROGRAM}"
        RESULT_VARIABLE supp_result
        OUTPUT_VARIABLE supp_out ERROR_VARIABLE supp_err)
    if(NOT supp_result EQUAL 0)
        file(REMOVE "${temp_file}")
        message(FATAL_ERROR
            "Updated ${MEMCHECK_SUPPRESSIONS} failed valgrind's suppression parse - "
            "the original file was NOT replaced:\n${supp_out}${supp_err}")
    endif()
endif()

file(RENAME "${temp_file}" "${MEMCHECK_SUPPRESSIONS}")
message(STATUS "Added ${n_added} new possible-only suppression block(s) to ${MEMCHECK_SUPPRESSIONS}")
message(STATUS "Skipped ${n_dup} duplicate(s)")
if(n_rejected_leak GREATER 0)
    message(WARNING
        "${n_rejected_leak} definite/indirect leak block(s) were NOT suppressed - "
        "those are real leaks and must be FIXED in code, not papered over")
endif()
if(n_rejected_error GREATER 0)
    message(WARNING
        "${n_rejected_error} error-kind block(s) (Memcheck:Addr/Cond/...) were NOT "
        "suppressed - those are real defects and must be FIXED in code")
endif()
message(STATUS "Re-run `make memcheck` (or ctest -R '^memcheck_unit') to confirm the noise is gone.")
