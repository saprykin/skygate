#include "EphemerisFixtureSupport.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/AtmosphericRefractionCalculator.hpp"
#include "engine/highprecision/FrameTransformer.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QtTest/QtTest>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using skygate::ephemeris::tests::angularSeparationDegrees;
using skygate::ephemeris::tests::EphemerisRaDecExpectation;
namespace core = skygate::core;

constexpr double kSecondsPerDay = 86'400.0;

struct TopocentricFixtureCase {
    CelestialBody body;
    SolarSystemKernelVector inputGcrsPositionAu;
    EphemerisRaDecExpectation expectedEquatorial;
    core::HorizontalCoordinate expectedHorizontal;
};

struct TopocentricFixture {
    EphemerisRequest request;
    double ttMinusUtcSeconds = 0.0;
    double ut1MinusUtcSeconds = 0.0;
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    double angularToleranceDegrees = 0.0;
    double horizontalToleranceDegrees = 0.0;
    std::vector<TopocentricFixtureCase> cases;
};

[[nodiscard]] QString topocentricFixturePath()
{
    return QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR
                          "/ephemeris/topocentric_observer_apparent_solar_system_smoke.json");
}

[[nodiscard]] core::SkyContext makeContext()
{
    core::SkyContext context;
    context.utcTime = core::UtcTimePoint(std::chrono::seconds(1'704'067'200));
    context.observer = {
        .latitudeDeg = 37.7749,
        .longitudeDeg = -122.4194,
        .elevationMeters = 10.0,
    };
    return context;
}

[[nodiscard]] EphemerisRequest makeRequest(const EphemerisCorrectionFlags correctionFlags)
{
    EphemerisRequest request;
    request.context = makeContext();
    request.epoch = {
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = TimeScale::Tdb,
    };
    request.options.engineKind = EphemerisEngineKind::HighPrecision;
    request.options.correctionFlags = correctionFlags;
    request.options.enableAtmosphericRefraction = false;
    return request;
}

[[nodiscard]] CelestialBody makeBody()
{
    return {
        .id = "sun",
        .displayName = "Sun",
        .type = CelestialBodyType::Sun,
        .ephemerisSource = CelestialBodyEphemerisSource::Sun,
    };
}

[[nodiscard]] CelestialBody makeBody(const QString& id, const QString& bodyType)
{
    if (bodyType == QStringLiteral("sun")) {
        return {
            .id = id.toStdString(),
            .displayName = "Sun",
            .type = CelestialBodyType::Sun,
            .ephemerisSource = CelestialBodyEphemerisSource::Sun,
        };
    }
    if (bodyType == QStringLiteral("moon")) {
        return {
            .id = id.toStdString(),
            .displayName = "Moon",
            .type = CelestialBodyType::Moon,
            .ephemerisSource = CelestialBodyEphemerisSource::Moon,
        };
    }

    return {
        .id = id.toStdString(),
        .displayName = id.toStdString(),
        .type = CelestialBodyType::Planet,
        .ephemerisSource = CelestialBodyEphemerisSource::Planet,
    };
}

[[nodiscard]] HighPrecisionComputationInput makeInput(const EphemerisRequest& request)
{
    static const CelestialBody kBody = makeBody();
    return {
        .request = request,
        .body = kBody,
        .bodyIndex = 0U,
    };
}

[[nodiscard]] HighPrecisionComputationInput makeInput(const EphemerisRequest& request, const CelestialBody& body)
{
    return {
        .request = request,
        .body = body,
        .bodyIndex = 0U,
    };
}

[[nodiscard]] HighPrecisionCalculatorResult makeCalculatorResult()
{
    HighPrecisionCalculatorResult result;
    result.equatorial = core::EquatorialCoordinate{
        .rightAscensionHours = 0.0,
        .declinationDeg = 0.0,
    };
    result.metadata.dataSourceProvenance = "unit-test source";
    return result;
}

[[nodiscard]] HighPrecisionCalculatorResult makeSolarSystemCalculatorResult(const SolarSystemKernelVector& positionAu)
{
    HighPrecisionCalculatorResult result = makeCalculatorResult();
    result.observerRelativePositionAu = positionAu;
    return result;
}

[[nodiscard]] double addSecondsToJulianDatePart2(const double julianDatePart2, const double seconds) noexcept
{
    return julianDatePart2 + seconds / kSecondsPerDay;
}

[[nodiscard]] double angularDifferenceDegrees(const double lhs, const double rhs) noexcept
{
    double difference = std::fmod(lhs - rhs + 540.0, 360.0) - 180.0;
    if (difference < -180.0) {
        difference += 360.0;
    }
    return std::abs(difference);
}

[[nodiscard]] SolarSystemKernelVector parseKernelVector(const QJsonArray& array)
{
    Q_ASSERT(array.size() == 3);
    return {
        .xAu = array.at(0).toDouble(),
        .yAu = array.at(1).toDouble(),
        .zAu = array.at(2).toDouble(),
    };
}

[[nodiscard]] std::optional<TopocentricFixture> loadTopocentricFixture(QString* errorText)
{
    auto fail = [errorText](const QString& message) -> std::optional<TopocentricFixture> {
        if (errorText != nullptr) {
            *errorText = message;
        }
        return std::nullopt;
    };

    QFile file(topocentricFixturePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("Could not open topocentric fixture"));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QStringLiteral("Topocentric fixture is not valid JSON object content"));
    }

    const QJsonObject root = document.object();
    const QJsonObject requestObject = root.value(QStringLiteral("request")).toObject();
    const QJsonObject toleranceObject = root.value(QStringLiteral("tolerance")).toObject();
    const QJsonArray cases = root.value(QStringLiteral("cases")).toArray();
    if (cases.isEmpty()) {
        return fail(QStringLiteral("Topocentric fixture has no cases"));
    }

    TopocentricFixture fixture;
    fixture.request.context.observer = {
        .latitudeDeg = requestObject.value(QStringLiteral("latitudeDegrees")).toDouble(),
        .longitudeDeg = requestObject.value(QStringLiteral("longitudeDegrees")).toDouble(),
        .elevationMeters = requestObject.value(QStringLiteral("elevationMeters")).toDouble(),
    };
    fixture.request.epoch = {
        .julianDatePart1 = requestObject.value(QStringLiteral("utcJulianDate")).toDouble(),
        .julianDatePart2 = 0.0,
        .timeScale = TimeScale::Utc,
    };
    fixture.request.options.engineKind = EphemerisEngineKind::HighPrecision;
    fixture.request.options.correctionFlags = EphemerisCorrectionFlags::Topocentric;
    fixture.request.options.enableAtmosphericRefraction = false;
    fixture.ttMinusUtcSeconds = requestObject.value(QStringLiteral("ttMinusUtcSeconds")).toDouble();
    fixture.ut1MinusUtcSeconds = requestObject.value(QStringLiteral("ut1MinusUtcSeconds")).toDouble();
    fixture.polarMotionXArcseconds = requestObject.value(QStringLiteral("polarMotionXArcseconds")).toDouble();
    fixture.polarMotionYArcseconds = requestObject.value(QStringLiteral("polarMotionYArcseconds")).toDouble();
    fixture.angularToleranceDegrees = toleranceObject.value(QStringLiteral("angularDegrees")).toDouble();
    fixture.horizontalToleranceDegrees = toleranceObject.value(QStringLiteral("horizontalDegrees")).toDouble();

    for (const QJsonValue& value : cases) {
        const QJsonObject object = value.toObject();
        const QJsonObject expected = object.value(QStringLiteral("expected")).toObject();
        fixture.cases.push_back(TopocentricFixtureCase{
            .body = makeBody(
                object.value(QStringLiteral("bodyId")).toString(), object.value(QStringLiteral("bodyType")).toString()
            ),
            .inputGcrsPositionAu = parseKernelVector(object.value(QStringLiteral("inputGcrsPositionAu")).toArray()),
            .expectedEquatorial =
                {
                    .rightAscensionHours = expected.value(QStringLiteral("rightAscensionHours")).toDouble(),
                    .declinationDegrees = expected.value(QStringLiteral("declinationDegrees")).toDouble(),
                },
            .expectedHorizontal =
                {
                    .altitudeDeg = expected.value(QStringLiteral("altitudeDegrees")).toDouble(),
                    .azimuthDeg = expected.value(QStringLiteral("azimuthDegrees")).toDouble(),
                },
        });
    }

    return fixture;
}

