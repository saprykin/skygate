find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
find_package(ZLIB REQUIRED)

add_library(skygate-ephemeris STATIC
    include/skygate/ephemeris/CatalogLoadResult.hpp
    include/skygate/ephemeris/CatalogPayloadParser.hpp
    include/skygate/ephemeris/CelestialReferenceCalculator.hpp
    include/skygate/ephemeris/ConstellationData.hpp
    include/skygate/ephemeris/ConstellationReferenceCalculator.hpp
    include/skygate/ephemeris/EphemerisEngineFactory.hpp
    include/skygate/ephemeris/IEphemerisEngine.hpp
    include/skygate/ephemeris/IStarCatalog.hpp
    include/skygate/ephemeris/StarCatalogFactory.hpp
    include/skygate/ephemeris/StellariumConstellationParser.hpp
    include/skygate/ephemeris/Types.hpp
    src/catalog/CatalogBodyNormalization.hpp
    src/catalog/CatalogBodyParseResult.hpp
    src/catalog/BundledCatalogParser.hpp
    src/catalog/CatalogBodyNormalization.cpp
    src/catalog/CatalogPayloadParser.cpp
    src/catalog/BundledConstellationData.cpp
    src/catalog/CsvRowTokenizer.hpp
    src/catalog/GzipCodec.hpp
    src/catalog/HygCatalogParser.hpp
    src/catalog/HygGzipCatalogParser.hpp
    src/catalog/InMemoryStarCatalog.hpp
    src/catalog/OpenNgcCatalogParser.hpp
    src/catalog/ZipCodec.hpp
    src/catalog/StellariumConstellationParser.cpp
    src/engine/CoordinateTransform.hpp
    src/engine/JulianDateTime.hpp
    src/engine/KnownConstellationLookup.hpp
    src/engine/MoonEquatorialCalculator.hpp
    src/engine/PlanetEquatorialCalculator.hpp
    src/engine/StarEquatorialCalculator.hpp
    src/engine/SunEquatorialCalculator.hpp
    src/CelestialReferenceCalculator.cpp
    src/ConstellationReferenceCalculator.cpp
    src/BundledStarCatalog.cpp
    src/SimpleEphemerisEngine.cpp
    src/catalog/BundledCatalogParser.cpp
    src/catalog/CsvRowTokenizer.cpp
    src/catalog/GzipCodec.cpp
    src/catalog/HygCatalogParser.cpp
    src/catalog/HygGzipCatalogParser.cpp
    src/catalog/InMemoryStarCatalog.cpp
    src/catalog/OpenNgcCatalogParser.cpp
    src/catalog/ZipCodec.cpp
    src/engine/CoordinateTransform.cpp
    src/engine/JulianDateTime.cpp
    src/engine/KnownConstellationLookup.cpp
    src/engine/MoonEquatorialCalculator.cpp
    src/engine/PlanetEquatorialCalculator.cpp
    src/engine/StarEquatorialCalculator.cpp
    src/engine/SunEquatorialCalculator.cpp
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
