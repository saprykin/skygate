add_executable(skygate-ephemeris-unit-tests
    main.cpp
    StarCatalogTests.cpp
    EphemerisEngineTests.cpp
)

target_link_libraries(skygate-ephemeris-unit-tests
    PRIVATE
        skygate::core
        skygate::ephemeris
)

target_compile_features(skygate-ephemeris-unit-tests PRIVATE cxx_std_20)

add_test(
    NAME skygate-ephemeris-unit-tests
    COMMAND $<TARGET_FILE:skygate-ephemeris-unit-tests>
)