class RecordingFrameTransformer final : public IFrameTransformer {
public:
    [[nodiscard]] CelestialFrameTransformResult transformCelestialVector(const CelestialFrameTransformRequest& request
    ) const override
    {
        ++m_callCount;
        m_lastSourceFrame = request.sourceFrame;
        m_lastTargetFrame = request.targetFrame;
        return {
            .vector = m_resultVector.value_or(request.vector),
            .metadata = m_resultMetadata,
        };
    }

    void setResultVector(const CelestialFrameVector& vector) noexcept
    {
        m_resultVector = vector;
    }

    void setResultMetadata(const EphemerisResultMetadata& metadata)
    {
        m_resultMetadata = metadata;
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] CelestialReferenceFrame lastSourceFrame() const noexcept
    {
        return m_lastSourceFrame;
    }

    [[nodiscard]] CelestialReferenceFrame lastTargetFrame() const noexcept
    {
        return m_lastTargetFrame;
    }

private:
    mutable int m_callCount = 0;
    mutable CelestialReferenceFrame m_lastSourceFrame = CelestialReferenceFrame::Icrs;
    mutable CelestialReferenceFrame m_lastTargetFrame = CelestialReferenceFrame::Icrs;
    std::optional<CelestialFrameVector> m_resultVector;
    EphemerisResultMetadata m_resultMetadata = {.status = EphemerisResultStatus::Valid};
};

