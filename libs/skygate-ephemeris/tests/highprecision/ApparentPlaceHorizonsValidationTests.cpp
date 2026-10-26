#include "ApparentPlaceTestFixture.hpp"
#include "EphemerisFixtureSupport.hpp"
#include "EphemerisRequest.hpp"
#include "HorizontalCoordinate.hpp"
#include "OwnGalaxyCelestialBody.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "engine/IEarthOrientationProvider.hpp"
#include "engine/ITimeScaleService.hpp"
#include "engine/TableBackedEarthOrientationProvider.hpp"
#include "engine/TimeScaleConversionResult.hpp"
#include "engine/TimeScaleConversionStatus.hpp"
#include "math/Vector3d.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/TimeScale.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/CelestialFrameTransformResult.hpp"
#include "engine/highprecision/CelestialReferenceFrame.hpp"
#include "engine/highprecision/ErfaFrameTransformer.hpp"
#include "engine/highprecision/HighPrecisionCalculatorResult.hpp"
#include "engine/highprecision/IFrameTransformer.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QtTest/QtTest>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using skygate::ephemeris::tests::angularSeparationDegrees;
using skygate::ephemeris::tests::ApparentPlaceTestFixture;
using skygate::ephemeris::tests::EphemerisRaDecExpectation;
using namespace skygate::core;

constexpr double kSecondsPerDay = 86'400.0;

struct TopocentricFixtureCase {
    OwnGalaxyCelestialBody body;
    Vector3d inputGcrsPositionAu;
    EphemerisRaDecExpectation expectedEquatorial;
    HorizontalCoordinate expectedHorizontal;
};

