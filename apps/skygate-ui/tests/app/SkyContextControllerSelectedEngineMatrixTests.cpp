#include "ITimeSource.hpp"
#include "SkyContextController.hpp"
#include "SkyEphemerisTestSupport.hpp"
#include "SkyObjectSearchModel.hpp"
#include "SkyOverlayLayerSettings.hpp"
#include "SkySceneModel.hpp"
#include "SkySceneModelTestSupport.hpp"
#include "UtcTimeCodec.hpp"

#include <QtTest>

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
    return skygate::core::UtcTimeCodec::secondsSinceEpochDouble(utcTime);
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
    return skygate::ephemeris::EphemerisCorrectionFlags::has(
        options.correctionFlags(), skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
    );
}

[[nodiscard]] int optionTier(const skygate::ephemeris::EphemerisEngineOptions& options) noexcept
{
    if (options.engineKind() == skygate::ephemeris::EphemerisEngineKind::Type::Simple) {
        return 0;
    }
    return hasLightTime(options) ? 2 : 1;
}

[[nodiscard]] double expectedLongitudeOffset(const skygate::ephemeris::EphemerisEngineOptions& options) noexcept
{
    return 5.0 + static_cast<double>(optionTier(options)) * 30.0;
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

[[nodiscard]] bool pointsDiffer(const QPointF& lhs, const QPointF& rhs) noexcept
{
    constexpr double kPointTolerance = 1.0e-3;
    return !nearlyEqual(lhs.x(), rhs.x(), kPointTolerance) || !nearlyEqual(lhs.y(), rhs.y(), kPointTolerance);
}

[[nodiscard]] bool pointsEqual(const QPointF& lhs, const QPointF& rhs) noexcept
{
    return !pointsDiffer(lhs, rhs);
}

[[nodiscard]] bool lineFingerprintsDiffer(const QVector<double>& lhs, const QVector<double>& rhs) noexcept
{
    constexpr double kLineTolerance = 1.0e-3;
    if (lhs.size() != rhs.size()) {
        return true;
    }

    for (qsizetype index = 0; index < lhs.size(); ++index) {
        if (!nearlyEqual(lhs.at(index), rhs.at(index), kLineTolerance)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] skygate::ephemeris::EphemerisCorrectionFlags sceneRenderCorrectionFlagsFor(
    const skygate::ephemeris::EphemerisEngineKind::Type kind,
    const skygate::ephemeris::EphemerisCorrectionFlags correctionFlags
) noexcept
{
    if (kind != skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision) {
        return correctionFlags;
    }

    return skygate::ephemeris::EphemerisCorrectionFlags::precessionNutation()
           | skygate::ephemeris::EphemerisCorrectionFlags::earthOrientation()
           | skygate::ephemeris::EphemerisCorrectionFlags::diurnalParallax();
}

[[nodiscard]] const SkyRenderPoint*
renderPointForBodyIndex(const SkySceneModel& sceneModel, const std::uint32_t bodyIndex) noexcept
{
    for (const SkyRenderPoint& point : sceneModel.renderPointSpan()) {
        if (point.bodyIndex == bodyIndex) {
            return &point;
        }
    }
    return nullptr;
}

[[nodiscard]] QVariantMap referenceOverlayItem(const QVariantList& overlayItems, const QString& text)
{
    for (const QVariant& itemValue : overlayItems) {
        const QVariantMap item = itemValue.toMap();
        if (item.value(QStringLiteral("kind")).toString() == QStringLiteral("referenceLine")
            && item.value(QStringLiteral("text")).toString() == text) {
            return item;
        }
    }
    return {};
}

[[nodiscard]] QVector<double> trailLineFingerprint(const SkySceneModel& sceneModel)
{
    QVector<double> fingerprint;
    for (const SkyRenderLine& line : sceneModel.renderLineSpan()) {
        const bool isTrailLine = (nearlyEqual(line.widthPx, 1.4) && line.color.alpha() == 105)
                                 || (nearlyEqual(line.widthPx, 2.0) && line.color.alpha() == 175);
        if (!isTrailLine) {
            continue;
        }

        fingerprint.push_back(line.x1);
        fingerprint.push_back(line.y1);
        fingerprint.push_back(line.x2);
        fingerprint.push_back(line.y2);
    }
    return fingerprint;
}

[[nodiscard]] QString firstSunRowValue(const QVariantMap& nightConditions)
{
    const QVariantList sunRows = nightConditions.value(QStringLiteral("sunRows")).toList();
    if (sunRows.isEmpty()) {
        return {};
    }
    return sunRows.front().toMap().value(QStringLiteral("value")).toString();
}

class MatrixEphemerisEngine final : public skygate::ephemeris::IEphemerisEngine {
public:
    MatrixEphemerisEngine(
        std::vector<skygate::ephemeris::CelestialBody> bodies,
        const skygate::ephemeris::EphemerisEngineKind::Type kind,
        const skygate::ephemeris::EphemerisCorrectionFlags correctionFlags
    )
        : m_bodies(std::make_shared<const std::vector<skygate::ephemeris::CelestialBody>>(std::move(bodies)))
    {
        m_options.setEngineKind(kind);
        m_options.setCorrectionFlags(correctionFlags);
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineKind::Type kind() const noexcept override
    {
        return m_options.engineKind();
    }

    [[nodiscard]] std::string_view name() const noexcept override
    {
        return "Selected-engine matrix test engine";
    }

    [[nodiscard]] skygate::ephemeris::EphemerisCapabilities capabilities() const noexcept override
    {
        skygate::ephemeris::EphemerisCapabilities capabilities =
            skygate::ephemeris::EphemerisCapabilities::noCapabilities();

        capabilities = capabilities | skygate::ephemeris::EphemerisCapabilities::catalogStars();
        capabilities = capabilities | skygate::ephemeris::EphemerisCapabilities::topocentricPositions();
        return capabilities;
    }

    [[nodiscard]] skygate::ephemeris::EphemerisEngineOptions options() const noexcept override
    {
        return m_options;
    }

    [[nodiscard]] skygate::ephemeris::SkySnapshot
    compute(const skygate::ephemeris::EphemerisRequest& request) const override
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
            const double sunPhase = fraction - 0.25 + static_cast<double>(tier) * 0.04;
            return {
                .altitudeDeg = 48.0 * std::sin(2.0 * kPi * sunPhase),
                .azimuthDeg = 180.0 + 20.0 * wave,
            };
        }
        if (bodyIndex == 1U) {
            return {
                .altitudeDeg = 36.0 * std::sin(2.0 * kPi * (fraction + 0.18)) - static_cast<double>(tier),
                .azimuthDeg = 220.0 + 16.0 * wave,
            };
        }

        const double targetPhase = fraction - 0.5 + static_cast<double>(tier) * 0.05;
        const double targetAzimuthPhase = fraction + static_cast<double>(tier) * 0.03;
        return {
            .altitudeDeg = 35.0 + static_cast<double>(tier) * 5.0 + 6.0 * std::sin(2.0 * kPi * targetPhase),
            .azimuthDeg = 120.0 + static_cast<double>(tier) * 30.0 + 12.0 * std::cos(2.0 * kPi * targetAzimuthPhase),
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
    const skygate::ephemeris::EphemerisEngineKind::Type kind,
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

    auto* overlayLayers = qobject_cast<SkyOverlayLayerSettings*>(controller->overlayLayers());
    Q_ASSERT(overlayLayers != nullptr);
    overlayLayers->setEcliptic(true);
    overlayLayers->setCelestialEquator(true);
    overlayLayers->setCircumpolarBoundary(true);

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
    const skygate::ephemeris::EphemerisEngineKind::Type kind,
    const skygate::ephemeris::EphemerisCorrectionFlags correctionFlags
)
{
    QCOMPARE(static_cast<std::uint8_t>(actual.engineKind()), static_cast<std::uint8_t>(kind));
    QCOMPARE(static_cast<std::uint32_t>(actual.correctionFlags()), static_cast<std::uint32_t>(correctionFlags));
}

struct MatrixConsumerObservations final {
    QPointF targetRenderPoint;
    double referenceLongitudeDeg = 0.0;
    QPointF eclipticLabelPoint;
    QPointF celestialEquatorLabelPoint;
    QPointF circumpolarLabelPoint;
    QString altAzText;
    QString raDecText;
    QString riseText;
    QString setText;
    QString culminationText;
    QVector<double> trailLineFingerprint;
    QString nightIconKind;
    QString sunsetText;
    QString moonRiseText;
    QString moonSetText;
};

void verifyConsumerMatrix(
    MatrixConsumerObservations& observations,
    const skygate::ephemeris::EphemerisEngineKind::Type kind,
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
    auto renderRequest = initialRequestContext.request;
    renderRequest.options.setCorrectionFlags(sceneRenderCorrectionFlagsFor(kind, correctionFlags));
    QVERIFY(engine.lastRequestOptions().has_value());
    verifyRequestOptions(*engine.lastRequestOptions(), kind, renderRequest.options.correctionFlags());

    const auto expectedFocusHorizontal = engine.expectedTargetHorizontal(initialRequestContext.request);

    const auto* targetPoint = renderPointForBodyIndex(sceneModel, static_cast<std::uint32_t>(kTargetIndex));
    QVERIFY(targetPoint != nullptr);
    const auto projection = sceneModel.preparedProjection();
    QVERIFY(projection.has_value());
    const auto expectedTargetPoint = projection->project(engine.expectedTargetHorizontal(renderRequest));
    QVERIFY(expectedTargetPoint.isVisible);
    QVERIFY(nearlyEqual(targetPoint->x, expectedTargetPoint.x));
    QVERIFY(nearlyEqual(targetPoint->y, expectedTargetPoint.y));
    observations.targetRenderPoint = QPointF(targetPoint->x, targetPoint->y);

    const auto referenceContext = sceneModel.referenceOverlayContext();
    QVERIFY(referenceContext.has_value());
    QVERIFY(nearlyEqual(
        referenceContext->observer.longitudeDeg,
        controller.skyContext().observer.longitudeDeg + expectedLongitudeOffset(renderRequest.options)
    ));
    observations.referenceLongitudeDeg = referenceContext->observer.longitudeDeg;

    const QVariantMap eclipticItem = referenceOverlayItem(sceneModel.overlayItems(), QStringLiteral("Ecliptic"));
    const QVariantMap celestialEquatorItem =
        referenceOverlayItem(sceneModel.overlayItems(), QStringLiteral("Celestial equator"));
    QVERIFY(!eclipticItem.isEmpty());
    QVERIFY(!celestialEquatorItem.isEmpty());
    observations.eclipticLabelPoint =
        QPointF(eclipticItem.value(QStringLiteral("x")).toDouble(), eclipticItem.value(QStringLiteral("y")).toDouble());
    observations.celestialEquatorLabelPoint = QPointF(
        celestialEquatorItem.value(QStringLiteral("x")).toDouble(),
        celestialEquatorItem.value(QStringLiteral("y")).toDouble()
    );

    engine.resetCounters();
    QVERIFY(controller.focusSearchTarget(QStringLiteral("body"), QString::fromUtf8(kTargetId.data())));

    QCOMPARE(controller.selectedSearchTargetKind(), QString("body"));
    QCOMPARE(controller.selectedSearchTargetId(), QString::fromUtf8(kTargetId.data()));
    QVERIFY(nearlyEqual(controller.viewCenterAltitudeDeg(), expectedFocusHorizontal.altitudeDeg));
    QVERIFY(nearlyEqual(controller.viewCenterAzimuthDeg(), expectedFocusHorizontal.azimuthDeg));
    QVERIFY(!sceneModel.selectionMarker().isEmpty());

    const QVariantMap inspector = sceneModel.selectedObjectInspector();
    QCOMPARE(inspector.value("title").toString(), QString("Matrix Target"));
    observations.altAzText = skygate::ui::tests::inspectorFieldValue(inspector, QStringLiteral("Alt / Az"));
    observations.raDecText = skygate::ui::tests::inspectorFieldValue(inspector, QStringLiteral("RA / Dec"));
    observations.riseText = skygate::ui::tests::inspectorFieldValue(inspector, QStringLiteral("Rise"));
    observations.setText = skygate::ui::tests::inspectorFieldValue(inspector, QStringLiteral("Set"));
    observations.culminationText = skygate::ui::tests::inspectorFieldValue(inspector, QStringLiteral("Culmination"));
    QCOMPARE(observations.altAzText, formattedHorizontal(expectedFocusHorizontal));
    QVERIFY(!observations.raDecText.isEmpty());
    QVERIFY(!observations.riseText.isEmpty());
    QVERIFY(!observations.setText.isEmpty());
    QVERIFY(!observations.culminationText.isEmpty());
    observations.trailLineFingerprint = trailLineFingerprint(sceneModel);
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
    QVERIFY(engine.requestBodyStateCount() > 0);
    QCOMPARE(engine.contextSnapshotCount(), 0);

    engine.resetCounters();
    observations.nightIconKind = controller.nightConditionsIconKind();
    QVERIFY(QStringList({"sun", "twilight", "moon"}).contains(observations.nightIconKind));
    controller.refreshNightConditions();
    const QVariantMap nightConditions = controller.nightConditions();
    QVERIFY(nightConditions.value("valid").toBool());
    observations.sunsetText = firstSunRowValue(nightConditions);
    observations.moonRiseText = nightConditions.value(QStringLiteral("moonRiseText")).toString();
    observations.moonSetText = nightConditions.value(QStringLiteral("moonSetText")).toString();
    QVERIFY(!observations.sunsetText.isEmpty());
    QVERIFY(!observations.moonRiseText.isEmpty());
    QVERIFY(!observations.moonSetText.isEmpty());
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
    MatrixConsumerObservations simpleObservations;
    verifyConsumerMatrix(
        simpleObservations,
        skygate::ephemeris::EphemerisEngineKind::Type::Simple,
        skygate::ephemeris::EphemerisCorrectionFlags::noCorrections()
    );
    MatrixConsumerObservations highPrecisionObservations;
    verifyConsumerMatrix(
        highPrecisionObservations,
        skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision,
        skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
    );

    QVERIFY(pointsDiffer(simpleObservations.targetRenderPoint, highPrecisionObservations.targetRenderPoint));
    QVERIFY(!nearlyEqual(simpleObservations.referenceLongitudeDeg, highPrecisionObservations.referenceLongitudeDeg));
    QVERIFY(pointsDiffer(simpleObservations.eclipticLabelPoint, highPrecisionObservations.eclipticLabelPoint));
    QVERIFY(simpleObservations.altAzText != highPrecisionObservations.altAzText);
    QVERIFY(simpleObservations.raDecText != highPrecisionObservations.raDecText);
    QVERIFY(
        simpleObservations.riseText != highPrecisionObservations.riseText
        || simpleObservations.setText != highPrecisionObservations.setText
        || simpleObservations.culminationText != highPrecisionObservations.culminationText
    );
    if (!simpleObservations.trailLineFingerprint.isEmpty()
        && !highPrecisionObservations.trailLineFingerprint.isEmpty()) {
        QVERIFY(lineFingerprintsDiffer(
            simpleObservations.trailLineFingerprint, highPrecisionObservations.trailLineFingerprint
        ));
    }
    QVERIFY(
        simpleObservations.nightIconKind != highPrecisionObservations.nightIconKind
        || simpleObservations.sunsetText != highPrecisionObservations.sunsetText
        || simpleObservations.moonRiseText != highPrecisionObservations.moonRiseText
        || simpleObservations.moonSetText != highPrecisionObservations.moonSetText
    );
}

void SkyContextControllerSelectedEngineMatrixTests::optionChangesAffectPositionsWithoutChangingCatalogSearchResults()
{
    MatrixHarness astrometricHarness = createMatrixHarness(
        skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision,
        skygate::ephemeris::EphemerisCorrectionFlags::noCorrections()
    );
    MatrixHarness apparentHarness = createMatrixHarness(
        skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision,
        skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
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

    MatrixConsumerObservations astrometricObservations;
    verifyConsumerMatrix(
        astrometricObservations,
        skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision,
        skygate::ephemeris::EphemerisCorrectionFlags::noCorrections()
    );
    MatrixConsumerObservations apparentObservations;
    verifyConsumerMatrix(
        apparentObservations,
        skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision,
        skygate::ephemeris::EphemerisCorrectionFlags::lightTime()
    );

    QVERIFY(pointsEqual(astrometricObservations.targetRenderPoint, apparentObservations.targetRenderPoint));
    QVERIFY(nearlyEqual(astrometricObservations.referenceLongitudeDeg, apparentObservations.referenceLongitudeDeg));
    QVERIFY(pointsEqual(astrometricObservations.eclipticLabelPoint, apparentObservations.eclipticLabelPoint));
    QVERIFY(
        pointsEqual(astrometricObservations.celestialEquatorLabelPoint, apparentObservations.celestialEquatorLabelPoint)
    );
    QVERIFY(!astrometricObservations.altAzText.isEmpty());
    QVERIFY(!apparentObservations.altAzText.isEmpty());
    QVERIFY(!astrometricObservations.raDecText.isEmpty());
    QVERIFY(!apparentObservations.raDecText.isEmpty());
    QVERIFY(
        astrometricObservations.riseText != apparentObservations.riseText
        || astrometricObservations.setText != apparentObservations.setText
        || astrometricObservations.culminationText != apparentObservations.culminationText
    );
    if (!astrometricObservations.trailLineFingerprint.isEmpty()
        && !apparentObservations.trailLineFingerprint.isEmpty()) {
        QVERIFY(lineFingerprintsDiffer(
            astrometricObservations.trailLineFingerprint, apparentObservations.trailLineFingerprint
        ));
    }
    QVERIFY(!astrometricObservations.nightIconKind.isEmpty());
    QVERIFY(!apparentObservations.nightIconKind.isEmpty());
    QVERIFY(!astrometricObservations.sunsetText.isEmpty());
    QVERIFY(!apparentObservations.sunsetText.isEmpty());
}

QTEST_GUILESS_MAIN(SkyContextControllerSelectedEngineMatrixTests)

#include "SkyContextControllerSelectedEngineMatrixTests.moc"