class PassThroughFrameTransformer final : public IFrameTransformer {
public:
    [[nodiscard]] CelestialFrameTransformResult transformCelestialVector(const CelestialFrameTransformRequest& request
    ) const override
    {
        ++m_callCount;
        m_lastSourceFrame = request.sourceFrame;
        m_lastTargetFrame = request.targetFrame;
        m_lastVector = request.vector;

        EphemerisResultMetadata metadata;
        metadata.status = EphemerisResultStatus::Valid;
        if (request.sourceFrame == CelestialReferenceFrame::Gcrs
            && request.targetFrame == CelestialReferenceFrame::TrueEquatorAndEquinox) {
            metadata.appliedCorrections = EphemerisCorrectionFlags::PrecessionNutation;
        } else if (request.sourceFrame != request.targetFrame) {
            metadata.appliedCorrections = EphemerisCorrectionFlags::EarthOrientation;
        }

        return {
            .vector = request.vector,
            .metadata = metadata,
        };
    }

    [[nodiscard]] int callCount() const noexcept
    {
        return m_callCount;
    }

    [[nodiscard]] CelestialReferenceFrame lastSourceFrame() const noexcept
    {
        return m_lastSourceFrame;
    }

    [[nodiscard]] CelestialReferenceFrame lastTargetFrame() const noexcept
    {
        return m_lastTargetFrame;
    }

    [[nodiscard]] CelestialFrameVector lastVector() const noexcept
    {
        return m_lastVector;
    }

private:
    mutable int m_callCount = 0;
    mutable CelestialReferenceFrame m_lastSourceFrame = CelestialReferenceFrame::Icrs;
    mutable CelestialReferenceFrame m_lastTargetFrame = CelestialReferenceFrame::Icrs;
    mutable CelestialFrameVector m_lastVector;
};

class ValidUtcTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        ++m_convertCallCount;
        m_lastTargetScale = targetScale;
        return {
            .epoch = normalizedAstronomicalEpoch(AstronomicalEpoch{
                .julianDatePart1 = epoch.julianDatePart1,
                .julianDatePart2 = epoch.julianDatePart2,
                .timeScale = targetScale,
            }),
            .status = TimeScaleConversionStatus::Valid,
            .diagnosticText = "unit test conversion",
        };
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);
        static_cast<void>(targetScale);
        return {};
    }

    [[nodiscard]] int convertCallCount() const noexcept
    {
        return m_convertCallCount;
    }

    [[nodiscard]] TimeScale lastTargetScale() const noexcept
    {
        return m_lastTargetScale;
    }

