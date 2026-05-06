find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

function(add_skygate_core_qtest target_name source_file)
    cmake_parse_arguments(
        TEST
        ""
        "TIMEOUT"
        "ENVIRONMENT;LABELS"
        ${ARGN}
    )

    add_executable(${target_name}
        ${source_file}
    )

    set_target_properties(${target_name} PROPERTIES AUTOMOC ON)

    target_link_libraries(${target_name}
        PRIVATE
            Qt6::Test
            skygate::core
    )

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    add_test(
        NAME ${target_name}
        COMMAND $<TARGET_FILE:${target_name}>
    )

    if(NOT TEST_LABELS)
        set(TEST_LABELS unit)
    endif()
    if(NOT TEST_TIMEOUT)
        set(TEST_TIMEOUT 30)
    endif()

    list(REMOVE_DUPLICATES TEST_LABELS)
    set_tests_properties(${target_name}
        PROPERTIES
            LABELS "${TEST_LABELS}"
            TIMEOUT "${TEST_TIMEOUT}"
    )
    if(TEST_ENVIRONMENT)
        set_property(TEST ${target_name}
            PROPERTY ENVIRONMENT "${TEST_ENVIRONMENT}"
        )
    endif()
endfunction()

add_skygate_core_qtest(skygate-core-geolocation-tests GeoLocationTests.cpp)
add_skygate_core_qtest(skygate-core-core-types-tests CoreTypesTests.cpp)
add_skygate_core_qtest(skygate-core-angle-math-tests AngleMathTests.cpp)
add_skygate_core_qtest(skygate-core-geometry-2d-tests Geometry2dTests.cpp)
add_skygate_core_qtest(skygate-core-geometry-helpers-tests GeometryHelpersTests.cpp)
add_skygate_core_qtest(skygate-core-spherical-geometry-tests SphericalGeometryTests.cpp)
add_skygate_core_qtest(skygate-core-projection-factory-tests ProjectionFactoryTests.cpp)
add_skygate_core_qtest(skygate-core-azimuthal-equidistant-projection-tests AzimuthalEquidistantProjectionTests.cpp)
add_skygate_core_qtest(skygate-core-stereographic-projection-tests StereographicProjectionTests.cpp)
add_skygate_core_qtest(skygate-core-perspective-projection-tests PerspectiveProjectionTests.cpp)
add_skygate_core_qtest(skygate-core-prepared-projection-tests PreparedProjectionTests.cpp)
add_skygate_core_qtest(skygate-core-viewport-math-tests ViewportMathTests.cpp)
