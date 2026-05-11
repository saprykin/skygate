find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
    message(FATAL_ERROR
        "SkyGate requires zlib for gzip and ZIP catalog import. "
        "Install vcpkg, set VCPKG_ROOT, and configure with one of the "
        "*-vcpkg presets, or provide a system zlib through CMAKE_PREFIX_PATH."
    )
endif()

add_library(skygate-ephemeris STATIC
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
    include/skygate/ephemeris/EphemerisEngineFactory.hpp
    include/skygate/ephemeris/EphemerisEngineQueries.hpp
    include/skygate/ephemeris/IEphemerisEngine.hpp
    include/skygate/ephemeris/NightConditionsCalculator.hpp
    include/skygate/ephemeris/IStarCatalog.hpp
    include/skygate/ephemeris/ObservationEventCalculator.hpp
    include/skygate/ephemeris/StellariumConstellationParser.hpp
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
    src/engine/AstronomicalTime.hpp
    src/engine/EclipticToEquatorialCalculator.hpp
    src/engine/EquatorialToHorizontalCalculator.hpp
    src/engine/MoonEquatorialCalculator.hpp
    src/engine/PlanetEquatorialCalculator.hpp
    src/engine/SunEquatorialCalculator.hpp
    src/CelestialReferenceCalculator.cpp
    src/ConstellationReferenceCalculator.cpp
    src/EphemerisEngineQueries.cpp
    src/NightConditionsCalculator.cpp
    src/ObservationEventCalculator.cpp
    src/SimpleEphemerisEngine.cpp
    src/catalog/bundled/BundledCatalogParser.cpp
    src/catalog/io/CsvRowTokenizer.cpp
    src/catalog/hyg/HygCatalogParser.cpp
    src/catalog/hyg/HygGzipCatalogParser.cpp
    src/catalog/model/InMemoryStarCatalog.cpp
    src/catalog/opengc/OpenNgcCatalogParser.cpp
    src/catalog/io/zip/ZipCodec.cpp
    src/engine/AstronomicalTime.cpp
    src/engine/EclipticToEquatorialCalculator.cpp
    src/engine/EquatorialToHorizontalCalculator.cpp
    src/engine/MoonEquatorialCalculator.cpp
    src/engine/PlanetEquatorialCalculator.cpp
    src/engine/SunEquatorialCalculator.cpp
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

target_compile_features(skygate-ephemeris PUBLIC cxx_std_20)

add_library(skygate::ephemeris ALIAS skygate-ephemeris)
