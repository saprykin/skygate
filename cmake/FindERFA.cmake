include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_ERFA QUIET erfa)
endif()

find_path(ERFA_INCLUDE_DIR
    NAMES erfa.h
    HINTS ${PC_ERFA_INCLUDE_DIRS}
)

find_library(ERFA_LIBRARY
    NAMES erfa
    HINTS ${PC_ERFA_LIBRARY_DIRS}
)

if(UNIX)
    find_library(ERFA_MATH_LIBRARY NAMES m)
endif()

find_package_handle_standard_args(ERFA
    REQUIRED_VARS ERFA_LIBRARY ERFA_INCLUDE_DIR
    VERSION_VAR PC_ERFA_VERSION
)

if(ERFA_FOUND AND NOT TARGET ERFA::ERFA)
    add_library(ERFA::ERFA UNKNOWN IMPORTED)
    set_target_properties(ERFA::ERFA PROPERTIES
        IMPORTED_LOCATION "${ERFA_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ERFA_INCLUDE_DIR}"
    )
    if(PC_ERFA_CFLAGS_OTHER)
        set_property(TARGET ERFA::ERFA
            PROPERTY INTERFACE_COMPILE_OPTIONS "${PC_ERFA_CFLAGS_OTHER}"
        )
    endif()
    if(ERFA_MATH_LIBRARY)
        set_property(TARGET ERFA::ERFA
            APPEND PROPERTY INTERFACE_LINK_LIBRARIES "${ERFA_MATH_LIBRARY}"
        )
    endif()
endif()

mark_as_advanced(ERFA_INCLUDE_DIR ERFA_LIBRARY ERFA_MATH_LIBRARY)
