find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    message(FATAL_ERROR
        "SkyGate requires zlib for gzip and ZIP catalog import. "
        "Install vcpkg, set VCPKG_ROOT, and configure with one of the "
        "*-vcpkg presets, or provide a system zlib through CMAKE_PREFIX_PATH."
    )
endif()

if(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    find_package(calceph CONFIG REQUIRED GLOBAL)
    find_package(zstd CONFIG REQUIRED GLOBAL)
    find_package(ERFA REQUIRED)
endif()

set(SKYGATE_EPHEMERIS_SIMPLE_ENGINE_SOURCES
    src/engine/simple/AstronomicalTime.hpp
    src/engine/simple/EclipticToEquatorialCalculator.hpp
    src/engine/simple/EquatorialToHorizontalCalculator.hpp
    src/engine/simple/MoonEquatorialCalculator.hpp
    src/engine/simple/PlanetEquatorialCalculator.hpp
    src/engine/simple/SunEquatorialCalculator.hpp
    src/engine/simple/SimpleEphemerisEngine.cpp
    src/engine/simple/AstronomicalTime.cpp
    src/engine/simple/EclipticToEquatorialCalculator.cpp
    src/engine/simple/EquatorialToHorizontalCalculator.cpp
    src/engine/simple/MoonEquatorialCalculator.cpp
    src/engine/simple/PlanetEquatorialCalculator.cpp
    src/engine/simple/SunEquatorialCalculator.cpp
)

set(SKYGATE_EPHEMERIS_HIGHPRECISION_ENGINE_SOURCES)
list(APPEND SKYGATE_EPHEMERIS_HIGHPRECISION_ENGINE_SOURCES
    src/engine/highprecision/ApparentPlaceCalculator.hpp
    src/engine/highprecision/ApparentPlaceCalculator.cpp
    src/engine/highprecision/AtmosphericRefractionCalculator.hpp
    src/engine/highprecision/AtmosphericRefractionCalculator.cpp
    src/engine/highprecision/CatalogStarAstrometryArrays.hpp
    src/engine/highprecision/CatalogStarAstrometryArrays.cpp
    src/engine/highprecision/CalcephKernelProvider.hpp
    src/engine/highprecision/CalcephKernelProvider.cpp
    src/engine/highprecision/HighPrecisionEphemerisEngine.hpp
    src/engine/highprecision/HighPrecisionEphemerisEngine.cpp
    src/engine/highprecision/DeltaTProvider.cpp
    src/engine/highprecision/EarthOrientationProvider.cpp
    src/engine/highprecision/EphemerisDataActivation.cpp
    src/engine/highprecision/EphemerisDataManifest.cpp
    src/engine/highprecision/EphemerisComputationCache.hpp
    src/engine/highprecision/EphemerisComputationCache.cpp
    src/engine/highprecision/EphemerisMetadataMerge.hpp
    src/engine/highprecision/EphemerisMetadataMerge.cpp
    src/engine/highprecision/EphemerisResultBuilder.hpp
    src/engine/highprecision/EphemerisResultBuilder.cpp
    src/engine/highprecision/ErfaAstrometry.hpp
    src/engine/highprecision/ErfaAstrometry.cpp
    src/engine/highprecision/FrameTransformer.hpp
    src/engine/highprecision/FrameTransformer.cpp
    src/engine/highprecision/HighPrecisionTextParser.hpp
    src/engine/highprecision/HighPrecisionTextParser.cpp
    src/engine/highprecision/LeapSecondProvider.cpp
    src/engine/highprecision/ObserverGeodesy.hpp
    src/engine/highprecision/ObserverGeodesy.cpp
    src/engine/highprecision/SolarSystemStateCalculator.hpp
    src/engine/highprecision/SolarSystemStateCalculator.cpp
    src/engine/highprecision/StarAstrometryCalculator.hpp
    src/engine/highprecision/StarAstrometryCalculator.cpp
    src/engine/highprecision/TimeScaleService.cpp
)

if(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    add_library(skygate-ephemeris-highprecision-dependencies INTERFACE)
    target_link_libraries(skygate-ephemeris-highprecision-dependencies
        INTERFACE
            calceph
            zstd::libzstd
            ERFA::ERFA
    )
    add_library(
        skygate::ephemeris-highprecision-dependencies
        ALIAS skygate-ephemeris-highprecision-dependencies
    )
endif()

add_library(skygate-ephemeris STATIC
    ${SKYGATE_EPHEMERIS_SIMPLE_ENGINE_SOURCES}
    ${SKYGATE_EPHEMERIS_HIGHPRECISION_ENGINE_SOURCES}
    include/skygate/ephemeris/CatalogLoadResult.hpp
    include/skygate/ephemeris/BodyTrailCalculator.hpp
    include/skygate/ephemeris/CatalogComposer.hpp
    include/skygate/ephemeris/CatalogFactory.hpp
    include/skygate/ephemeris/CatalogLoader.hpp
    include/skygate/ephemeris/CatalogPayloadParser.hpp
    include/skygate/ephemeris/CelestialReferenceCalculator.hpp
    include/skygate/ephemeris/ConstellationData.hpp
    include/skygate/ephemeris/ConstellationDataCodec.hpp
    include/skygate/ephemeris/ConstellationReferenceCalculator.hpp
    include/skygate/ephemeris/DeltaTProvider.hpp
    include/skygate/ephemeris/EarthOrientationProvider.hpp
    include/skygate/ephemeris/EphemerisDataActivation.hpp
    include/skygate/ephemeris/EphemerisDataManifest.hpp
    include/skygate/ephemeris/EphemerisDataSnapshot.hpp
    include/skygate/ephemeris/EphemerisEngineFactory.hpp
    include/skygate/ephemeris/EphemerisEngineQueries.hpp
    include/skygate/ephemeris/EphemerisPrecisionPolicy.hpp
    include/skygate/ephemeris/EphemerisRequestFactory.hpp
    include/skygate/ephemeris/IEphemerisEngine.hpp
    include/skygate/ephemeris/LeapSecondProvider.hpp
    include/skygate/ephemeris/NightConditionsCalculator.hpp
    include/skygate/ephemeris/IStarCatalog.hpp
    include/skygate/ephemeris/ObservationEventCalculator.hpp
    include/skygate/ephemeris/StellariumConstellationParser.hpp
    include/skygate/ephemeris/TimeScaleService.hpp
    include/skygate/ephemeris/Types.hpp
    src/StringUtilities.hpp
    src/catalog/normalize/CatalogBodyNormalization.hpp
    src/catalog/model/CatalogBodyParseResult.hpp
    src/catalog/bundled/BundledCatalogParser.hpp
    src/catalog/model/CatalogIdentity.hpp
    src/catalog/normalize/CatalogParsingUtilities.hpp
    src/catalog/io/CatalogPayloadFormatDetector.hpp
    src/catalog/io/CatalogZipEntrySelector.hpp
    src/catalog/io/CompressedDataInflater.hpp
    src/catalog/composition/CoreBodyCatalogAugmenter.hpp
    src/catalog/composition/DeepSkyCatalogMerger.hpp
    src/catalog/io/DelimitedCatalogReader.hpp
    src/catalog/opengc/OpenNgcObjectMapper.hpp
    src/catalog/stellarium/StellariumHipParser.hpp
    src/catalog/stellarium/StellariumLabelRefExtractor.hpp
    src/catalog/stellarium/StellariumLineRefExtractor.hpp
    src/catalog/io/zip/ZipBinaryUtilities.hpp
    src/StringUtilities.cpp
    src/catalog/normalize/CatalogBodyNormalization.cpp
    src/catalog/model/CatalogIdentity.cpp
    src/catalog/CatalogComposer.cpp
    src/catalog/normalize/CatalogParsingUtilities.cpp
    src/catalog/CatalogFactory.cpp
    src/catalog/CatalogLoader.cpp
    src/catalog/io/CatalogPayloadFormatDetector.cpp
    src/catalog/CatalogPayloadParser.cpp
    src/catalog/io/CatalogZipEntrySelector.cpp
    src/catalog/io/CompressedDataInflater.cpp
    src/catalog/constellation/ConstellationDataCodec.cpp
    src/catalog/composition/CoreBodyCatalogAugmenter.cpp
    src/catalog/composition/DeepSkyCatalogMerger.cpp
    src/catalog/io/DelimitedCatalogReader.cpp
    src/catalog/io/CsvRowTokenizer.hpp
    src/catalog/hyg/HygCatalogParser.hpp
    src/catalog/hyg/HygGzipCatalogParser.hpp
    src/catalog/model/InMemoryStarCatalog.hpp
    src/catalog/opengc/OpenNgcObjectMapper.cpp
    src/catalog/opengc/OpenNgcCatalogParser.hpp
    src/catalog/io/zip/ZipCodec.hpp
    src/catalog/io/zip/ZipDirectoryReader.cpp
    src/catalog/io/zip/ZipDirectoryReader.hpp
    src/catalog/io/zip/ZipEntryExtractor.cpp
    src/catalog/io/zip/ZipEntryExtractor.hpp
    src/catalog/stellarium/StellariumConstellationParser.cpp
    src/catalog/stellarium/StellariumHipParser.cpp
    src/catalog/stellarium/StellariumLabelRefExtractor.cpp
    src/catalog/stellarium/StellariumLineRefExtractor.cpp
    src/CelestialReferenceCalculator.cpp
    src/ConstellationReferenceCalculator.cpp
    src/EphemerisEngineQueries.cpp
    src/EphemerisRequestFactory.cpp
    src/NightConditionsCalculator.cpp
    src/ObservationEventCalculator.cpp
    src/catalog/bundled/BundledCatalogParser.cpp
    src/catalog/io/CsvRowTokenizer.cpp
    src/catalog/hyg/HygCatalogParser.cpp
    src/catalog/hyg/HygGzipCatalogParser.cpp
    src/catalog/model/InMemoryStarCatalog.cpp
    src/catalog/opengc/OpenNgcCatalogParser.cpp
    src/catalog/io/zip/ZipCodec.cpp
    src/BodyTrailCalculator.cpp
)

target_include_directories(skygate-ephemeris
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(skygate-ephemeris
    PUBLIC
        skygate::core
    PRIVATE
        Qt6::Core
        ZLIB::ZLIB
)

if(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    target_link_libraries(skygate-ephemeris
        PRIVATE
            skygate::ephemeris-highprecision-dependencies
    )
endif()

target_compile_features(skygate-ephemeris PUBLIC cxx_std_20)
target_compile_definitions(skygate-ephemeris
    PRIVATE
        $<$<BOOL:${SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS}>:SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS=1>
)

add_library(skygate::ephemeris ALIAS skygate-ephemeris)
