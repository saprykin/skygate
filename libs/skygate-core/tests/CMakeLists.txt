add_executable(skygate-core-unit-tests
    main.cpp
    GeoLocationTests.cpp
    ProjectionTests.cpp
)

target_link_libraries(skygate-core-unit-tests
    PRIVATE
        skygate::core
)

target_compile_features(skygate-core-unit-tests PRIVATE cxx_std_20)

add_test(
    NAME skygate-core-unit-tests
    COMMAND $<TARGET_FILE:skygate-core-unit-tests>
)
