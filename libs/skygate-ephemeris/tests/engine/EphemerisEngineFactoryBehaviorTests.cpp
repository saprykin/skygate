#include "EphemerisEngineFactory.hpp"

#include "engine/highprecision/CalcephKernelProvider.hpp"

#include "engine/highprecision/EarthOrientationProvider.hpp"
#include "engine/highprecision/EphemerisDataManifest.hpp"
#include "engine/highprecision/EphemerisDataSnapshot.hpp"
#include "engine/highprecision/TimeScaleService.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>

#include <QtTest/QtTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] skygate::ephemeris::CelestialBody makeFactoryBehaviorBody()
{
    return {
        .id = "factory-behavior-target",
        .displayName = "Factory Behavior Target",
        .type = skygate::ephemeris::CelestialBodyType::Star,
        .fixedEquatorial = skygate::core::EquatorialCoordinate{
            .rightAscensionHours = 5.25,
            .declinationDeg = -12.75,
        },
    };
}

[[nodiscard]] skygate::ephemeris::CelestialBody makeFactoryBehaviorSun()
{
    return {
        .id = "sun",
        .displayName = "Sun",
        .type = skygate::ephemeris::CelestialBodyType::Sun,
        .ephemerisSource = skygate::ephemeris::CelestialBodyEphemerisSource::Sun,
    };
}

[[nodiscard]] skygate::ephemeris::CelestialBody makeFactoryBehaviorMars()
{
    return {
        .id = "mars",
        .displayName = "Mars",
        .type = skygate::ephemeris::CelestialBodyType::Planet,
        .ephemerisSource = skygate::ephemeris::CelestialBodyEphemerisSource::Planet,
    };
}

[[nodiscard]] skygate::core::SkyContext makeContext()
{
    skygate::core::SkyContext context;
    context.utcTime = skygate::core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

template <typename Opaque>
[[nodiscard]] std::shared_ptr<const Opaque> makeOpaqueHandle(const std::shared_ptr<int>& owner) noexcept
{
    return {owner, reinterpret_cast<const Opaque*>(owner.get())};
}

class TestStarCatalog final : public skygate::ephemeris::IStarCatalog {
public:
    explicit TestStarCatalog(std::vector<skygate::ephemeris::CelestialBody> bodies) : m_bodies(std::move(bodies)) {}

    [[nodiscard]] std::span<const skygate::ephemeris::CelestialBody> bodies() const override
    {
        return m_bodies;
    }

private:
    std::vector<skygate::ephemeris::CelestialBody> m_bodies;
};

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
    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convert(
        const skygate::ephemeris::AstronomicalEpoch& epoch, const skygate::ephemeris::TimeScale targetScale
    ) const override
    {
        static_cast<void>(epoch);
        ++m_convertCallCount;
        skygate::ephemeris::TimeScaleConversionResult result;
        result.epoch = {
            .julianDatePart1 = 2'400'000.5,
            .julianDatePart2 = 53'736.0,
            .timeScale = targetScale,
        };
        result.status = skygate::ephemeris::TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] skygate::ephemeris::TimeScaleConversionResult convertCivilDateTime(
        const skygate::ephemeris::CivilDateTime& dateTime, const skygate::ephemeris::TimeScale targetScale
    ) const override
    {
        const auto epoch = skygate::ephemeris::astronomicalEpochFromCivilDateTime(dateTime);
        if (epoch.has_value()) {
            return convert(*epoch, targetScale);
        }

        skygate::ephemeris::TimeScaleConversionResult result;
        result.status = skygate::ephemeris::TimeScaleConversionStatus::Failed;
        result.addWarning(skygate::ephemeris::TimeScaleConversionWarningCode::InvalidInput);
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
        const skygate::ephemeris::AstronomicalEpoch& effectiveUtcEpoch = skygate::ephemeris::AstronomicalEpoch{
            .julianDatePart1 = 2'400'000.5,
            .julianDatePart2 = 53'736.0,
            .timeScale = skygate::ephemeris::TimeScale::Utc,
        }
    )
    {
        m_dataInfo.status = skygate::ephemeris::EarthOrientationDataStatus::Available;
        m_dataInfo.provenance = "factory behavior EOP";
        m_entries.push_back(
            skygate::ephemeris::EarthOrientationTableEntry{
                .effectiveUtcDate =
                    skygate::ephemeris::CivilDateTime{
                        .astronomicalYear = 2023,
                        .month = 2,
                        .day = 25,
                        .timeScale = skygate::ephemeris::TimeScale::Utc,
                    },
                .effectiveUtcEpoch = effectiveUtcEpoch,
                .ut1MinusUtcSeconds = 0.0,
                .polarMotionXArcseconds = 0.0,
                .polarMotionYArcseconds = 0.0,
            }
        );
    }

    [[nodiscard]] const skygate::ephemeris::EarthOrientationDataInfo& dataInfo() const noexcept override
    {
        ++m_dataInfoCallCount;
        return m_dataInfo;
    }

    [[nodiscard]] std::span<const skygate::ephemeris::EarthOrientationTableEntry> entries() const noexcept override
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
    skygate::ephemeris::EarthOrientationDataInfo m_dataInfo;
    std::vector<skygate::ephemeris::EarthOrientationTableEntry> m_entries;
    mutable std::size_t m_dataInfoCallCount = 0U;
    mutable std::size_t m_entriesCallCount = 0U;
};

