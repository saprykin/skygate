#include "StaticCalcephKernelProvider.hpp"
#include "TestCalcephKernel.hpp"
#include "engine/EphemerisDataManifest.hpp"
#include "engine/IEphemerisDataSnapshot.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/ITimeScaleService.hpp"
#include "factory/EphemerisEngineFactory.hpp"
#include "time/CalendarTime.hpp"
#include "engine/highprecision/CalcephKernelProvider.hpp"
#include "engine/highprecision/ICalcephKernel.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] bool
hasErrorDiagnostics(const std::vector<skygate::ephemeris::EphemerisFactoryCreationDiagnostic>& diagnostics)
{
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [](const skygate::ephemeris::EphemerisFactoryCreationDiagnostic& diagnostic) { return diagnostic.isError(); }
    );
}

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeFactoryBehaviorSun()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "sun";
    body.displayName = "Sun";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Sun;
    return body;
}

[[nodiscard]] skygate::ephemeris::OwnGalaxyCelestialBody makeFactoryBehaviorMars()
{
    skygate::ephemeris::OwnGalaxyCelestialBody body;
    body.id = "mars";
    body.displayName = "Mars";
    body.kind = skygate::ephemeris::BaseCelestialBody::Kind::Planet;
    return body;
}

template <typename BodyRange>
[[nodiscard]] std::shared_ptr<const skygate::ephemeris::CelestialBodyCatalog> makeCatalogHandle(const BodyRange& bodies)
{
    return std::make_shared<const skygate::ephemeris::CelestialBodyCatalog>(
        std::span<const skygate::ephemeris::OwnGalaxyCelestialBody>{bodies}
    );
}

