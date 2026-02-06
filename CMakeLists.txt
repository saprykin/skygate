cmake_minimum_required(VERSION 3.24)

project(skygate VERSION 0.1.0 LANGUAGES CXX)

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
    add_subdirectory(tests)
endif()
