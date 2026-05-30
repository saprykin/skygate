#include "OwnGalaxyCelestialBody.hpp"
#include "EphemerisFixtureSupport.hpp"
#include "time/CalendarTime.hpp"
#include "engine/highprecision/ApparentPlaceCalculator.hpp"
#include "engine/highprecision/ErfaFrameTransformer.hpp"
#include "engine/highprecision/ICalcephKernelProvider.hpp"
#include "engine/highprecision/SolarSystemStateCalculator.hpp"
#include "engine/highprecision/TimeScaleService.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace skygate::ephemeris;
using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;
using skygate::ephemeris::tests::angularSeparationDegrees;
using skygate::ephemeris::tests::EphemerisRaDecExpectation;
using skygate::ephemeris::tests::EphemerisRaDecFixture;
using skygate::ephemeris::tests::loadRaDecFixture;

[[nodiscard]] CelestialFrameTransformResult transformSingleVector(
    const IFrameTransformer& transformer,
    const CelestialReferenceFrame::Type sourceFrame,
    const CelestialReferenceFrame::Type targetFrame,
    const AstronomicalEpoch& epoch,
    const Vector3d& vector
)
{
    const std::array<Vector3d, 1U> vectors{vector};
    const std::vector<CelestialFrameTransformResult> results = transformer.transform(
        CelestialFrameTransformRequest{
            .sourceFrame = sourceFrame,
            .targetFrame = targetFrame,
            .epoch = epoch,
            .vectors = vectors,
        }
    );
    return results.front();
}

[[nodiscard]] QString apparentFixturePath()
{
    return QStringLiteral(SKYGATE_EPHEMERIS_TESTDATA_DIR "/ephemeris/apparent_solar_system_mars_smoke.json");
}

[[nodiscard]] OwnGalaxyCelestialBody makeMarsBody()
{
    OwnGalaxyCelestialBody body;
    body.id = "mars";
    body.displayName = "Mars";
    body.kind = BaseCelestialBody::Kind::Planet;
    return body;
}

[[nodiscard]] HighPrecisionComputationInput makeInput(const BaseCelestialBody& body, const EphemerisRequest& request)
{
    return {
        .request = request,
        .body = body,
        .bodyIndex = 0U,
    };
}

class FixtureCalcephKernelProvider final : public ICalcephKernelProvider {
public:
    struct Call {
        AstronomicalEpoch epoch;
        int targetNaifId = 0;
        int centerNaifId = 0;
    };

    [[nodiscard]] SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, const int targetNaifId, const int centerNaifId) const override
    {
        calls.push_back({
            .epoch = epoch,
            .targetNaifId = targetNaifId,
            .centerNaifId = centerNaifId,
        });

        const std::pair key{targetNaifId, centerNaifId};
        if (const auto sequence = responseSequences.find(key); sequence != responseSequences.end()) {
            const std::size_t index = responseSequenceIndexes[key]++;
            if (index < sequence->second.size()) {
                return sequence->second[index];
            }
            return sequence->second.back();
        }
        if (const auto match = responses.find(key); match != responses.end()) {
            return match->second;
        }

        SolarSystemKernelStateResult result;
        result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Failed;
        result.metadata.addWarning(EphemerisEngineWarning::Code::MissingEphemerisData);
        result.metadata.dataSourceProvenance = "JPL Horizons apparent fixture";
        return result;
    }

    mutable std::vector<Call> calls;
    mutable std::map<std::pair<int, int>, std::size_t> responseSequenceIndexes;
    std::map<std::pair<int, int>, SolarSystemKernelStateResult> responses;
    std::map<std::pair<int, int>, std::vector<SolarSystemKernelStateResult>> responseSequences;
};

