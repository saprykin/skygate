cmake_minimum_required(VERSION 3.24)

set(SKYGATE_APP_VERSION "1.0.0")

project(skygate VERSION ${SKYGATE_APP_VERSION} LANGUAGES CXX)

set(SKYGATE_QML_MODULE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(SKYGATE_GIT_HASH "unknown")

include(GNUInstallDirs)

find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE SKYGATE_GIT_HASH
        RESULT_VARIABLE skygateGitHashResult
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT skygateGitHashResult EQUAL 0 OR SKYGATE_GIT_HASH STREQUAL "")
        set(SKYGATE_GIT_HASH "unknown")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SkyGateCoverage)

option(SKYGATE_BUILD_UI "Build Qt UI application" ON)
option(SKYGATE_BUILD_TESTS "Build tests" ON)
option(SKYGATE_ENABLE_COVERAGE "Build tests with coverage instrumentation" OFF)

skygate_enable_coverage()

add_subdirectory(libs/skygate-core)
add_subdirectory(libs/skygate-ephemeris)

if(SKYGATE_BUILD_UI)
    add_subdirectory(apps/skygate-ui)
endif()

if(SKYGATE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/support)
    add_subdirectory(libs/skygate-core/tests)
    add_subdirectory(libs/skygate-ephemeris/tests)
    if(SKYGATE_BUILD_UI)
        add_subdirectory(apps/skygate-ui/tests)
    endif()
endif()

skygate_add_coverage_targets()

if(SKYGATE_BUILD_UI)
    set(skygatePackageLicenseFile "${CMAKE_CURRENT_BINARY_DIR}/SkyGateLicense.txt")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "${skygatePackageLicenseFile}" COPYONLY)

    set(CPACK_PACKAGE_NAME "SkyGate")
    set(CPACK_PACKAGE_VENDOR "SkyGate")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SkyGate desktop sky visualization application")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "SkyGate")
    set(CPACK_RESOURCE_FILE_LICENSE "${skygatePackageLicenseFile}")
    set(CPACK_PACKAGE_EXECUTABLES "skygate-ui" "SkyGate")
    set(CPACK_PACKAGE_FILE_NAME
        "SkyGate-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
    )
    if(WIN32)
        set(CPACK_GENERATOR "ZIP;WIX")
        set(CPACK_WIX_UPGRADE_GUID "4E48F5D2-389E-4116-85B8-91E98366365E")
        set(CPACK_WIX_ROOT_FEATURE_TITLE "SkyGate")
        set(CPACK_WIX_ROOT_FEATURE_DESCRIPTION "SkyGate desktop sky visualization application")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(CPACK_WIX_ARCHITECTURE "x64")
        endif()
    else()
        set(CPACK_GENERATOR "ZIP")
    endif()
    include(CPack)
endif()
