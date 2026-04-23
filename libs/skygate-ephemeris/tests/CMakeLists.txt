find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

function(add_skygate_ephemeris_qtest target_name source_file)
    add_executable(${target_name}
        ${source_file}
    )

    set_target_properties(${target_name} PROPERTIES AUTOMOC ON)

    target_link_libraries(${target_name}
        PRIVATE
            Qt6::Test
            skygate::ephemeris
            skygate::core
    )

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    add_test(
        NAME ${target_name}
        COMMAND $<TARGET_FILE:${target_name}>
    )
endfunction()

add_skygate_ephemeris_qtest(skygate-ephemeris-catalog-payload-parser-tests CatalogPayloadParserTests.cpp)
add_skygate_ephemeris_qtest(skygate-ephemeris-hyg-catalog-tests HygCatalogParserTests.cpp)
add_skygate_ephemeris_qtest(skygate-ephemeris-hyg-gzip-tests HygCatalogGzipTests.cpp)
add_skygate_ephemeris_qtest(skygate-ephemeris-stellarium-parser-tests StellariumConstellationParserTests.cpp)
add_skygate_ephemeris_qtest(skygate-ephemeris-catalog-factory-tests CatalogFactoryTests.cpp)
add_skygate_ephemeris_qtest(skygate-ephemeris-celestial-reference-tests CelestialReferenceCalculatorTests.cpp)
add_skygate_ephemeris_qtest(skygate-ephemeris-engine-baseline-tests EphemerisEngineBaselineTests.cpp)
add_skygate_ephemeris_qtest(skygate-ephemeris-engine-fallback-tests EphemerisEngineFallbackTests.cpp)