class SameInstantTimeScaleService final : public ITimeScaleService {
public:
    [[nodiscard]] TimeScaleConversionResult
    convert(const AstronomicalEpoch& epoch, const TimeScale targetScale) const override
    {
        TimeScaleConversionResult result;
        result.epoch =
            AstronomicalEpoch{
                .julianDatePart1 = epoch.julianDatePart1,
                .julianDatePart2 = epoch.julianDatePart2,
                .timeScale = targetScale,
            }
                .normalized();
        result.status = TimeScaleConversionStatus::Valid;
        return result;
    }

    [[nodiscard]] TimeScaleConversionResult
    convertCivilDateTime(const CivilDateTime& dateTime, const TimeScale targetScale) const override
    {
        const std::optional<AstronomicalEpoch> epoch = CalendarTime::astronomicalEpochFromCivilDateTime(dateTime);
        if (!epoch.has_value()) {
            TimeScaleConversionResult result;
            result.status = TimeScaleConversionStatus::Failed;
            result.addWarning(TimeScaleConversionWarningCode::UnsupportedConversion);
            return result;
        }
        return convert(*epoch, targetScale);
    }
};

struct ApparentValidationFixture {
    EphemerisRaDecFixture raDec;
    AstronomicalEpoch requestEpoch;
    std::shared_ptr<FixtureCalcephKernelProvider> provider;
};

[[nodiscard]] Vector3d parseVector(const QJsonArray& array)
{
    Q_ASSERT(array.size() == 3);
    return {
        .x = array.at(0).toDouble(),
        .y = array.at(1).toDouble(),
        .z = array.at(2).toDouble(),
    };
}

[[nodiscard]] SolarSystemKernelStateResult makeKernelState(const QJsonObject& object)
{
    SolarSystemKernelStateResult result;
    result.positionAu = parseVector(object.value(QStringLiteral("positionAu")).toArray());
    if (object.contains(QStringLiteral("velocityAuPerDay"))) {
        result.velocityAuPerDay = parseVector(object.value(QStringLiteral("velocityAuPerDay")).toArray());
    }
    result.metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
    result.metadata.dataSourceProvenance = "JPL Horizons apparent fixture";
    return result;
}

[[nodiscard]] std::optional<ApparentValidationFixture> loadApparentValidationFixture(QString* errorText)
{
    auto fail = [errorText](const QString& message) -> std::optional<ApparentValidationFixture> {
        if (errorText != nullptr) {
            *errorText = message;
        }
        return std::nullopt;
    };

    std::optional<EphemerisRaDecFixture> raDec = loadRaDecFixture(apparentFixturePath(), errorText);
    if (!raDec.has_value()) {
        return std::nullopt;
    }

    QFile file(apparentFixturePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("Could not reopen apparent fixture"));
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return fail(QStringLiteral("Apparent fixture is not a JSON object"));
    }
    const QJsonObject root = document.object();
    const QJsonObject request = root.value(QStringLiteral("request")).toObject();
    const auto provider = std::make_shared<FixtureCalcephKernelProvider>();

    const QJsonArray kernelStates = root.value(QStringLiteral("kernelStates")).toArray();
    if (kernelStates.isEmpty()) {
        return fail(QStringLiteral("Apparent fixture has no kernel states"));
    }
    for (const QJsonValue& value : kernelStates) {
        const QJsonObject state = value.toObject();
        const QString role = state.value(QStringLiteral("role")).toString();
        const std::pair key{
            state.value(QStringLiteral("targetNaifId")).toInt(),
            state.value(QStringLiteral("centerNaifId")).toInt(),
        };
        if (role == QStringLiteral("target-retarded")) {
            provider->responseSequences[key].push_back(makeKernelState(state));
        } else {
            provider->responses[key] = makeKernelState(state);
        }
    }

    return ApparentValidationFixture{
        .raDec = *raDec,
        .requestEpoch =
            {
                .julianDatePart1 = request.value(QStringLiteral("julianDateTdb")).toDouble(),
                .julianDatePart2 = 0.0,
                .timeScale = TimeScale::Tdb,
            },
        .provider = provider,
    };
}

}  // namespace

