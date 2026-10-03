# EngineHeaderIsolationTest.cmake
#
# Scans application and non-engine ephemeris sources for includes of concrete
# engine implementation headers (engine/simple/*.hpp,
# engine/highprecision/*.hpp, and engine/SimpleEphemerisGuidanceStrategy.hpp).
#
# Concrete engine knowledge is reserved for:
#   - the engine factory (src/factory)
#   - the backend composition component (src/composition)
#   - the engine implementation directories themselves (src/engine/simple and
#     src/engine/highprecision)
#   - test support
#   - the documented, build-gated TdbTtConverter ERFA adapter crossing
#
# Directories passed through ALLOWLIST_PREFIXES must end with "/". File entries
# are matched as exact prefixes.

set(_scanRoots ${SCAN_ROOTS})
set(_allowlistPrefixes ${ALLOWLIST_PREFIXES})
set(_forbiddenTokens
    "engine/simple/"
    "engine/highprecision/"
    "engine/SimpleEphemerisGuidanceStrategy.hpp"
)

set(_scannedFileCount 0)
set(_allowlistedFileCount 0)

foreach(_root IN LISTS _scanRoots)
    file(GLOB_RECURSE _candidateFiles
        "${_root}/*.hpp"
        "${_root}/*.cpp"
        "${_root}/*.h"
        "${_root}/*.cc"
        "${_root}/*.cxx"
    )

    foreach(_file IN LISTS _candidateFiles)
        set(_allowlisted FALSE)
        foreach(_prefix IN LISTS _allowlistPrefixes)
            if(_prefix STREQUAL "")
                continue()
            endif()
            string(FIND "${_file}" "${_prefix}" _prefixPosition)
            if(_prefixPosition EQUAL 0)
                set(_allowlisted TRUE)
                break()
            endif()
        endforeach()

        if(_allowlisted)
            math(EXPR _allowlistedFileCount "${_allowlistedFileCount} + 1")
            continue()
        endif()

        math(EXPR _scannedFileCount "${_scannedFileCount} + 1")

        file(READ "${_file}" _contents)
        string(REPLACE "\r\n" "\n" _contents "${_contents}")
        string(REPLACE "\n" ";" _lines "${_contents}")

        foreach(_line IN LISTS _lines)
            if(NOT _line MATCHES "^[ \t]*#[ \t]*include")
                continue()
            endif()

            foreach(_token IN LISTS _forbiddenTokens)
                string(FIND "${_line}" "${_token}" _tokenPosition)
                if(NOT _tokenPosition EQUAL -1)
                    message(FATAL_ERROR
                        "Forbidden concrete engine header include in ${_file}:\n"
                        "  ${_line}"
                    )
                endif()
            endforeach()
        endforeach()
    endforeach()
endforeach()

message(STATUS
    "Engine header isolation check passed: "
    "${_scannedFileCount} file(s) scanned, "
    "${_allowlistedFileCount} file(s) allowlisted."
)