[[nodiscard]] skygate::core::ObservationContext makeContext()
{
    skygate::core::ObservationContext context;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

class TestEphemerisDataSnapshot final : public skygate::ephemeris::IEphemerisDataSnapshot {
public:
    explicit TestEphemerisDataSnapshot(skygate::ephemeris::EphemerisKernelDataAsset kernelAsset)
        : m_kernelAsset(std::move(kernelAsset))
    {
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisTextDataAsset> leapSecondTableAsset() const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisKernelDataAsset>
    solarSystemKernelAsset(const std::string_view id) const override
    {
        if (id == m_kernelAsset.id) {
            return m_kernelAsset;
        }

        return std::nullopt;
    }

private:
    skygate::ephemeris::EphemerisKernelDataAsset m_kernelAsset;
};

class TestTimeScaleService final : public skygate::ephemeris::ITimeScaleService {
public:
    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult
    convert(const skygate::core::AstronomicalEpoch& epoch, const skygate::core::TimeScale targetScale) const override
    {
        static_cast<void>(epoch);
        ++m_convertCallCount;
        skygate::ephemeris::TimeScaleConversionResult result;
        result.epoch = {
            .julianDatePart1 = 2'400'000.5,
            .julianDatePart2 = 53'736.0,
            .timeScale = targetScale,
        };
        result.status = skygate::ephemeris::TimeScaleConversionStatus::Type::Valid;
        return result;
    }

    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convertCivilDateTime(
        const skygate::core::CivilDateTime& dateTime, const skygate::core::TimeScale targetScale
    ) const override
    {
        const auto epoch = skygate::core::CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
        if (epoch.has_value()) {
            return convert(*epoch, targetScale);
        }

        skygate::ephemeris::TimeScaleConversionResult result;
        result.status = skygate::ephemeris::TimeScaleConversionStatus::Type::Failed;
        result.addWarning(skygate::ephemeris::TimeScaleConversionWarningCode::Type::InvalidInput);
        return result;
    }

    [[nodiscard]] std::size_t convertCallCount() const noexcept
    {
        return m_convertCallCount;
    }

private:
    mutable std::size_t m_convertCallCount = 0U;
};

class TestEarthOrientationProvider final : public skygate::ephemeris::IEarthOrientationProvider {
public:
    explicit TestEarthOrientationProvider(
        const skygate::core::AstronomicalEpoch& effectiveUtcEpoch = skygate::core::AstronomicalEpoch{
            .julianDatePart1 = 2'400'000.5,
            .julianDatePart2 = 53'736.0,
            .timeScale = skygate::core::TimeScale::Utc,
        }
    )
    {
        m_dataInfo.status = skygate::ephemeris::IEarthOrientationProvider::DataStatus::Available;
        m_dataInfo.provenance = "factory behavior EOP";
        m_entries.push_back(
            skygate::ephemeris::IEarthOrientationProvider::TableEntry{
                .effectiveUtcEpoch = effectiveUtcEpoch,
                .ut1MinusUtcSeconds = 0.0,
                .polarMotionXArcseconds = 0.0,
                .polarMotionYArcseconds = 0.0,
            }
        );
    }

    [[nodiscard]] const skygate::ephemeris::IEarthOrientationProvider::DataInfo& dataInfo() const noexcept override
    {
        ++m_dataInfoCallCount;
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const skygate::ephemeris::IEarthOrientationProvider::TableEntry>
    entries() const noexcept override
    {
        ++m_entriesCallCount;
        return m_entries;
    }

    [[nodiscard]] std::size_t dataInfoCallCount() const noexcept
    {
        return m_dataInfoCallCount;
    }

    [[nodiscard]] std::size_t entriesCallCount() const noexcept
    {
        return m_entriesCallCount;
    }

private:
    skygate::ephemeris::IEarthOrientationProvider::DataInfo m_dataInfo;
    std::vector<skygate::ephemeris::IEarthOrientationProvider::TableEntry> m_entries;
    mutable std::size_t m_dataInfoCallCount = 0U;
    mutable std::size_t m_entriesCallCount = 0U;
};

[[nodiscard]] skygate::ephemeris::EphemerisDateRange makeKernelRange();

[[nodiscard]] skygate::ephemeris::highprecision::ICalcephKernel::Info
makeKernelInfo(std::string id = "kernel", std::string profileId = "modern", std::string version = "snapshot-version")
{
    return skygate::ephemeris::highprecision::ICalcephKernel::Info{
        .id = std::move(id),
        .profileId = std::move(profileId),
        .version = std::move(version),
        .provenance = "snapshot provenance",
        .validityRange = makeKernelRange(),
    };
}

[[nodiscard]] skygate::ephemeris::highprecision::ICalcephKernel::StateResult
makeKernelResult(const skygate::ephemeris::highprecision::ICalcephKernel::Info& info)
{
    skygate::ephemeris::highprecision::ICalcephKernel::StateResult result;
    result.positionAu = skygate::core::Vector3d{.x = 1.0, .y = 0.0, .z = 0.0};
    result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
    result.metadata.dataSourceProvenance = info.provenance;
    result.metadata.effectiveDataValidityRange = info.validityRange;
    return result;
}

[[nodiscard]] std::shared_ptr<skygate::ephemeris::tests::TestCalcephKernel>
makeReadyKernel(skygate::ephemeris::highprecision::ICalcephKernel::Info info = makeKernelInfo())
{
    auto kernel = std::make_shared<skygate::ephemeris::tests::TestCalcephKernel>(info);
    kernel->setDefaultResult(makeKernelResult(info));
    return kernel;
}

[[nodiscard]] std::string writeKernelFixture(QTemporaryDir& directory, const QByteArray& payload)
{
    const QString path = directory.filePath(QStringLiteral("de440s.bsp"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (file.write(payload) != payload.size()) {
        return {};
    }

    return path.toStdString();
}

[[nodiscard]] skygate::ephemeris::EphemerisDateRange makeKernelRange()
{
    return {
        .id = "modern",
        .displayName = "Modern",
        .start = {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
        .end = {.julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
    };
}

[[nodiscard]] skygate::ephemeris::EphemerisDataManifest
makeFactoryManifest(const QByteArray& payload, const std::uint64_t payloadSize)
{
    skygate::ephemeris::EphemerisDataManifest manifest;
    manifest.dataSetInfo.id = "factory-fixture";
    manifest.dataSetInfo.displayName = "Factory Fixture";
    manifest.dataSetInfo.version = "test";
    manifest.dataSetInfo.provenance = "factory test";
    manifest.profiles.push_back(
        skygate::ephemeris::EphemerisDataManifest::Profile{
            .id = "modern",
            .displayName = "Modern",
            .bundled = true,
            .longRange = false,
            .assetIds = {"kernel"},
        }
    );
    manifest.assets.push_back(
        skygate::ephemeris::EphemerisDataManifest::Asset{
            .id = "kernel",
            .kind = skygate::ephemeris::EphemerisDataManifest::AssetKind::SolarSystemKernel,
            .profileId = "modern",
            .version = "de440s-test",
            .sourceUrl = "https://example.test/de440s.bsp",
            .relativePath = "de440s.bsp",
            .checksum =
                skygate::ephemeris::EphemerisDataManifest::Checksum{
                    .algorithm = "sha256",
                    .value = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex().toStdString(),
                },
            .compression =
                skygate::ephemeris::EphemerisDataManifest::Compression{
                    .kind = skygate::ephemeris::EphemerisDataManifest::CompressionKind::None,
                    .uncompressedSizeBytes = payloadSize,
                },
            .validityRange = makeKernelRange(),
        }
    );
    return manifest;
}

}  // namespace

class EphemerisEngineFactoryBehaviorTests final : public QObject {
    Q_OBJECT

private slots:
    void highPrecisionRequestBuildsEngineWithDependencies();
    void highPrecisionRequestUsesOnlyDatasetManifest();
    void highPrecisionRequestRequiresDatasetInfoSource();
    void highPrecisionRequestHandlesNullKernelProvider();
    void highPrecisionRequestSkipsKernelRehash();
    void highPrecisionRequestVerifiesChecksum();
    void highPrecisionDE441AvoidsBarycenterFallback();
    void highPrecisionRequestWiresTopocentricPath();
    void defaultProviderReportsUnavailableBuildFirst();
};

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestBuildsEngineWithDependencies()
{
    const QByteArray kernelPayload("fake kernel payload");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::string kernelPath = writeKernelFixture(directory, kernelPayload);
    QVERIFY(!kernelPath.empty());

    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    const skygate::ephemeris::EphemerisDataManifest manifest =
        makeFactoryManifest(kernelPayload, static_cast<std::uint64_t>(kernelPayload.size()));
    request.dataManifest = &manifest;
    request.activeDataSnapshot =
        std::make_shared<TestEphemerisDataSnapshot>(skygate::ephemeris::EphemerisKernelDataAsset{
            .id = "kernel",
            .profileId = "modern",
            .version = "snapshot-version",
            .provenance = "snapshot provenance",
            .activePath = kernelPath,
        });
    request.timeScaleService = std::make_shared<TestTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<TestEarthOrientationProvider>();
    auto kernelProvider = std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>(makeReadyKernel());
    request.calcephKernelProvider = kernelProvider;

    auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(result.status));
    QVERIFY(result.engine != nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(result.diagnostics.empty());
    QCOMPARE(
        static_cast<std::uint8_t>(result.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision)
    );
    QCOMPARE(result.engine->dataSetInfo().id, std::string{"factory-fixture"});
    QCOMPARE(result.engine->dataSetInfo().provenance, std::string{"factory test"});
    QCOMPARE(result.engine->supportedDateRanges().size(), std::size_t{1});
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            result.engine->capabilities(), skygate::ephemeris::EphemerisCapabilities::solarSystemBodies()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCapabilities::has(
            result.engine->capabilities(), skygate::ephemeris::EphemerisCapabilities::topocentricPositions()
        )
    );
    QCOMPARE(kernelProvider->openCount(), 1);

    skygate::ephemeris::EphemerisRequest computeRequest;
    computeRequest.context = makeContext();
    computeRequest.epoch = {
        .julianDatePart1 = 2'460'000.5,
        .julianDatePart2 = 0.0,
        .timeScale = skygate::core::TimeScale::Tdb,
    };
    computeRequest.options = request.options;

    const auto state = result.engine->computeBodyState(computeRequest, "sun");
    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QCOMPARE(state->equatorial.rightAscensionHours, 0.0);
    QCOMPARE(state->equatorial.declinationDeg, 0.0);
    QCOMPARE(state->metadata.dataSourceProvenance, std::string{"snapshot provenance"});
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestUsesOnlyDatasetManifest()
{
    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisDatasetInfo datasetManifest;
    datasetManifest.id = "injected-dataset";
    datasetManifest.displayName = "Injected dataset";
    datasetManifest.provenance = "injected dataset provenance";

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    request.datasetManifest = &datasetManifest;
    request.timeScaleService = std::make_shared<TestTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<TestEarthOrientationProvider>();
    request.calcephKernelProvider =
        std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>(makeReadyKernel());

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(result.status));
    QVERIFY(result.engine != nullptr);
    QVERIFY(result.diagnostics.empty());
    QCOMPARE(result.engine->dataSetInfo().id, std::string{"injected-dataset"});
    QCOMPARE(result.engine->dataSetInfo().provenance, std::string{"injected dataset provenance"});
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestRequiresDatasetInfoSource()
{
    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    request.options.setFallbackToSimpleEngine(false);
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::Type::StrictHighPrecision;
    request.timeScaleService = std::make_shared<TestTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<TestEarthOrientationProvider>();
    request.calcephKernelProvider =
        std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>(makeReadyKernel());

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(!skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(result.status));
    QVERIFY(hasErrorDiagnostics(result.diagnostics));
    QVERIFY(!result.diagnostics.empty());
    QCOMPARE(
        static_cast<std::uint8_t>(result.diagnostics.front().code),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::RequiredEphemerisDataUnavailable
        )
    );
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestHandlesNullKernelProvider()
{
    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisDatasetInfo datasetManifest;
    datasetManifest.id = "injected-dataset";

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    request.options.setFallbackToSimpleEngine(false);
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::Type::StrictHighPrecision;
    request.datasetManifest = &datasetManifest;
    request.timeScaleService = std::make_shared<TestTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<TestEarthOrientationProvider>();
    request.calcephKernelProvider = std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>();

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(!skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(result.status));
    QVERIFY(hasErrorDiagnostics(result.diagnostics));
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationStatus::Type::FailedCreationError)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.diagnostics.front().code),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::EngineCreationFailed)
    );
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestSkipsKernelRehash()
{
    const QByteArray activatedKernelPayload("active kernel bytes");
    const QByteArray manifestChecksumPayload("other kernel bytes ");
    QCOMPARE(activatedKernelPayload.size(), manifestChecksumPayload.size());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::string kernelPath = writeKernelFixture(directory, activatedKernelPayload);
    QVERIFY(!kernelPath.empty());

    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    const skygate::ephemeris::EphemerisDataManifest manifest =
        makeFactoryManifest(manifestChecksumPayload, static_cast<std::uint64_t>(activatedKernelPayload.size()));
    request.dataManifest = &manifest;
    request.activeDataSnapshot =
        std::make_shared<TestEphemerisDataSnapshot>(skygate::ephemeris::EphemerisKernelDataAsset{
            .id = "kernel",
            .profileId = "modern",
            .version = "snapshot-version",
            .provenance = "snapshot provenance",
            .activePath = kernelPath,
        });
    request.timeScaleService = std::make_shared<TestTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<TestEarthOrientationProvider>();
    auto kernelProvider = std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>(makeReadyKernel());
    request.calcephKernelProvider = kernelProvider;

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(result.status));
    QVERIFY(result.engine != nullptr);
    QVERIFY(result.diagnostics.empty());
    QCOMPARE(kernelProvider->openCount(), 1);
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestVerifiesChecksum()
{
    const QByteArray activatedKernelPayload("active kernel bytes");
    const QByteArray manifestChecksumPayload("other kernel bytes ");
    QCOMPARE(activatedKernelPayload.size(), manifestChecksumPayload.size());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::string kernelPath = writeKernelFixture(directory, activatedKernelPayload);
    QVERIFY(!kernelPath.empty());

    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    request.options.setFallbackToSimpleEngine(false);
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::Type::StrictHighPrecision;
    const skygate::ephemeris::EphemerisDataManifest manifest =
        makeFactoryManifest(manifestChecksumPayload, static_cast<std::uint64_t>(activatedKernelPayload.size()));
    request.dataManifest = &manifest;
    request.activeDataSnapshot =
        std::make_shared<TestEphemerisDataSnapshot>(skygate::ephemeris::EphemerisKernelDataAsset{
            .id = "kernel",
            .profileId = "modern",
            .version = "snapshot-version",
            .provenance = "snapshot provenance",
            .activePath = kernelPath,
        });
    request.timeScaleService = std::make_shared<TestTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<TestEarthOrientationProvider>();

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(!skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(result.status));
    QVERIFY(hasErrorDiagnostics(result.diagnostics));
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    QCOMPARE(
        static_cast<std::uint8_t>(result.diagnostics.front().code),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::RequiredEphemerisDataUnavailable
        )
    );
    QVERIFY(result.diagnostics.front().displayText().find("checksum") != std::string_view::npos);
#else
    QCOMPARE(
        static_cast<std::uint8_t>(result.diagnostics.front().code),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::HighPrecisionUnavailable
        )
    );
#endif
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionDE441AvoidsBarycenterFallback()
{
    const QByteArray kernelPayload("fake de441 kernel payload");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::string kernelPath = writeKernelFixture(directory, kernelPayload);
    QVERIFY(!kernelPath.empty());

    skygate::ephemeris::EphemerisDataManifest manifest =
        makeFactoryManifest(kernelPayload, static_cast<std::uint64_t>(kernelPayload.size()));
    manifest.profiles.front().id = "de441-long-range";
    manifest.profiles.front().displayName = "DE441 long range";
    manifest.profiles.front().bundled = false;
    manifest.profiles.front().longRange = true;
    manifest.profiles.front().assetIds = {"de441-kernel"};
    manifest.assets.front().id = "de441-kernel";
    manifest.assets.front().profileId = "de441-long-range";
    manifest.assets.front().version = "DE441";
    manifest.assets.front().sourceUrl = "https://example.test/de441.bsp";

    const std::array bodies{makeFactoryBehaviorMars()};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    request.dataManifest = &manifest;
    request.activeDataSnapshot =
        std::make_shared<TestEphemerisDataSnapshot>(skygate::ephemeris::EphemerisKernelDataAsset{
            .id = "de441-kernel",
            .profileId = "de441-long-range",
            .version = "DE441",
            .provenance = "DE441 test snapshot",
            .activePath = kernelPath,
        });
    request.timeScaleService = std::make_shared<TestTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<TestEarthOrientationProvider>();
    auto kernel = makeReadyKernel(makeKernelInfo("de441-kernel", "de441-long-range", "DE441"));
    auto kernelProvider = std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>(kernel);
    request.calcephKernelProvider = kernelProvider;

    auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(result.status));
    QVERIFY(result.engine != nullptr);
    const skygate::ephemeris::EphemerisRequest computeRequest{
        .epoch = {.julianDatePart1 = 2'460'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::core::TimeScale::Tdb},
        .context = makeContext(),
        .options = request.options,
    };

    const auto state = result.engine->computeBodyState(computeRequest, "mars");

    QVERIFY(state.has_value());
    QVERIFY(!kernel->calls().empty());
    QCOMPARE(kernel->calls().front().targetNaifId, 4);
    QCOMPARE(kernel->calls().front().centerNaifId, 399);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(!state->metadata.hasWarning(skygate::ephemeris::EphemerisEngineWarning::Code::BarycenterFallback));
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestWiresTopocentricPath()
{
    const QByteArray kernelPayload("fake kernel payload");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::string kernelPath = writeKernelFixture(directory, kernelPayload);
    QVERIFY(!kernelPath.empty());

    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::apparentTopocentric());
    request.options.setEnableAtmosphericRefraction(false);
    const skygate::ephemeris::EphemerisDataManifest manifest =
        makeFactoryManifest(kernelPayload, static_cast<std::uint64_t>(kernelPayload.size()));
    request.dataManifest = &manifest;
    request.activeDataSnapshot =
        std::make_shared<TestEphemerisDataSnapshot>(skygate::ephemeris::EphemerisKernelDataAsset{
            .id = "kernel",
            .profileId = "modern",
            .version = "snapshot-version",
            .provenance = "snapshot provenance",
            .activePath = kernelPath,
        });
    auto timeScaleService = std::make_shared<TestTimeScaleService>();
    auto earthOrientationProvider = std::make_shared<TestEarthOrientationProvider>();
    request.timeScaleService = timeScaleService;
    request.earthOrientationProvider = earthOrientationProvider;
    request.calcephKernelProvider =
        std::make_shared<skygate::ephemeris::tests::StaticCalcephKernelProvider>(makeReadyKernel());

    auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(result.status));
    QVERIFY(result.engine != nullptr);

    skygate::ephemeris::EphemerisRequest computeRequest;
    computeRequest.context = makeContext();
    computeRequest.epoch = {
        .julianDatePart1 = 2'460'000.5,
        .julianDatePart2 = 0.0,
        .timeScale = skygate::core::TimeScale::Tdb,
    };
    computeRequest.options = request.options;

    const auto state = result.engine->computeBodyState(computeRequest, "sun");

    QVERIFY(state.has_value());
    QVERIFY(timeScaleService->convertCallCount() > std::size_t{0});
    QVERIFY(earthOrientationProvider->dataInfoCallCount() > std::size_t{0});
    QVERIFY(earthOrientationProvider->entriesCallCount() > std::size_t{0});
}