class ApparentRaDecValidationTests final : public QObject {
    Q_OBJECT

private slots:
    void computesGeocentricApparentRaDecAgainstHorizonsFixture();
};

void ApparentRaDecValidationTests::computesGeocentricApparentRaDecAgainstHorizonsFixture()
{
    QString errorText;
    const std::optional<ApparentValidationFixture> loadedFixture = loadApparentValidationFixture(&errorText);
    QVERIFY2(loadedFixture.has_value(), qPrintable(errorText));
    const ApparentValidationFixture& fixture = *loadedFixture;
    QVERIFY(fixture.raDec.metadata.apiParameters.contains(QStringLiteral("QUANTITIES='2'")));
    QVERIFY(fixture.raDec.metadata.sourceFrame.contains(QStringLiteral("apparent"), Qt::CaseInsensitive));

    EphemerisRequest request;
    request.epoch = fixture.requestEpoch;
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(EphemerisCorrectionFlags::apparent());
    request.options.setEnableAtmosphericRefraction(false);

    const OwnGalaxyCelestialBody mars = makeMarsBody();
    const BaseCelestialBody marsCatalogBody(mars);
    const HighPrecisionComputationInput input = makeInput(marsCatalogBody, request);
    const SolarSystemStateCalculator solarSystemCalculator(fixture.provider);
    const auto timeScaleService = std::make_shared<SameInstantTimeScaleService>();
    const auto frameTransformer = std::make_shared<ErfaFrameTransformer>(timeScaleService);
    const CelestialFrameTransformResult availabilityResult = transformSingleVector(
        *frameTransformer,
        CelestialReferenceFrame::Type::Gcrs,
        CelestialReferenceFrame::Type::TrueEquatorAndEquinox,
        fixture.requestEpoch,
        {.x = 1.0, .y = 0.0, .z = 0.0}
    );
    if (!availabilityResult.vector.has_value()) {
        QSKIP("ERFA-backed apparent RA/Dec validation is not available in this build.");
    }

    const ApparentPlaceCalculator apparentPlaceCalculator(frameTransformer, timeScaleService, nullptr);

    const HighPrecisionCalculatorResult astrometricResult = solarSystemCalculator.calculate(input);
    const HighPrecisionCalculatorResult apparentResult = apparentPlaceCalculator.apply(input, astrometricResult);

    QVERIFY(apparentResult.equatorial.has_value());
    const EphemerisRaDecExpectation actual{
        .rightAscensionHours = apparentResult.equatorial->rightAscensionHours,
        .declinationDegrees = apparentResult.equatorial->declinationDeg,
    };
    const double angularErrorDegrees = angularSeparationDegrees(actual, fixture.raDec.expected);
    QVERIFY2(
        angularErrorDegrees <= fixture.raDec.toleranceDegrees,
        qPrintable(QStringLiteral("angular error %1 deg exceeds tolerance %2 deg")
                       .arg(angularErrorDegrees, 0, 'g', 12)
                       .arg(fixture.raDec.toleranceDegrees, 0, 'g', 12))
    );
    QCOMPARE(
        static_cast<std::uint8_t>(apparentResult.metadata.status),
        static_cast<std::uint8_t>(skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid)
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            apparentResult.metadata.appliedCorrections, EphemerisCorrectionFlags::lightTime()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            apparentResult.metadata.appliedCorrections, EphemerisCorrectionFlags::stellarAberration()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            apparentResult.metadata.appliedCorrections, EphemerisCorrectionFlags::gravitationalLightDeflection()
        )
    );
    QVERIFY(
        skygate::ephemeris::EphemerisCorrectionFlags::has(
            apparentResult.metadata.appliedCorrections, EphemerisCorrectionFlags::precessionNutation()
        )
    );
}

QTEST_APPLESS_MAIN(ApparentRaDecValidationTests)

#include "ApparentRaDecValidationTests.moc"