private:
    mutable int m_convertCallCount = 0;
    mutable TimeScale m_lastTargetScale = TimeScale::Utc;
};

class FixedTopocentricTimeScaleService final : public ITimeScaleService {
public:
    FixedTopocentricTimeScaleService(const double ttMinusUtcSeconds, const double ut1MinusUtcSeconds)
        : m_ttMinusUtcSeconds(ttMinusUtcSeconds), m_ut1MinusUtcSeconds(ut1MinusUtcSeconds)
    {
    }

    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        TimeScaleConversionResult result;
        result.status = TimeScaleConversionStatus::Valid;
        result.diagnosticText = "unit test fixed UTC conversion";
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        if (epoch.timeScale == TimeScale::Utc && targetScale == TimeScale::Tt) {
            result.epoch.julianDatePart2 = addSecondsToJulianDatePart2(epoch.julianDatePart2, m_ttMinusUtcSeconds);
        } else if (epoch.timeScale == TimeScale::Utc && targetScale == TimeScale::Ut1) {
            result.epoch.julianDatePart2 = addSecondsToJulianDatePart2(epoch.julianDatePart2, m_ut1MinusUtcSeconds);
        }
        result.epoch = normalizedAstronomicalEpoch(result.epoch);
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);
        static_cast<void>(targetScale);
        return {};
    }

private:
    double m_ttMinusUtcSeconds = 0.0;
    double m_ut1MinusUtcSeconds = 0.0;
};

class DegradedTtTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        ++m_convertCallCount;
        m_lastTargetScale = targetScale;
        if (targetScale != TimeScale::Tt) {
            TimeScaleConversionResult result;
            result.epoch = epoch;
            result.status = TimeScaleConversionStatus::Failed;
            result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
            return result;
        }

        TimeScaleConversionResult result;
        result.epoch = normalizedAstronomicalEpoch(AstronomicalEpoch{
            .julianDatePart1 = epoch.julianDatePart1,
            .julianDatePart2 = epoch.julianDatePart2,
            .timeScale = TimeScale::Tt,
        });
        result.status = TimeScaleConversionStatus::Degraded;
        result.diagnosticText = "unit test degraded TT conversion";
        result.addWarning(TimeScaleConversionWarningCode::LeapSecondTableMissing);
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        static_cast<void>(dateTime);
        static_cast<void>(targetScale);
        return {};
    }

    [[nodiscard]] int convertCallCount() const noexcept
    {
        return m_convertCallCount;
    }

    [[nodiscard]] TimeScale lastTargetScale() const noexcept
    {
        return m_lastTargetScale;
    }

private:
    mutable int m_convertCallCount = 0;
    mutable TimeScale m_lastTargetScale = TimeScale::Utc;
};

[[nodiscard]] std::shared_ptr<const IEarthOrientationProvider> makeEarthOrientationProvider(
    const AstronomicalEpoch& utcEpoch,
    const double ut1MinusUtcSeconds,
    const double polarMotionXArcseconds,
    const double polarMotionYArcseconds
)
{
    EarthOrientationDataInfo info;
    info.version = "unit-test-topocentric-eop";
    info.provenance = "JPL Horizons topocentric validation fixture";
    info.status = EarthOrientationDataStatus::Available;
    info.diagnosticText = "Earth-orientation data loaded.";

    EarthOrientationTableEntry entry;
    entry.effectiveUtcEpoch = utcEpoch;
    entry.ut1MinusUtcSeconds = ut1MinusUtcSeconds;
    entry.polarMotionXArcseconds = polarMotionXArcseconds;
    entry.polarMotionYArcseconds = polarMotionYArcseconds;

    return std::make_shared<TableBackedEarthOrientationProvider>(
        std::move(info), std::vector<EarthOrientationTableEntry>{entry}
    );
}

}  // namespace