void EphemerisEngineFactoryBehaviorTests::defaultProviderReportsUnavailableBuildFirst()
{
    const QByteArray kernelPayload("fake kernel payload");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::string kernelPath = writeKernelFixture(directory, kernelPayload);
    QVERIFY(!kernelPath.empty());

    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision;
    request.catalog = makeCatalogHandle(bodies);
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(skygate::ephemeris::EphemerisCorrectionFlags::geometric());
    request.options.setFallbackToSimpleEngine(false);
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::Type::StrictHighPrecision;
    const skygate::ephemeris::EphemerisDataManifest manifest =
        makeFactoryManifest(kernelPayload, static_cast<std::uint64_t>(kernelPayload.size()));
    request.dataManifest = &manifest;
    request.activeDataSnapshot =
        std::make_shared<TestEphemerisDataSnapshot>(skygate::ephemeris::EphemerisKernelDataAsset{
            .id = "kernel",
            .profileId = "modern",
            .version = "snapshot-version",
            .provenance = "snapshot provenance",
            .activePath = kernelPath,
        });
    request.timeScaleService = std::make_shared<TestTimeScaleService>();
    request.earthOrientationProvider = std::make_shared<TestEarthOrientationProvider>();

    const auto result = skygate::ephemeris::EphemerisEngineFactory::create(request);

    QVERIFY(!skygate::ephemeris::EphemerisFactoryCreationStatus::isSuccess(result.status));
    QVERIFY(hasErrorDiagnostics(result.diagnostics));
#if defined(SKYGATE_ENABLE_HIGH_PRECISION_EPHEMERIS)
    QCOMPARE(
        static_cast<std::uint8_t>(result.diagnostics.front().code),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::RequiredEphemerisDataUnavailable
        )
    );
#else
    QCOMPARE(
        static_cast<std::uint8_t>(result.diagnostics.front().code),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationDiagnostic::Code::HighPrecisionUnavailable
        )
    );
#endif
}

QTEST_APPLESS_MAIN(EphemerisEngineFactoryBehaviorTests)

#include "EphemerisEngineFactoryBehaviorTests.moc"
