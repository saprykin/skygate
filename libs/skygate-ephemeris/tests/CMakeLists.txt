find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

function(add_skygate_ephemeris_qtest target_name source_file)
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
            skygate::ephemeris
            skygate::core
            skygate::test-support
    )

    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/support
    )

    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_compile_definitions(${target_name}
        PRIVATE
            SKYGATE_EPHEMERIS_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )

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

add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-payload-parser-tests
    catalog/CatalogPayloadParserTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-property-tests
    catalog/CatalogPropertyTests.cpp
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-fixture-corpus-tests
    catalog/CatalogFixtureCorpusTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-performance-guard-tests
    catalog/CatalogPerformanceGuardTests.cpp
    LABELS perf advisory slow
    TIMEOUT 90
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-delimited-reader-tests
    catalog/io/CatalogDelimitedReaderTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-string-utilities-tests
    engine/StringUtilitiesTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-parsing-utilities-tests
    catalog/CatalogParsingUtilitiesTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-payload-format-detector-tests
    catalog/CatalogPayloadFormatDetectorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-archive-codec-tests
    catalog/io/CatalogArchiveCodecTests.cpp
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-archive-inflater-tests
    catalog/io/CatalogArchiveInflaterTests.cpp
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-archive-zip-directory-tests
    catalog/io/CatalogArchiveZipDirectoryTests.cpp
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-archive-zip-entry-extractor-tests
    catalog/io/CatalogArchiveZipEntryExtractorTests.cpp
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-archive-property-tests
    catalog/io/CatalogArchivePropertyTests.cpp
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-stellarium-extractor-tests
    catalog/stellarium/StellariumExtractorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-open-ngc-object-mapper-tests
    catalog/opengc/OpenNgcObjectMapperTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-open-ngc-parser-edge-tests
    catalog/opengc/OpenNgcParserEdgeTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-hyg-catalog-tests
    catalog/hyg/HygCatalogParserTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-hyg-gzip-tests
    catalog/hyg/HygCatalogGzipTests.cpp
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-stellarium-parser-tests
    catalog/stellarium/StellariumConstellationParserTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-factory-tests
    catalog/CatalogFactoryTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-composer-tests
    catalog/CatalogComposerTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-constellation-data-codec-tests
    catalog/constellation/ConstellationDataCodecTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-body-trail-calculator-tests
    engine/BodyTrailCalculatorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-celestial-reference-tests
    engine/CelestialReferenceCalculatorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-observation-event-calculator-tests
    events/ObservationEventCalculatorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-night-conditions-calculator-tests
    events/NightConditionsCalculatorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-engine-baseline-tests
    engine/EphemerisEngineBaselineTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-engine-fallback-tests
    engine/EphemerisEngineFallbackTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-regression-tests
    engine/EphemerisRegressionTests.cpp
    LABELS integration
    TIMEOUT 45
)