class ApparentPlaceCalculatorTests final : public QObject {
    Q_OBJECT

private slots:
    void routesGeometricRequestsToGcrs();
    void routesRequestsWithoutPrecessionNutationToGcrs();
    void routesAstrometricRequestsWithUnsupportedRefractionToGcrs();
    void routesApparentRequestsToTrueEquatorAndEquinox();
    void appliesPrecessionNutationWhenRequested();
    void propagatesDegradedTransformMetadataForPrecessionNutation();
    void propagatesDegradedRealFrameTransformMetadataForPrecessionNutation();
    void appliesTopocentricParallaxAndHorizontalCoordinates();
    void validatesTopocentricMoonSunPlanetAgainstHorizonsFixtures();
    void reportsInvalidObserverForTopocentricRequest();
    void reportsMissingEarthOrientationForTopocentricRequest();
    void reportsUnavailableParallaxWhenDistanceVectorIsMissing();
    void leavesApparentRequestGeocentricWhenParallaxIsDisabled();
    void changesTopocentricPositionWhenObserverElevationChanges();
    void appliesAtmosphericRefractionToTopocentricHorizontalCoordinates();
    void reportsUnavailableRefractionMode();
};

void ApparentPlaceCalculatorTests::routesGeometricRequestsToGcrs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Geometric);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastSourceFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 0.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
}

void ApparentPlaceCalculatorTests::routesRequestsWithoutPrecessionNutationToGcrs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::LightTime);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastSourceFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 0.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
}

void ApparentPlaceCalculatorTests::routesAstrometricRequestsWithUnsupportedRefractionToGcrs()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    EphemerisRequest request =
        makeRequest(EphemerisCorrectionFlags::LightTime | EphemerisCorrectionFlags::AtmosphericRefraction);
    request.options.enableAtmosphericRefraction = true;

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastSourceFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 0.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
}

void ApparentPlaceCalculatorTests::routesApparentRequestsToTrueEquatorAndEquinox()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Apparent);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastSourceFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::TrueEquatorAndEquinox)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
}

void ApparentPlaceCalculatorTests::appliesPrecessionNutationWhenRequested()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    frameTransformer->setResultVector(CelestialFrameVector{
        .x = 0.0,
        .y = 1.0,
        .z = 0.0,
    });
    EphemerisResultMetadata transformMetadata;
    transformMetadata.status = EphemerisResultStatus::Valid;
    transformMetadata.appliedCorrections = EphemerisCorrectionFlags::PrecessionNutation;
    frameTransformer->setResultMetadata(transformMetadata);

    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request =
        makeRequest(EphemerisCorrectionFlags::LightTime | EphemerisCorrectionFlags::PrecessionNutation);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::TrueEquatorAndEquinox)
    );
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(result.equatorial->rightAscensionHours, 6.0);
    QCOMPARE(result.equatorial->declinationDeg, 0.0);
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::PrecessionNutation));
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
    );
}

void ApparentPlaceCalculatorTests::propagatesDegradedTransformMetadataForPrecessionNutation()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    EphemerisResultMetadata transformMetadata;
    transformMetadata.status = EphemerisResultStatus::Degraded;
    transformMetadata.addWarning(EphemerisWarningCode::AccuracyDegraded);
    transformMetadata.addWarning(EphemerisWarningCode::TimeScaleDataUnavailable);
    transformMetadata.appliedCorrections = EphemerisCorrectionFlags::PrecessionNutation;
    frameTransformer->setResultMetadata(transformMetadata);

    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::PrecessionNutation);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::TrueEquatorAndEquinox)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::PrecessionNutation));
}

void ApparentPlaceCalculatorTests::propagatesDegradedRealFrameTransformMetadataForPrecessionNutation()
{
    const ErfaFrameTransformer availabilityTransformer(nullptr);
    const CelestialFrameTransformResult availabilityResult =
        availabilityTransformer.transformCelestialVector(CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Cirs,
            .epoch =
                {
                    .julianDatePart1 = 2'400'000.5,
                    .julianDatePart2 = 53'736.0,
                    .timeScale = TimeScale::Tt,
                },
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        });
    if (!availabilityResult.vector.has_value()) {
        QSKIP("ERFA-backed frame transforms are not available in this build.");
    }

    auto timeScaleService = std::make_shared<DegradedTtTimeScaleService>();
    const auto frameTransformer = std::make_shared<ErfaFrameTransformer>(timeScaleService);
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::PrecessionNutation);
    request.epoch = {
        .julianDatePart1 = 2'400'000.5,
        .julianDatePart2 = 53'736.0,
        .timeScale = TimeScale::Utc,
    };

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(timeScaleService->convertCallCount(), 1);
    QCOMPARE(static_cast<std::uint8_t>(timeScaleService->lastTargetScale()), static_cast<std::uint8_t>(TimeScale::Tt));
    QVERIFY(result.equatorial.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::PrecessionNutation));
}

