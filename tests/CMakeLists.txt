add_executable(skygate-unit-tests
    skygate-unit-tests.cpp
)

target_link_libraries(skygate-unit-tests
    PRIVATE
        skygate::core
        skygate::ephemeris
)

target_compile_features(skygate-unit-tests PRIVATE cxx_std_20)

add_test(
    NAME skygate-unit-tests
    COMMAND $<TARGET_FILE:skygate-unit-tests>
)
