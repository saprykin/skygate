find_package(ZLIB REQUIRED)

add_library(skygate-ephemeris STATIC
    src/BundledStarCatalog.cpp
    src/EphemerisEngineStub.cpp
)

target_include_directories(skygate-ephemeris
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(skygate-ephemeris
    PUBLIC
        skygate::core
    PRIVATE
        ZLIB::ZLIB
)

target_compile_features(skygate-ephemeris PUBLIC cxx_std_20)

add_library(skygate::ephemeris ALIAS skygate-ephemeris)