struct TopocentricFixture {
    QString source;
    QString apiParameters;
    QString sourceFrame;
    QString observer;
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
    return QStringLiteral(
        SKYGATE_EPHEMERIS_TESTDATA_DIR "/ephemeris/topocentric_observer_apparent_solar_system_smoke.json"
    );
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

[[nodiscard]] Vector3d parseKernelVector(const QJsonArray& array)
{
    Q_ASSERT(array.size() == 3);
    return {
        .x = array.at(0).toDouble(),
        .y = array.at(1).toDouble(),
        .z = array.at(2).toDouble(),
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
    const QJsonObject metadataObject = root.value(QStringLiteral("metadata")).toObject();
    const QJsonObject requestObject = root.value(QStringLiteral("request")).toObject();
    const QJsonObject toleranceObject = root.value(QStringLiteral("tolerance")).toObject();
    const QJsonArray cases = root.value(QStringLiteral("cases")).toArray();
    if (cases.isEmpty()) {
        return fail(QStringLiteral("Topocentric fixture has no cases"));
    }

    TopocentricFixture fixture;
    fixture.source = metadataObject.value(QStringLiteral("source")).toString();
    fixture.apiParameters = metadataObject.value(QStringLiteral("apiParameters")).toString();
    fixture.sourceFrame = metadataObject.value(QStringLiteral("sourceFrame")).toString();
    fixture.observer = metadataObject.value(QStringLiteral("observer")).toString();
    fixture.request.context.observer = {
        .latitudeDeg = requestObject.value(QStringLiteral("latitudeDegrees")).toDouble(),
        .longitudeDeg = requestObject.value(QStringLiteral("longitudeDegrees")).toDouble(),
        .elevationMeters = requestObject.value(QStringLiteral("elevationMeters")).toDouble(),
    };
    fixture.request.epoch = {
        .julianDatePart1 = requestObject.value(QStringLiteral("utcJulianDate")).toDouble(),
        .julianDatePart2 = 0.0,
        .timeScale = skygate::core::TimeScale::Utc,
    };
    fixture.request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    fixture.request.options.setCorrectionFlags(EphemerisCorrectionFlags::topocentric());
    fixture.request.options.setEnableAtmosphericRefraction(false);
    fixture.ttMinusUtcSeconds = requestObject.value(QStringLiteral("ttMinusUtcSeconds")).toDouble();
    fixture.ut1MinusUtcSeconds = requestObject.value(QStringLiteral("ut1MinusUtcSeconds")).toDouble();
    fixture.polarMotionXArcseconds = requestObject.value(QStringLiteral("polarMotionXArcseconds")).toDouble();
    fixture.polarMotionYArcseconds = requestObject.value(QStringLiteral("polarMotionYArcseconds")).toDouble();
    fixture.angularToleranceDegrees = toleranceObject.value(QStringLiteral("angularDegrees")).toDouble();
    fixture.horizontalToleranceDegrees = toleranceObject.value(QStringLiteral("horizontalDegrees")).toDouble();

    for (const QJsonValue& value : cases) {
        const QJsonObject object = value.toObject();
        const QJsonObject expected = object.value(QStringLiteral("expected")).toObject();
        fixture.cases.push_back(
            TopocentricFixtureCase{
                .body = ApparentPlaceTestFixture::body(
                    object.value(QStringLiteral("bodyId")).toString().toStdString(),
                    object.value(QStringLiteral("bodyType")).toString().toStdString()
                ),
                .inputGcrsPositionAu = parseKernelVector(object.value(QStringLiteral("inputGcrsPositionAu")).toArray()),
                .expectedEquatorial =
                    {
                        .rightAscensionHours = expected.value(QStringLiteral("rightAscensionHours")).toDouble(),
                        .declinationDegrees = expected.value(QStringLiteral("declinationDegrees")).toDouble(),
                    },
                .expectedHorizontal = {
                    .altitudeDeg = expected.value(QStringLiteral("altitudeDegrees")).toDouble(),
                    .azimuthDeg = expected.value(QStringLiteral("azimuthDegrees")).toDouble(),
                },
            }
        );
    }

    return fixture;
}

class FixedTopocentricTimeScaleService final : public ITimeScaleService {
public:
    FixedTopocentricTimeScaleService(const double ttMinusUtcSeconds, const double ut1MinusUtcSeconds)
        : m_ttMinusUtcSeconds(ttMinusUtcSeconds), m_ut1MinusUtcSeconds(ut1MinusUtcSeconds)
    {
    }

    [[nodiscard]] TimeScaleConversionResult
    convert(const skygate::core::AstronomicalEpoch& epoch, const skygate::core::TimeScale targetScale) const override
    {
        TimeScaleConversionResult result;
        result.status = TimeScaleConversionStatus::Type::Valid;
        result.diagnosticText = "unit test fixed UTC conversion";
        result.epoch = epoch;
        result.epoch.timeScale = targetScale;
        if (epoch.timeScale == skygate::core::TimeScale::Utc && targetScale == skygate::core::TimeScale::Tt) {
            result.epoch.julianDatePart2 = addSecondsToJulianDatePart2(epoch.julianDatePart2, m_ttMinusUtcSeconds);
        } else if (epoch.timeScale == skygate::core::TimeScale::Utc && targetScale == skygate::core::TimeScale::Ut1) {
            result.epoch.julianDatePart2 = addSecondsToJulianDatePart2(epoch.julianDatePart2, m_ut1MinusUtcSeconds);
        }
        result.epoch = result.epoch.normalized();
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult convertCivilDateTime(
        const skygate::core::CivilDateTime& dateTime, const skygate::core::TimeScale targetScale
    ) const override
    {
        static_cast<void>(dateTime);
        static_cast<void>(targetScale);
        return {};
    }

private:
    double m_ttMinusUtcSeconds = 0.0;
    double m_ut1MinusUtcSeconds = 0.0;
};

[[nodiscard]] std::shared_ptr<const IEarthOrientationProvider> makeEarthOrientationProvider(
    const skygate::core::AstronomicalEpoch& utcEpoch,
    const double ut1MinusUtcSeconds,
    const double polarMotionXArcseconds,
    const double polarMotionYArcseconds
)
{
    IEarthOrientationProvider::DataInfo info;
    info.version = "unit-test-topocentric-eop";
    info.provenance = "JPL Horizons topocentric validation fixture";
    info.status = IEarthOrientationProvider::DataStatus::Available;
    info.diagnosticText = "Earth-orientation data loaded.";

    IEarthOrientationProvider::TableEntry entry;
    entry.effectiveUtcEpoch = utcEpoch;
    entry.ut1MinusUtcSeconds = ut1MinusUtcSeconds;
    entry.polarMotionXArcseconds = polarMotionXArcseconds;
    entry.polarMotionYArcseconds = polarMotionYArcseconds;

    return std::make_shared<TableBackedEarthOrientationProvider>(
        std::move(info), std::vector<IEarthOrientationProvider::TableEntry>{entry}
    );
}

}  // namespace

class ApparentPlaceHorizonsValidationTests final : public QObject {
    Q_OBJECT

private slots:
    void validatesMoonSunPlanetAgainstHorizonsFixture();
};

void ApparentPlaceHorizonsValidationTests::validatesMoonSunPlanetAgainstHorizonsFixture()
{
    QString errorText;
    const std::optional<TopocentricFixture> loadedFixture = loadTopocentricFixture(&errorText);
    QVERIFY2(loadedFixture.has_value(), qPrintable(errorText));
    const TopocentricFixture& fixture = *loadedFixture;
    QCOMPARE(fixture.source, QStringLiteral("JPL Horizons"));
    QVERIFY(fixture.apiParameters.contains(QStringLiteral("EPHEM_TYPE=OBSERVER")));
    QVERIFY(fixture.apiParameters.contains(QStringLiteral("APPARENT=AIRLESS")));
    QVERIFY(fixture.sourceFrame.contains(QStringLiteral("observer apparent topocentric"), Qt::CaseInsensitive));
    QVERIFY(fixture.observer.contains(QStringLiteral("San Francisco"), Qt::CaseInsensitive));
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
    const CelestialFrameTransformResult availabilityResult = ApparentPlaceTestFixture::transformSingleVector(
        *frameTransformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::Itrs,
        fixture.request.epoch,
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );
    if (!availabilityResult.vector.has_value()) {
        QSKIP("ERFA-backed topocentric validation is not available in this build.");
    }

    const ApparentPlaceCalculator calculator(frameTransformer, timeScaleService, earthOrientationProvider);
    for (const TopocentricFixtureCase& testCase : fixture.cases) {
        const HighPrecisionCalculatorResult result = calculator.apply(
            ApparentPlaceTestFixture::input(fixture.request, testCase.body),
            ApparentPlaceTestFixture::solarSystemCalculatorResult(testCase.inputGcrsPositionAu)
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
            static_cast<std::uint8_t>(result.metadata.status),
            static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
        );
        QVERIFY(
            skygate::ephemeris::EphemerisCorrectionFlags::has(
                result.metadata.appliedCorrections, EphemerisCorrectionFlags::diurnalParallax()
            )
        );
        QVERIFY(
            skygate::ephemeris::EphemerisCorrectionFlags::has(
                result.metadata.appliedCorrections, EphemerisCorrectionFlags::earthOrientation()
            )
        );
        QVERIFY(
            skygate::ephemeris::EphemerisCorrectionFlags::has(
                result.metadata.appliedCorrections, EphemerisCorrectionFlags::precessionNutation()
            )
        );
    }
}

QTEST_APPLESS_MAIN(ApparentPlaceHorizonsValidationTests)

#include "ApparentPlaceHorizonsValidationTests.moc"
