#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/ITimeScaleService.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "factory/IEphemerisDiagnosticsSink.hpp"

#include <QtTest/QtTest>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeFactoryTestBody()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "vega";
    body.displayName = "Vega";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = 18.6156,
        .declinationDeg = 38.7837,
    };
    return body;
}

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeFactoryBehaviorBody()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "factory-behavior-target";
    body.displayName = "Factory Behavior Target";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Star;
    body.fixedEquatorial = skygate::core::EquatorialCoordinate{
        .rightAscensionHours = 5.25,
        .declinationDeg = -12.75,
    };
    return body;
}

template <typename BodyRange>
[[nodiscard]] std::shared_ptr<const skygate::ephemeris::CelestialBodyCatalog> makeCatalogHandle(const BodyRange& bodies)
{
    return std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>(bodies.begin(), bodies.end())
    );
}

template <typename Opaque>
[[nodiscard]] std::shared_ptr<const Opaque> makeOpaqueHandle(const std::shared_ptr<int>& owner) noexcept
{
    return {owner, reinterpret_cast<const Opaque*>(owner.get())};
}

}  // namespace

class EphemerisEngineFactoryModelTests final : public QObject {
    Q_OBJECT

private slots:
    void constructsFactoryRequestDefaults();
    void constructsSimpleAndHighPrecisionFactoryRequests();
    void requestCarriesCatalogOptionsAndOpaqueInputs();
};

void EphemerisEngineFactoryModelTests::constructsFactoryRequestDefaults()
{
    skygate::ephemeris::EphemerisEngineFactoryRequest request;

    QCOMPARE(
        static_cast<std::uint8_t>(request.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QVERIFY(request.catalog == nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(request.options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(request.fallbackPolicy),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryFallbackPolicy::Type::AllowSimpleEngineFallback)
    );
    QVERIFY(skygate::ephemeris::EphemerisFactoryFallbackPolicy::allowsSimpleEngineFallback(request.fallbackPolicy));
    QVERIFY(request.datasetManifest == nullptr);
    QVERIFY(request.activeDataSnapshot == nullptr);
    QVERIFY(request.timeScaleService == nullptr);
    QVERIFY(request.earthOrientationProvider == nullptr);
    QVERIFY(request.diagnosticsSink == nullptr);
    QVERIFY(request.dataManifest == nullptr);
    QVERIFY(request.calcephKernelProvider == nullptr);
}

void EphemerisEngineFactoryModelTests::constructsSimpleAndHighPrecisionFactoryRequests()
{
    auto catalog = std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::vector<skygate::ephemeris::OwnGalaxyCelestialBody>{makeFactoryTestBody()}
    );

    skygate::ephemeris::EphemerisEngineFactoryRequest simpleRequest;
    simpleRequest.catalog = catalog;

    QVERIFY(simpleRequest.catalog != nullptr);
    QCOMPARE(simpleRequest.catalog->size(), std::size_t{1});
    QVERIFY(simpleRequest.catalog->bodyAt(0).id == std::string{"vega"});
    QCOMPARE(
        static_cast<std::uint8_t>(simpleRequest.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::Simple)
    );

    skygate::ephemeris::EphemerisDatasetInfo manifest;
    manifest.id = "de440s";
    manifest.displayName = "DE440s";

    skygate::ephemeris::EphemerisEngineFactoryRequest highPrecisionRequest;
    highPrecisionRequest.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    highPrecisionRequest.catalog = catalog;
    highPrecisionRequest.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    highPrecisionRequest.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparent());
    highPrecisionRequest.datasetManifest = &manifest;
    highPrecisionRequest.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::Type::StrictHighPrecision;

    QCOMPARE(
        static_cast<std::uint8_t>(highPrecisionRequest.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(highPrecisionRequest.options.engineKind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(highPrecisionRequest.options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::apparent())
    );
    QVERIFY(highPrecisionRequest.datasetManifest == &manifest);
    QVERIFY(!skygate::ephemeris::EphemerisFactoryFallbackPolicy::allowsSimpleEngineFallback(
        highPrecisionRequest.fallbackPolicy
    ));
}

void EphemerisEngineFactoryModelTests::requestCarriesCatalogOptionsAndOpaqueInputs()
{
    const std::array bodies{makeFactoryBehaviorBody()};
    const auto opaqueOwner = std::make_shared<int>(42);

    skygate::ephemeris::EphemerisDatasetInfo manifest;
    manifest.id = "test-manifest";
    manifest.displayName = "Test Manifest";

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric());
    request.datasetManifest = &manifest;
    request.activeDataSnapshot = makeOpaqueHandle<skygate::ephemeris::IEphemerisDataSnapshot>(opaqueOwner);
    request.timeScaleService = makeOpaqueHandle<skygate::ephemeris::ITimeScaleService>(opaqueOwner);
    request.earthOrientationProvider = makeOpaqueHandle<skygate::ephemeris::IEarthOrientationProvider>(opaqueOwner);
    request.diagnosticsSink = reinterpret_cast<skygate::ephemeris::IEphemerisDiagnosticsSink*>(opaqueOwner.get());
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::Type::AllowSimpleEngineFallback;

    QCOMPARE(
        static_cast<std::uint8_t>(request.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QVERIFY(request.catalog != nullptr);
    QCOMPARE(request.catalog->size(), std::size_t{1});
    QVERIFY(request.catalog->bodyAt(0).id == std::string{"factory-behavior-target"});
    QCOMPARE(
        static_cast<std::uint32_t>(request.options.correctionFlags()),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric())
    );
    QVERIFY(request.datasetManifest == &manifest);
    QVERIFY(request.activeDataSnapshot != nullptr);
    QVERIFY(request.timeScaleService != nullptr);
    QVERIFY(request.earthOrientationProvider != nullptr);
    QVERIFY(request.diagnosticsSink != nullptr);
    QVERIFY(skygate::ephemeris::EphemerisFactoryFallbackPolicy::allowsSimpleEngineFallback(request.fallbackPolicy));
}

QTEST_APPLESS_MAIN(EphemerisEngineFactoryModelTests)

#include "EphemerisEngineFactoryModelTests.moc"