class RecordingEphemerisDiagnosticsSink final : public skygate::ephemeris::IEphemerisDiagnosticsSink {
public:
    void
    recordFactoryCreationDiagnostic(const skygate::ephemeris::EphemerisFactoryCreationDiagnostic& diagnostic) override
    {
        m_diagnostics.push_back(diagnostic);
    }

    [[nodiscard]] const std::vector<skygate::ephemeris::EphemerisFactoryCreationDiagnostic>&
    diagnostics() const noexcept
    {
        return m_diagnostics;
    }

private:
    std::vector<skygate::ephemeris::EphemerisFactoryCreationDiagnostic> m_diagnostics;
};

class TestCalcephKernelHandle final : public skygate::ephemeris::highprecision::ICalcephKernelHandle {
public:
    [[nodiscard]] std::optional<skygate::ephemeris::highprecision::SolarSystemKernelVector>
    computeGeometricState(const skygate::ephemeris::AstronomicalEpoch&, int, int) const override
    {
        return skygate::ephemeris::highprecision::SolarSystemKernelVector{.xAu = 1.0, .yAu = 0.0, .zAu = 0.0};
    }
};

class TestCalcephKernelRuntime final : public skygate::ephemeris::highprecision::ICalcephKernelRuntime {
public:
    [[nodiscard]] bool isAvailable() const noexcept override
    {
        return true;
    }

    [[nodiscard]] skygate::ephemeris::highprecision::CalcephKernelOpenResult
    openKernel(const std::filesystem::path& path) const override
    {
        m_openedPath = path.generic_string();
        return {.handle = std::make_unique<TestCalcephKernelHandle>()};
    }

    [[nodiscard]] std::string openedPath() const
    {
        return m_openedPath;
    }

private:
    mutable std::string m_openedPath;
};

class RecordingCalcephKernelHandle final : public skygate::ephemeris::highprecision::ICalcephKernelHandle {
public:
    using CallLog = std::vector<std::pair<int, int>>;

    explicit RecordingCalcephKernelHandle(std::shared_ptr<CallLog> calls) : m_calls(std::move(calls)) {}

    [[nodiscard]] std::optional<skygate::ephemeris::highprecision::SolarSystemKernelVector> computeGeometricState(
        const skygate::ephemeris::AstronomicalEpoch&, const int targetNaifId, const int centerNaifId
    ) const override
    {
        m_calls->emplace_back(targetNaifId, centerNaifId);
        return skygate::ephemeris::highprecision::SolarSystemKernelVector{.xAu = 1.0, .yAu = 0.0, .zAu = 0.0};
    }

private:
    std::shared_ptr<CallLog> m_calls;
};

class RecordingCalcephKernelRuntime final : public skygate::ephemeris::highprecision::ICalcephKernelRuntime {
public:
    using CallLog = RecordingCalcephKernelHandle::CallLog;

    [[nodiscard]] bool isAvailable() const noexcept override
    {
        return true;
    }

    [[nodiscard]] skygate::ephemeris::highprecision::CalcephKernelOpenResult
    openKernel(const std::filesystem::path&) const override
    {
        return {.handle = std::make_unique<RecordingCalcephKernelHandle>(m_calls)};
    }

