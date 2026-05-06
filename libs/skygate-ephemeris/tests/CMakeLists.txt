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
    CatalogPayloadParserTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-fixture-corpus-tests
    CatalogFixtureCorpusTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-performance-guard-tests
    CatalogPerformanceGuardTests.cpp
    LABELS perf advisory slow
    TIMEOUT 90
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-delimited-reader-tests
    CatalogDelimitedReaderTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-string-utilities-tests
    StringUtilitiesTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-parsing-utilities-tests
    CatalogParsingUtilitiesTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-payload-format-detector-tests
    CatalogPayloadFormatDetectorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-archive-codec-tests
    CatalogArchiveCodecTests.cpp
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-stellarium-extractor-tests
    StellariumExtractorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-open-ngc-object-mapper-tests
    OpenNgcObjectMapperTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-open-ngc-parser-edge-tests
    OpenNgcParserEdgeTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-hyg-catalog-tests
    HygCatalogParserTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-hyg-gzip-tests
    HygCatalogGzipTests.cpp
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-stellarium-parser-tests
    StellariumConstellationParserTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-factory-tests
    CatalogFactoryTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-catalog-composer-tests
    CatalogComposerTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-constellation-data-codec-tests
    ConstellationDataCodecTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-body-trail-calculator-tests
    BodyTrailCalculatorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-celestial-reference-tests
    CelestialReferenceCalculatorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-observation-event-calculator-tests
    ObservationEventCalculatorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-night-conditions-calculator-tests
    NightConditionsCalculatorTests.cpp
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-engine-baseline-tests
    EphemerisEngineBaselineTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-engine-fallback-tests
    EphemerisEngineFallbackTests.cpp
    LABELS integration
    TIMEOUT 45
)
add_skygate_ephemeris_qtest(
    skygate-ephemeris-regression-tests
    EphemerisRegressionTests.cpp
    LABELS integration
    TIMEOUT 45
)