void ApparentPlaceCalculatorTests::appliesTopocentricParallaxAndHorizontalCoordinates()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Topocentric);
    request.context.observer = {
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };

    const HighPrecisionCalculatorResult result = calculator.apply(
        makeInput(request),
        makeSolarSystemCalculatorResult(SolarSystemKernelVector{
            .xAu = 0.0,
            .yAu = 1.0,
            .zAu = 0.0,
        })
    );

    QCOMPARE(timeScaleService->convertCallCount(), 1);
    QCOMPARE(static_cast<std::uint8_t>(timeScaleService->lastTargetScale()), static_cast<std::uint8_t>(TimeScale::Utc));
    QCOMPARE(frameTransformer->callCount(), 3);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::TrueEquatorAndEquinox)
    );
    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.horizontal.has_value());
    QVERIFY(result.equatorial->rightAscensionHours > 6.0);
    QVERIFY(std::abs(result.equatorial->declinationDeg) < 1.0e-12);
    QVERIFY(result.horizontal->altitudeDeg < 0.0);
    QVERIFY(std::abs(result.horizontal->azimuthDeg - 90.0) < 1.0e-9);
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax));
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
}

void ApparentPlaceCalculatorTests::validatesTopocentricMoonSunPlanetAgainstHorizonsFixtures()
{
    QString errorText;
    const std::optional<TopocentricFixture> loadedFixture = loadTopocentricFixture(&errorText);
    QVERIFY2(loadedFixture.has_value(), qPrintable(errorText));
    const TopocentricFixture& fixture = *loadedFixture;
    QCOMPARE(fixture.cases.size(), static_cast<std::size_t>(3U));

    auto timeScaleService =
        std::make_shared<FixedTopocentricTimeScaleService>(fixture.ttMinusUtcSeconds, fixture.ut1MinusUtcSeconds);
    auto earthOrientationProvider = makeEarthOrientationProvider(
        fixture.request.epoch,
        fixture.ut1MinusUtcSeconds,
        fixture.polarMotionXArcseconds,
        fixture.polarMotionYArcseconds
    );
    auto frameTransformer = std::make_shared<ErfaFrameTransformer>(timeScaleService, earthOrientationProvider);
    const CelestialFrameTransformResult availabilityResult =
        frameTransformer->transformCelestialVector(CelestialFrameTransformRequest{
            .sourceFrame = CelestialReferenceFrame::Gcrs,
            .targetFrame = CelestialReferenceFrame::Itrs,
            .epoch = fixture.request.epoch,
            .vector = {.x = 1.0, .y = 0.0, .z = 0.0},
        });
    if (!availabilityResult.vector.has_value()) {
        QSKIP("ERFA-backed topocentric validation is not available in this build.");
    }

    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, earthOrientationProvider);
    for (const TopocentricFixtureCase& testCase : fixture.cases) {
        const HighPrecisionCalculatorResult result = calculator.apply(
            makeInput(fixture.request, testCase.body), makeSolarSystemCalculatorResult(testCase.inputGcrsPositionAu)
        );

        QVERIFY2(
            result.equatorial.has_value(),
            qPrintable(QString::fromStdString(testCase.body.id + " did not produce equatorial coordinates"))
        );
        QVERIFY2(
            result.horizontal.has_value(),
            qPrintable(QString::fromStdString(testCase.body.id + " did not produce horizontal coordinates"))
        );

        const EphemerisRaDecExpectation actualEquatorial{
            .rightAscensionHours = result.equatorial->rightAscensionHours,
            .declinationDegrees = result.equatorial->declinationDeg,
        };
        const double angularErrorDegrees = angularSeparationDegrees(actualEquatorial, testCase.expectedEquatorial);
        QVERIFY2(
            angularErrorDegrees <= fixture.angularToleranceDegrees,
            qPrintable(
                QString::fromStdString(testCase.body.id)
                + QStringLiteral(" angular error %1 deg exceeds tolerance %2 deg")
                      .arg(angularErrorDegrees, 0, 'g', 12)
                      .arg(fixture.angularToleranceDegrees, 0, 'g', 12)
            )
        );
        QVERIFY2(
            angularDifferenceDegrees(result.horizontal->azimuthDeg, testCase.expectedHorizontal.azimuthDeg)
                <= fixture.horizontalToleranceDegrees,
            qPrintable(QString::fromStdString(testCase.body.id) + QStringLiteral(" azimuth mismatch"))
        );
        QVERIFY2(
            std::abs(result.horizontal->altitudeDeg - testCase.expectedHorizontal.altitudeDeg)
                <= fixture.horizontalToleranceDegrees,
            qPrintable(QString::fromStdString(testCase.body.id) + QStringLiteral(" altitude mismatch"))
        );
        QCOMPARE(
            static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Valid)
        );
        QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax));
        QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::EarthOrientation));
        QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::PrecessionNutation));
    }
}