    [[nodiscard]] const CallLog& calls() const noexcept
    {
        return *m_calls;
    }

private:
    std::shared_ptr<CallLog> m_calls = std::make_shared<CallLog>();
};

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
        .start =
            {.julianDatePart1 = 2'400'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb},
        .end = {
            .julianDatePart1 = 2'500'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb
        },
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
        skygate::ephemeris::EphemerisDataManifestProfile{
            .id = "modern",
            .displayName = "Modern",
            .bundled = true,
            .longRange = false,
            .assetIds = {"kernel"},
        }
    );
    manifest.assets.push_back(
        skygate::ephemeris::EphemerisDataManifestAsset{
            .id = "kernel",
            .kind = skygate::ephemeris::EphemerisDataManifestAssetKind::SolarSystemKernel,
            .profileId = "modern",
            .version = "de440s-test",
            .sourceUrl = "https://example.test/de440s.bsp",
            .relativePath = "de440s.bsp",
            .checksum =
                skygate::ephemeris::EphemerisDataManifestChecksum{
                    .algorithm = "sha256",
                    .value = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex().toStdString(),
                },
            .compression =
                skygate::ephemeris::EphemerisDataManifestCompression{
                    .kind = skygate::ephemeris::EphemerisDataManifestCompressionKind::None,
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
    void requestCarriesCatalogOptionsAndOpaqueInputs();
    void resultHelpersDistinguishSuccessFallbackFailureAndDiagnostics();
    void compatibilityOverloadsCreateSimpleEngines();
    void simpleRequestCreatesRequestedEngineWithOptions();
    void highPrecisionRequestConstructsEngineWhenDependenciesAreAvailable();
    void highPrecisionRequestOpensActiveKernelWithoutRehashingIt();
    void highPrecisionDE441RequestUsesPlanetaryBarycentersIntentionally();
    void highPrecisionRequestWiresApparentTopocentricCorrectionPath();
    void highPrecisionRequestFallsBackOnlyWhenAllowed();
    void invalidEngineKindReturnsStructuredInvalidRequest();
};

void EphemerisEngineFactoryBehaviorTests::requestCarriesCatalogOptionsAndOpaqueInputs()
{
    const std::array bodies{makeFactoryBehaviorBody()};
    const auto opaqueOwner = std::make_shared<int>(42);

    skygate::ephemeris::EphemerisDataSetInfo manifest;
    manifest.id = "test-manifest";
    manifest.displayName = "Test Manifest";

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric;
    request.dataSetManifest = &manifest;
    request.activeDataSnapshot = makeOpaqueHandle<skygate::ephemeris::IEphemerisDataSnapshot>(opaqueOwner);
    request.timeScaleService = makeOpaqueHandle<skygate::ephemeris::ITimeScaleService>(opaqueOwner);
    request.earthOrientationProvider = makeOpaqueHandle<skygate::ephemeris::IEarthOrientationProvider>(opaqueOwner);
    request.diagnosticsSink = reinterpret_cast<skygate::ephemeris::IEphemerisDiagnosticsSink*>(opaqueOwner.get());
    request.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;

    QCOMPARE(
        static_cast<std::uint8_t>(request.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(request.catalogBodies.size(), std::size_t{1});
    QVERIFY(request.catalogBodies.front().id == std::string{"factory-behavior-target"});
    QCOMPARE(
        static_cast<std::uint32_t>(request.options.correctionFlags),
        static_cast<std::uint32_t>(skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric)
    );
    QVERIFY(request.dataSetManifest == &manifest);
    QVERIFY(request.activeDataSnapshot != nullptr);
    QVERIFY(request.timeScaleService != nullptr);
    QVERIFY(request.earthOrientationProvider != nullptr);
    QVERIFY(request.diagnosticsSink != nullptr);
    QVERIFY(skygate::ephemeris::allowsSimpleEngineFallback(request.fallbackPolicy));
}

void EphemerisEngineFactoryBehaviorTests::resultHelpersDistinguishSuccessFallbackFailureAndDiagnostics()
{
    skygate::ephemeris::EphemerisFactoryCreationDiagnostic warning{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::HighPrecisionUnavailable,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Warning,
    };
    skygate::ephemeris::EphemerisFactoryCreationDiagnostic error{
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::InvalidRequest,
        skygate::ephemeris::EphemerisFactoryCreationDiagnosticSeverity::Error,
        "invalid engine kind",
    };

    auto success =
        skygate::ephemeris::EphemerisEngineFactoryResult::success(skygate::ephemeris::createEphemerisEngine());
    QVERIFY(success.isSuccess());
    QVERIFY(!success.isFailure());
    QVERIFY(!success.usedSimpleEngineFallback());
    QVERIFY(!success.hasDiagnostics());
    QVERIFY(!success.hasErrors());

    auto fallback = skygate::ephemeris::EphemerisEngineFactoryResult::success(
        skygate::ephemeris::createEphemerisEngine(),
        skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback,
        {warning}
    );
    QVERIFY(fallback.isSuccess());
    QVERIFY(fallback.usedSimpleEngineFallback());
    QVERIFY(fallback.hasDiagnostics());
    QVERIFY(!fallback.hasErrors());
    QVERIFY(!fallback.diagnostics.front().displayText().empty());

    auto failure = skygate::ephemeris::EphemerisEngineFactoryResult::failure(
        skygate::ephemeris::EphemerisFactoryCreationStatus::FailedInvalidRequest, {error}
    );
    QVERIFY(!failure.isSuccess());
    QVERIFY(failure.isFailure());
    QVERIFY(!failure.usedSimpleEngineFallback());
    QVERIFY(failure.hasDiagnostics());
    QVERIFY(failure.hasErrors());
    QVERIFY(failure.diagnostics.front().displayText() == std::string_view{"invalid engine kind"});
}

void EphemerisEngineFactoryBehaviorTests::compatibilityOverloadsCreateSimpleEngines()
{
    const auto context = makeContext();
    const std::array bodies{makeFactoryBehaviorBody()};

    const auto emptyEngine = skygate::ephemeris::createEphemerisEngine();
    QVERIFY(emptyEngine != nullptr);
    QCOMPARE(
        static_cast<std::uint8_t>(emptyEngine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QVERIFY(emptyEngine->compute(context).states.empty());

    const auto initializerEngine = skygate::ephemeris::createEphemerisEngine({makeFactoryBehaviorBody()});
    QVERIFY(initializerEngine != nullptr);
    const auto initializerState = initializerEngine->computeBodyState(context, "factory-behavior-target");
    QVERIFY(initializerState.has_value());
    QCOMPARE(initializerState->bodyIndex, 0U);

    const auto spanEngine = skygate::ephemeris::createEphemerisEngine(
        std::span<const skygate::ephemeris::CelestialBody>{bodies.data(), bodies.size()}
    );
    QVERIFY(spanEngine != nullptr);
    const auto spanState = spanEngine->computeBodyState(context, std::size_t{0});
    QVERIFY(spanState.has_value());
    QCOMPARE(spanState->equatorial.rightAscensionHours, 5.25);
    QCOMPARE(spanState->equatorial.declinationDeg, -12.75);

    const TestStarCatalog catalog({makeFactoryBehaviorBody()});
    const auto catalogEngine = skygate::ephemeris::createEphemerisEngine(catalog);
    QVERIFY(catalogEngine != nullptr);
    const auto catalogState = catalogEngine->computeBodyState(context, "factory-behavior-target");
    QVERIFY(catalogState.has_value());
    QCOMPARE(catalogState->equatorial.rightAscensionHours, spanState->equatorial.rightAscensionHours);
    QCOMPARE(catalogState->equatorial.declinationDeg, spanState->equatorial.declinationDeg);
}

void EphemerisEngineFactoryBehaviorTests::simpleRequestCreatesRequestedEngineWithOptions()
{
    const std::array bodies{makeFactoryBehaviorBody()};

    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::Simple;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::LightTime
                                      | skygate::ephemeris::EphemerisCorrectionFlags::StellarAberration;

    auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(!result.hasDiagnostics());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedRequestedEngine)
    );

    const auto options = result.engine->options();
    QCOMPARE(
        static_cast<std::uint8_t>(options.engineKind),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::Simple)
    );
    QCOMPARE(
        static_cast<std::uint32_t>(options.correctionFlags), static_cast<std::uint32_t>(request.options.correctionFlags)
    );
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestConstructsEngineWhenDependenciesAreAvailable()
{
    const QByteArray kernelPayload("fake kernel payload");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::string kernelPath = writeKernelFixture(directory, kernelPayload);
    QVERIFY(!kernelPath.empty());

    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Geometric;
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
    auto runtime = std::make_shared<TestCalcephKernelRuntime>();
    request.calcephKernelRuntime = runtime;

    auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(!result.hasDiagnostics());
    QCOMPARE(
        static_cast<std::uint8_t>(result.engine->kind()),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineKind::HighPrecision)
    );
    QCOMPARE(result.engine->dataSetInfo().id, std::string{"factory-fixture"});
    QCOMPARE(result.engine->dataSetInfo().provenance, std::string{"factory test"});
    QCOMPARE(result.engine->supportedDateRanges().size(), std::size_t{1});
    QVERIFY(result.engine->capabilities().supportsSolarSystemBodies);
    QVERIFY(result.engine->capabilities().supportsTopocentricPositions);
    QCOMPARE(runtime->openedPath(), kernelPath);

    skygate::ephemeris::EphemerisRequest computeRequest;
    computeRequest.context = makeContext();
    computeRequest.epoch = {
        .julianDatePart1 = 2'460'000.5,
        .julianDatePart2 = 0.0,
        .timeScale = skygate::ephemeris::TimeScale::Tdb,
    };
    computeRequest.options = request.options;

    const auto state = result.engine->computeBodyState(computeRequest, "sun");
    QVERIFY(state.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisResultStatus::Valid)
    );
    QCOMPARE(state->equatorial.rightAscensionHours, 0.0);
    QCOMPARE(state->equatorial.declinationDeg, 0.0);
    QCOMPARE(state->metadata.dataSourceProvenance, std::string{"snapshot provenance"});
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestOpensActiveKernelWithoutRehashingIt()
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
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Geometric;
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
    auto runtime = std::make_shared<TestCalcephKernelRuntime>();
    request.calcephKernelRuntime = runtime;

    const auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    QVERIFY(!result.hasDiagnostics());
    QCOMPARE(runtime->openedPath(), kernelPath);
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionDE441RequestUsesPlanetaryBarycentersIntentionally()
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
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Geometric;
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
    auto runtime = std::make_shared<RecordingCalcephKernelRuntime>();
    request.calcephKernelRuntime = runtime;

    auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);
    const skygate::ephemeris::EphemerisRequest computeRequest{
        .epoch =
            {.julianDatePart1 = 2'460'000.5, .julianDatePart2 = 0.0, .timeScale = skygate::ephemeris::TimeScale::Tdb},
        .context = makeContext(),
        .options = request.options,
    };

    const auto state = result.engine->computeBodyState(computeRequest, "mars");

    QVERIFY(state.has_value());
    QVERIFY(!runtime->calls().empty());
    QCOMPARE(runtime->calls().front().first, 4);
    QCOMPARE(runtime->calls().front().second, 399);
    QCOMPARE(
        static_cast<std::uint8_t>(state->metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisResultStatus::Valid)
    );
    QVERIFY(!state->metadata.hasWarning(skygate::ephemeris::EphemerisWarningCode::BarycenterFallback));
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestWiresApparentTopocentricCorrectionPath()
{
    const QByteArray kernelPayload("fake kernel payload");
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const std::string kernelPath = writeKernelFixture(directory, kernelPayload);
    QVERIFY(!kernelPath.empty());

    const std::array bodies{makeFactoryBehaviorSun()};
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.catalogBodies = bodies;
    request.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::ApparentTopocentric;
    request.options.enableAtmosphericRefraction = false;
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
    request.calcephKernelRuntime = std::make_shared<TestCalcephKernelRuntime>();

    auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(result.isSuccess());
    QVERIFY(result.engine != nullptr);

    skygate::ephemeris::EphemerisRequest computeRequest;
    computeRequest.context = makeContext();
    computeRequest.epoch = {
        .julianDatePart1 = 2'460'000.5,
        .julianDatePart2 = 0.0,
        .timeScale = skygate::ephemeris::TimeScale::Tdb,
    };
    computeRequest.options = request.options;

    const auto state = result.engine->computeBodyState(computeRequest, "sun");

    QVERIFY(state.has_value());
    QVERIFY(timeScaleService->convertCallCount() > std::size_t{0});
    QVERIFY(earthOrientationProvider->dataInfoCallCount() > std::size_t{0});
    QVERIFY(earthOrientationProvider->entriesCallCount() > std::size_t{0});
}

void EphemerisEngineFactoryBehaviorTests::highPrecisionRequestFallsBackOnlyWhenAllowed()
{
    const std::array bodies{makeFactoryBehaviorBody()};
    RecordingEphemerisDiagnosticsSink fallbackDiagnosticsSink;

    skygate::ephemeris::EphemerisEngineFactoryRequest fallbackRequest;
    fallbackRequest.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    fallbackRequest.catalogBodies = bodies;
    fallbackRequest.options.engineKind = skygate::ephemeris::EphemerisEngineKind::HighPrecision;
    fallbackRequest.options.correctionFlags = skygate::ephemeris::EphemerisCorrectionFlags::Apparent;
    fallbackRequest.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::AllowSimpleEngineFallback;
    fallbackRequest.diagnosticsSink = &fallbackDiagnosticsSink;

    auto fallback = skygate::ephemeris::createEphemerisEngine(fallbackRequest);

    QVERIFY(fallback.isSuccess());
    QVERIFY(fallback.usedSimpleEngineFallback());
    QVERIFY(fallback.engine != nullptr);
    QVERIFY(fallback.hasDiagnostics());
    QVERIFY(!fallback.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(fallback.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationStatus::CreatedSimpleFallback)
    );
    QCOMPARE(fallbackDiagnosticsSink.diagnostics().size(), fallback.diagnostics.size());
    QCOMPARE(
        static_cast<std::uint8_t>(fallbackDiagnosticsSink.diagnostics().front().severity),
        static_cast<std::uint8_t>(fallback.diagnostics.front().severity)
    );

    RecordingEphemerisDiagnosticsSink strictDiagnosticsSink;
    skygate::ephemeris::EphemerisEngineFactoryRequest strictRequest = fallbackRequest;
    strictRequest.fallbackPolicy = skygate::ephemeris::EphemerisFactoryFallbackPolicy::StrictHighPrecision;
    strictRequest.diagnosticsSink = &strictDiagnosticsSink;

    const auto strict = skygate::ephemeris::createEphemerisEngine(strictRequest);

    QVERIFY(!strict.isSuccess());
    QVERIFY(strict.isFailure());
    QVERIFY(strict.engine == nullptr);
    QVERIFY(!strict.usedSimpleEngineFallback());
    QVERIFY(strict.hasDiagnostics());
    QVERIFY(strict.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(strict.status),
        static_cast<std::uint8_t>(
            skygate::ephemeris::EphemerisFactoryCreationStatus::FailedStrictHighPrecisionUnavailable
        )
    );
    QCOMPARE(strictDiagnosticsSink.diagnostics().size(), strict.diagnostics.size());
    QCOMPARE(
        static_cast<std::uint8_t>(strictDiagnosticsSink.diagnostics().front().severity),
        static_cast<std::uint8_t>(strict.diagnostics.front().severity)
    );
    QVERIFY(strictDiagnosticsSink.diagnostics().front().isError());
}

void EphemerisEngineFactoryBehaviorTests::invalidEngineKindReturnsStructuredInvalidRequest()
{
    skygate::ephemeris::EphemerisEngineFactoryRequest request;
    request.engineKind = static_cast<skygate::ephemeris::EphemerisEngineKind>(std::uint8_t{255});

    const auto result = skygate::ephemeris::createEphemerisEngine(request);

    QVERIFY(!result.isSuccess());
    QVERIFY(result.isFailure());
    QVERIFY(result.engine == nullptr);
    QVERIFY(!result.usedSimpleEngineFallback());
    QVERIFY(result.hasDiagnostics());
    QVERIFY(result.hasErrors());
    QCOMPARE(
        static_cast<std::uint8_t>(result.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationStatus::FailedInvalidRequest)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.diagnostics.front().code),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisFactoryCreationDiagnosticCode::InvalidRequest)
    );
    QVERIFY(result.diagnostics.front().isError());
    QVERIFY(!result.diagnostics.front().displayText().empty());
}

QTEST_APPLESS_MAIN(EphemerisEngineFactoryBehaviorTests)

#include "EphemerisEngineFactoryBehaviorTests.moc"
