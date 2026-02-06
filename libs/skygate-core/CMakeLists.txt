add_library(skygate-core STATIC
    src/GeoLocation.cpp
)

target_include_directories(skygate-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(skygate-core PUBLIC cxx_std_20)

add_library(skygate::core ALIAS skygate-core)