void ApparentPlaceCalculatorTests::reportsInvalidObserverForTopocentricRequest()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Topocentric);
    request.context.observer.latitudeDeg = 120.0;

    const HighPrecisionCalculatorResult result = calculator.apply(
        makeInput(request),
        makeSolarSystemCalculatorResult(SolarSystemKernelVector{
            .xAu = 0.0,
            .yAu = 1.0,
            .zAu = 0.0,
        })
    );

    QVERIFY(result.equatorial.has_value());
    QVERIFY(!result.horizontal.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::MissingObserver));
    QVERIFY(!hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax));
}

void ApparentPlaceCalculatorTests::reportsMissingEarthOrientationForTopocentricRequest()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Topocentric);

    const HighPrecisionCalculatorResult result = calculator.apply(
        makeInput(request),
        makeSolarSystemCalculatorResult(SolarSystemKernelVector{
            .xAu = 0.0,
            .yAu = 1.0,
            .zAu = 0.0,
        })
    );

    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.horizontal.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::TimeScaleDataUnavailable));
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax));
}

void ApparentPlaceCalculatorTests::reportsUnavailableParallaxWhenDistanceVectorIsMissing()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::Topocentric);

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QVERIFY(result.equatorial.has_value());
    QVERIFY(result.horizontal.has_value());
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
    QVERIFY(!hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax));
}

void ApparentPlaceCalculatorTests::leavesApparentRequestGeocentricWhenParallaxIsDisabled()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    const EphemerisRequest apparentRequest = makeRequest(EphemerisCorrectionFlags::Apparent);
    EphemerisRequest topocentricRequest = makeRequest(EphemerisCorrectionFlags::Topocentric);
    topocentricRequest.context.observer = {
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };
    const HighPrecisionCalculatorResult calculatorResult =
        makeSolarSystemCalculatorResult(SolarSystemKernelVector{.xAu = 0.0, .yAu = 1.0, .zAu = 0.0});

    const HighPrecisionCalculatorResult apparentResult = calculator.apply(makeInput(apparentRequest), calculatorResult);
    const HighPrecisionCalculatorResult topocentricResult =
        calculator.apply(makeInput(topocentricRequest), calculatorResult);

    QVERIFY(apparentResult.equatorial.has_value());
    QVERIFY(topocentricResult.equatorial.has_value());
    QVERIFY(!apparentResult.horizontal.has_value());
    QVERIFY(topocentricResult.horizontal.has_value());
    QVERIFY(std::abs(apparentResult.equatorial->rightAscensionHours - 6.0) < 1.0e-12);
    QVERIFY(topocentricResult.equatorial->rightAscensionHours > apparentResult.equatorial->rightAscensionHours);
    QVERIFY(!hasCorrectionFlag(apparentResult.metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax));
    QVERIFY(hasCorrectionFlag(topocentricResult.metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax)
    );
}

