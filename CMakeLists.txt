cmake_minimum_required(VERSION 3.24)

set(SKYGATE_APP_VERSION "1.0.0")

project(skygate VERSION ${SKYGATE_APP_VERSION} LANGUAGES CXX)

set(SKYGATE_QML_MODULE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(SKYGATE_GIT_HASH "unknown")

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

option(SKYGATE_BUILD_UI "Build Qt UI application" ON)
option(SKYGATE_BUILD_TESTS "Build tests" ON)

add_subdirectory(libs/skygate-core)
add_subdirectory(libs/skygate-ephemeris)

if(SKYGATE_BUILD_UI)
    add_subdirectory(apps/skygate-ui)
endif()

if(SKYGATE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(libs/skygate-core/tests)
    add_subdirectory(libs/skygate-ephemeris/tests)
    if(SKYGATE_BUILD_UI)
        add_subdirectory(apps/skygate-ui/tests)
    endif()
endif()
