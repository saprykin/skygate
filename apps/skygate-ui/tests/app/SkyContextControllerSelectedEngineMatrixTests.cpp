#include <QtTest>

#include "SkyContextController.hpp"
#include "SkyEphemerisTestSupport.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkySceneModel.hpp"
#include "SkySceneModelTestSupport.hpp"

#include "skygate/core/ITimeSource.hpp"

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kTargetId = "matrix_target";
constexpr std::size_t kTargetIndex = 2U;
constexpr double kPi = 3.141592653589793238462643383279502884;

class MatrixTimeSource final : public skygate::core::ITimeSource {
public:
    [[nodiscard]] skygate::core::UtcTimePoint nowUtc() const noexcept override
    {
        return skygate::core::UtcTimePoint(std::chrono::seconds(1'777'777'000));
    }
};

[[nodiscard]] std::vector<skygate::ephemeris::CelestialBody> matrixBodies()
{
    return {
        skygate::ui::tests::makeBody("sun", "Sun", skygate::ephemeris::CelestialBodyType::Sun, -26.7),
        skygate::ui::tests::makeBody("moon", "Moon", skygate::ephemeris::CelestialBodyType::Moon, -12.0),
        skygate::ui::tests::makeFixedBody(
            std::string(kTargetId), "Matrix Target", skygate::ephemeris::CelestialBodyType::Star, 1.0, 4.0, 12.0
        ),
    };
}

[[nodiscard]] double secondsSinceEpoch(const skygate::core::UtcTimePoint& utcTime) noexcept
{
    return static_cast<double>(utcTime.time_since_epoch().count());
}

[[nodiscard]] double dayFraction(const skygate::core::UtcTimePoint& utcTime) noexcept
{
    constexpr double kSecondsPerDay = 86'400.0;
    double fraction = std::fmod(secondsSinceEpoch(utcTime), kSecondsPerDay) / kSecondsPerDay;
    if (fraction < 0.0) {
        fraction += 1.0;
    }
    return fraction;
}

[[nodiscard]] bool hasLightTime(const skygate::ephemeris::EphemerisEngineOptions& options) noexcept
{
    return skygate::ephemeris::hasCorrectionFlag(
        options.correctionFlags, skygate::ephemeris::EphemerisCorrectionFlags::LightTime
    );
}

[[nodiscard]] int optionTier(const skygate::ephemeris::EphemerisEngineOptions& options) noexcept
{
    if (options.engineKind == skygate::ephemeris::EphemerisEngineKind::Simple) {
        return 0;
    }
    return hasLightTime(options) ? 2 : 1;
}

[[nodiscard]] double expectedLongitudeOffset(const skygate::ephemeris::EphemerisEngineOptions& options) noexcept
{
    return 1.5 + static_cast<double>(optionTier(options)) * 4.0;
}

[[nodiscard]] bool nearlyEqual(const double lhs, const double rhs, const double tolerance = 1.0e-6) noexcept
{
    return std::abs(lhs - rhs) <= tolerance;
}

[[nodiscard]] QString formattedHorizontal(const skygate::core::HorizontalCoordinate& horizontal)
{
    return QString("%1 / %2 deg")
        .arg(QString::number(horizontal.altitudeDeg, 'f', 1), QString::number(horizontal.azimuthDeg, 'f', 1));
}

class MatrixEphemerisEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    MatrixEphemerisEngine(
        std::vector<skygate::ephemeris::CelestialBody> bodies,
        const skygate::ephemeris::EphemerisEngineKind kind,
        const skygate::ephemeris::EphemerisCorrectionFlags correctionFlags
    )
        : m_bodies(std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(std::move(bodies)))
    {
        m_options.engineKind = kind;
        m_options.correctionFlags = correctionFlags;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind kind() const noexcept override
    {
        return m_options.engineKind;
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Selected-engine matrix test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisCapabilities capabilities() const noexcept override
    {
        skygate::ephemeris::EphemerisCapabilities capabilities;
        capabilities.engineKind = m_options.engineKind;
        capabilities.supportedCorrections = m_options.correctionFlags;
        capabilities.supportsCatalogStars = true;
        capabilities.supportsTopocentricPositions = true;
        return capabilities;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        return m_options;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::ephemeris::EphemerisRequest& request
    ) const override
    {
        ++m_requestSnapshotCount;
        m_lastRequestOptions = request.options;

        skygate::ephemeris::SkySnapshot snapshot;
        snapshot.context = request.context;
        snapshot.context.observer.longitudeDeg += expectedLongitudeOffset(request.options);
        snapshot.catalogBodies = m_bodies;
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            snapshot.states.push_back(stateFor(request, bodyIndex));
        }
        return snapshot;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::string_view bodyId) const override
    {
        for (std::size_t bodyIndex = 0; bodyIndex < m_bodies->size(); ++bodyIndex) {
            if ((*m_bodies)[bodyIndex].id == bodyId) {
                return computeBodyState(request, bodyIndex);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const override
    {
        ++m_requestBodyStateCount;
        m_lastRequestOptions = request.options;
        if (bodyIndex >= m_bodies->size()) {
            return std::nullopt;
        }
        return stateFor(request, bodyIndex);
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot compute(const skygate::core::SkyContext& context) const override
    {
        ++m_contextSnapshotCount;
        skygate::ephemeris::EphemerisRequest request;
        request.context = context;
        request.options = m_options;
        return compute(request);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::string_view bodyId) const override
    {
        ++m_contextBodyStateCount;
        skygate::ephemeris::EphemerisRequest request;
        request.context = context;
        request.options = m_options;
        return computeBodyState(request, bodyId);
    }

    [[nodiscard]] std::optional<skygate::ephemeris::CelestialBodyState>
    computeBodyState(const skygate::core::SkyContext& context, const std::uint32_t bodyIndex) const override
    {
        ++m_contextBodyStateCount;
        skygate::ephemeris::EphemerisRequest request;
        request.context = context;
        request.options = m_options;
        return computeBodyState(request, static_cast<std::size_t>(bodyIndex));
    }

    [[nodiscard]] skygate::core::HorizontalCoordinate
    expectedTargetHorizontal(const skygate::ephemeris::EphemerisRequest& request) const noexcept
    {
        return horizontalFor(request, kTargetIndex);
    }

    [[nodiscard]] int requestSnapshotCount() const noexcept
    {
        return m_requestSnapshotCount;
    }

    [[nodiscard]] int requestBodyStateCount() const noexcept
    {
        return m_requestBodyStateCount;
    }

    [[nodiscard]] int contextSnapshotCount() const noexcept
    {
        return m_contextSnapshotCount;
    }

    [[nodiscard]] int contextBodyStateCount() const noexcept
    {
        return m_contextBodyStateCount;
    }

    [[nodiscard]] std::optional<skygate::ephemeris::EphemerisEngineOptions> lastRequestOptions() const
    {
        return m_lastRequestOptions;
    }

    void resetCounters() const noexcept
    {
        m_requestSnapshotCount = 0;
        m_requestBodyStateCount = 0;
        m_contextSnapshotCount = 0;
        m_contextBodyStateCount = 0;
        m_lastRequestOptions.reset();
    }

private:
    [[nodiscard]] skygate::ephemeris::CelestialBodyState
    stateFor(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const noexcept
    {
        const skygate::core::HorizontalCoordinate horizontal = horizontalFor(request, bodyIndex);
        return skygate::ephemeris::CelestialBodyState{
            .bodyIndex = static_cast<std::uint32_t>(bodyIndex),
            .equatorial =
                {
                    .rightAscensionHours = 2.0 + static_cast<double>(bodyIndex) + optionTier(request.options),
                    .declinationDeg = horizontal.altitudeDeg / 2.0,
                },
            .horizontal = horizontal,
        };
    }

    [[nodiscard]] skygate::core::HorizontalCoordinate
    horizontalFor(const skygate::ephemeris::EphemerisRequest& request, const std::size_t bodyIndex) const noexcept
    {
        const int tier = optionTier(request.options);
        const double fraction = dayFraction(request.context.utcTime);
        const double wave = std::sin(2.0 * kPi * fraction);

        if (bodyIndex == 0U) {
            return {
                .altitudeDeg = 48.0 * std::sin(2.0 * kPi * (fraction - 0.25)) + static_cast<double>(tier),
                .azimuthDeg = 180.0 + 20.0 * wave,
            };
        }
        if (bodyIndex == 1U) {
            return {
                .altitudeDeg = 36.0 * std::sin(2.0 * kPi * (fraction + 0.18)) - static_cast<double>(tier),
                .azimuthDeg = 220.0 + 16.0 * wave,
            };
        }

        const double baseAltitude = 28.0 + static_cast<double>(tier) * 18.0;
        const double baseAzimuth = 116.0 + static_cast<double>(tier) * 43.0;
        return {
            .altitudeDeg = baseAltitude + 4.0 * wave,
            .azimuthDeg = baseAzimuth + 7.0 * std::cos(2.0 * kPi * fraction),
        };
    }

    std::shared_ptr<const std::vector<skygate::ephemeris::CelestialBody>> m_bodies;
    skygate::ephemeris::EphemerisEngineOptions m_options;
    mutable int m_requestSnapshotCount = 0;
    mutable int m_requestBodyStateCount = 0;
    mutable int m_contextSnapshotCount = 0;
    mutable int m_contextBodyStateCount = 0;
    mutable std::optional<skygate::ephemeris::EphemerisEngineOptions> m_lastRequestOptions;
};

struct MatrixHarness final {
    std::unique_ptr<SkyContextController> controller;
    std::unique_ptr<SkySceneModel> sceneModel;
    MatrixEphemerisEngine* engine = nullptr;
};

[[nodiscard]] MatrixHarness createMatrixHarness(
    const skygate::ephemeris::EphemerisEngineKind kind,
    const skygate::ephemeris::EphemerisCorrectionFlags correctionFlags,
    const skygate::core::ITimeSource* timeSource = nullptr
)
{
    auto bodies = matrixBodies();
    auto starCatalog = skygate::ui::tests::createTestCatalog(bodies);
    Q_ASSERT(starCatalog != nullptr);

    auto ephemerisEngine = std::make_unique<MatrixEphemerisEngine>(std::move(bodies), kind, correctionFlags);
    MatrixEphemerisEngine* engine = ephemerisEngine.get();

    SkyContextController::InitializationOptions options;
    options.loadSettings = false;
    options.initializeLocation = false;
    options.rebuildEphemerisEngineOnStartup = false;
    options.timeSource = timeSource;

    auto controller =
        std::make_unique<SkyContextController>(std::move(starCatalog), std::move(ephemerisEngine), options, nullptr);
    Q_ASSERT(skygate::ui::tests::configureTestSkyContext(*controller));

    auto sceneModel = std::make_unique<SkySceneModel>();
    sceneModel->setViewportSize(1100.0, 760.0);
    sceneModel->setSkyContextController(controller.get());

    return MatrixHarness{
        .controller = std::move(controller),
        .sceneModel = std::move(sceneModel),
        .engine = engine,
    };
}

void verifyRequestOptions(
    const skygate::ephemeris::EphemerisEngineOptions& actual,
    const skygate::ephemeris::EphemerisEngineKind kind,
    const skygate::ephemeris::EphemerisCorrectionFlags correctionFlags
)
{
    QCOMPARE(static_cast<std::uint8_t>(actual.engineKind), static_cast<std::uint8_t>(kind));
    QCOMPARE(static_cast<std::uint32_t>(actual.correctionFlags), static_cast<std::uint32_t>(correctionFlags));
}

void verifyConsumerMatrix(
    const skygate::ephemeris::EphemerisEngineKind kind,
    const skygate::ephemeris::EphemerisCorrectionFlags correctionFlags
)
{
    MatrixTimeSource timeSource;
    MatrixHarness harness = createMatrixHarness(kind, correctionFlags, &timeSource);
    auto& controller = *harness.controller;
    auto& sceneModel = *harness.sceneModel;
    auto& engine = *harness.engine;

    const auto initialRequestContext = controller.ephemerisRequestContext();
    verifyRequestOptions(initialRequestContext.request.options, kind, correctionFlags);
    const auto expectedFocusHorizontal = engine.expectedTargetHorizontal(initialRequestContext.request);

    QVERIFY(!sceneModel.renderPointSpan().empty());
    const auto referenceContext = sceneModel.referenceOverlayContext();
    QVERIFY(referenceContext.has_value());
    QVERIFY(nearlyEqual(
        referenceContext->observer.longitudeDeg,
        controller.skyContext().observer.longitudeDeg + expectedLongitudeOffset(initialRequestContext.request.options)
    ));

    engine.resetCounters();
    QVERIFY(controller.focusSearchTarget(QStringLiteral("body"), QString::fromUtf8(kTargetId.data())));

    QCOMPARE(controller.selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller.selectedSearchTargetId(), QString::fromUtf8(kTargetId.data()));
    QVERIFY(nearlyEqual(controller.viewCenterAltitudeDeg(), expectedFocusHorizontal.altitudeDeg));
    QVERIFY(nearlyEqual(controller.viewCenterAzimuthDeg(), expectedFocusHorizontal.azimuthDeg));
    QVERIFY(!sceneModel.selectionMarker().isEmpty());

    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("Matrix Target"));
    QCOMPARE(
        skygate::ui::tests::inspectorFieldValue(inspector, QStringLiteral("Alt / Az")),
        formattedHorizontal(expectedFocusHorizontal)
    );
    QVERIFY(!skygate::ui::tests::inspectorFieldValue(inspector, QStringLiteral("Rise")).isEmpty());
    QVERIFY(!skygate::ui::tests::inspectorFieldValue(inspector, QStringLiteral("Set")).isEmpty());
    QVERIFY(!skygate::ui::tests::inspectorFieldValue(inspector, QStringLiteral("Culmination")).isEmpty());
    QVERIFY(!sceneModel.renderLineSpan().empty());
    QVERIFY(engine.requestSnapshotCount() > 0);
    QVERIFY(engine.requestBodyStateCount() > 0);
    QCOMPARE(engine.contextSnapshotCount(), 0);
    QCOMPARE(engine.contextBodyStateCount(), 0);
    QVERIFY(engine.lastRequestOptions().has_value());
    verifyRequestOptions(*engine.lastRequestOptions(), kind, correctionFlags);

    engine.resetCounters();
    QVERIFY(controller.trackSearchTarget(QStringLiteral("body"), QString::fromUtf8(kTargetId.data())));
    const auto trackingRequestContext = controller.ephemerisRequestContext();
    const auto expectedTrackingHorizontal = engine.expectedTargetHorizontal(trackingRequestContext.request);
    QVERIFY(controller.hasTrackedTarget());
    QCOMPARE(controller.trackedTargetId(), QString::fromUtf8(kTargetId.data()));
    QVERIFY(nearlyEqual(controller.viewCenterAltitudeDeg(), expectedTrackingHorizontal.altitudeDeg));
    QVERIFY(nearlyEqual(controller.viewCenterAzimuthDeg(), expectedTrackingHorizontal.azimuthDeg));
    QVERIFY(engine.requestSnapshotCount() > 0);
    QCOMPARE(engine.contextSnapshotCount(), 0);

    engine.resetCounters();
    const QString iconKind = controller.nightConditionsIconKind();
    QVERIFY(QStringList({"sun", "twilight", "moon"}).contains(iconKind));
    controller.refreshNightConditions();
    QVERIFY(controller.nightConditions().value("valid").toBool());
    QVERIFY(engine.requestBodyStateCount() > 0);
    QCOMPARE(engine.contextBodyStateCount(), 0);
    QVERIFY(engine.lastRequestOptions().has_value());
    verifyRequestOptions(*engine.lastRequestOptions(), kind, correctionFlags);
}

}  // namespace

class SkyContextControllerSelectedEngineMatrixTests final : public QObject {
    Q_OBJECT

private slots:
    void consumersSwitchTogetherForSimpleAndHighPrecisionEngines();
    void optionChangesAffectPositionsWithoutChangingCatalogSearchResults();
};

void SkyContextControllerSelectedEngineMatrixTests::consumersSwitchTogetherForSimpleAndHighPrecisionEngines()
{
    verifyConsumerMatrix(
        skygate::ephemeris::EphemerisEngineKind::Simple, skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections
    );
    verifyConsumerMatrix(
        skygate::ephemeris::EphemerisEngineKind::HighPrecision, skygate::ephemeris::EphemerisCorrectionFlags::LightTime
    );
}

void SkyContextControllerSelectedEngineMatrixTests::optionChangesAffectPositionsWithoutChangingCatalogSearchResults()
{
    MatrixHarness astrometricHarness = createMatrixHarness(
        skygate::ephemeris::EphemerisEngineKind::HighPrecision,
        skygate::ephemeris::EphemerisCorrectionFlags::NoCorrections
    );
    MatrixHarness apparentHarness = createMatrixHarness(
        skygate::ephemeris::EphemerisEngineKind::HighPrecision, skygate::ephemeris::EphemerisCorrectionFlags::LightTime
    );

    auto* astrometricSearchModel =
        qobject_cast<SkyObjectSearchModel*>(astrometricHarness.controller->objectSearchModel());
    auto* apparentSearchModel = qobject_cast<SkyObjectSearchModel*>(apparentHarness.controller->objectSearchModel());
    QVERIFY(astrometricSearchModel != nullptr);
    QVERIFY(apparentSearchModel != nullptr);
    astrometricSearchModel->setFilterText(QStringLiteral("matrix"));
    apparentSearchModel->setFilterText(QStringLiteral("matrix"));
    QCOMPARE(astrometricSearchModel->rowCount(), apparentSearchModel->rowCount());
    QCOMPARE(astrometricSearchModel->rowCount(), 1);

    QVERIFY(
        astrometricHarness.controller->focusSearchTarget(QStringLiteral("body"), QString::fromUtf8(kTargetId.data()))
    );
    QVERIFY(apparentHarness.controller->focusSearchTarget(QStringLiteral("body"), QString::fromUtf8(kTargetId.data())));

    QVERIFY(!nearlyEqual(
        astrometricHarness.controller->viewCenterAltitudeDeg(), apparentHarness.controller->viewCenterAltitudeDeg()
    ));
    QVERIFY(!nearlyEqual(
        astrometricHarness.controller->viewCenterAzimuthDeg(), apparentHarness.controller->viewCenterAzimuthDeg()
    ));
    QCOMPARE(astrometricHarness.controller->catalogRevision(), apparentHarness.controller->catalogRevision());
}

QTEST_GUILESS_MAIN(SkyContextControllerSelectedEngineMatrixTests)

#include "SkyContextControllerSelectedEngineMatrixTests.moc"