void ApparentPlaceCalculatorTests::changesTopocentricPositionWhenObserverElevationChanges()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr);
    EphemerisRequest seaLevelRequest = makeRequest(EphemerisCorrectionFlags::Topocentric);
    seaLevelRequest.context.observer = {
        .latitudeDeg = 0.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };
    EphemerisRequest elevatedRequest = seaLevelRequest;
    elevatedRequest.context.observer.elevationMeters = 4'200.0;
    const HighPrecisionCalculatorResult calculatorResult =
        makeSolarSystemCalculatorResult(SolarSystemKernelVector{.xAu = 0.0, .yAu = 1.0, .zAu = 0.0});

    const HighPrecisionCalculatorResult seaLevelResult = calculator.apply(makeInput(seaLevelRequest), calculatorResult);
    const HighPrecisionCalculatorResult elevatedResult = calculator.apply(makeInput(elevatedRequest), calculatorResult);

    QVERIFY(seaLevelResult.equatorial.has_value());
    QVERIFY(elevatedResult.equatorial.has_value());
    QVERIFY(seaLevelResult.horizontal.has_value());
    QVERIFY(elevatedResult.horizontal.has_value());
    QVERIFY(
        elevatedResult.equatorial->rightAscensionHours > seaLevelResult.equatorial->rightAscensionHours
        || elevatedResult.horizontal->altitudeDeg != seaLevelResult.horizontal->altitudeDeg
    );
    QVERIFY(hasCorrectionFlag(elevatedResult.metadata.appliedCorrections, EphemerisCorrectionFlags::DiurnalParallax));
}

void ApparentPlaceCalculatorTests::appliesAtmosphericRefractionToTopocentricHorizontalCoordinates()
{
    auto frameTransformer = std::make_shared<PassThroughFrameTransformer>();
    auto timeScaleService = std::make_shared<ValidUtcTimeScaleService>();
    const auto refractionCalculator = std::make_shared<AtmosphericRefractionCalculator>();
    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, nullptr, refractionCalculator);
    EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::ApparentTopocentric);
    request.options.enableAtmosphericRefraction = true;
    request.context.observer = {
        .latitudeDeg = 45.0,
        .longitudeDeg = 0.0,
        .elevationMeters = 0.0,
    };

    const HighPrecisionCalculatorResult result = calculator.apply(
        makeInput(request),
        makeSolarSystemCalculatorResult(SolarSystemKernelVector{
            .xAu = 1.0,
            .yAu = 0.0,
            .zAu = 1.0,
        })
    );

    QVERIFY(result.horizontal.has_value());
    QVERIFY(result.horizontal->altitudeDeg > 0.0);
    QVERIFY(hasCorrectionFlag(result.metadata.appliedCorrections, EphemerisCorrectionFlags::AtmosphericRefraction));
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::AccuracyDegraded));
}

void ApparentPlaceCalculatorTests::reportsUnavailableRefractionMode()
{
    auto frameTransformer = std::make_shared<RecordingFrameTransformer>();
    const ApparentPlaceCalculator calculator(frameTransformer, nullptr, nullptr);
    EphemerisRequest request = makeRequest(EphemerisCorrectionFlags::AtmosphericRefraction);
    request.options.enableAtmosphericRefraction = true;

    const HighPrecisionCalculatorResult result = calculator.apply(makeInput(request), makeCalculatorResult());

    QCOMPARE(frameTransformer->callCount(), 1);
    QCOMPARE(
        static_cast<std::uint8_t>(frameTransformer->lastTargetFrame()),
        static_cast<std::uint8_t>(CelestialReferenceFrame::Gcrs)
    );
    QCOMPARE(
        static_cast<std::uint8_t>(result.metadata.status), static_cast<std::uint8_t>(EphemerisResultStatus::Degraded)
    );
    QVERIFY(result.metadata.hasWarning(EphemerisWarningCode::CorrectionUnavailable));
}

QTEST_APPLESS_MAIN(ApparentPlaceCalculatorTests)

#include "ApparentPlaceCalculatorTests.moc"
