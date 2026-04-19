find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

function(add_skygate_core_qtest target_name source_file)
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
endfunction()

add_skygate_core_qtest(skygate-core-geolocation-tests GeoLocationTests.cpp)
add_skygate_core_qtest(skygate-core-core-types-tests CoreTypesTests.cpp)
add_skygate_core_qtest(skygate-core-projection-factory-tests ProjectionFactoryTests.cpp)
add_skygate_core_qtest(skygate-core-azimuthal-equidistant-projection-tests AzimuthalEquidistantProjectionTests.cpp)
add_skygate_core_qtest(skygate-core-stereographic-projection-tests StereographicProjectionTests.cpp)
add_skygate_core_qtest(skygate-core-perspective-projection-tests PerspectiveProjectionTests.cpp)
add_skygate_core_qtest(skygate-core-prepared-projection-tests PreparedProjectionTests.cpp)
add_skygate_core_qtest(skygate-core-viewport-math-tests ViewportMathTests.cpp)
