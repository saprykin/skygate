#include "ApparentPlaceTestFixture.hpp"

#include "HighPrecisionEngineFixture.hpp"
#include "engine/EphemerisEngineQueryStatus.hpp"
#include "time/TimeScale.hpp"
#include "engine/highprecision/CelestialFrameTransformRequest.hpp"
#include "engine/highprecision/EphemerisMetadataMerger.hpp"

#include <array>
#include <string>
#include <vector>

namespace skygate::ephemeris::tests {

using namespace skygate::ephemeris::highprecision;
using namespace skygate::core;

namespace {

[[nodiscard]] EphemerisEngineQueryResult
frameTransformMetadata(const CelestialReferenceFrame::Type sourceFrame, const CelestialReferenceFrame::Type targetFrame)
{
    EphemerisEngineQueryResult metadata;
    metadata.status = skygate::ephemeris::EphemerisEngineQueryStatus::Type::Valid;
    if (sourceFrame == CelestialReferenceFrame::Type::Gcrs
        && targetFrame == CelestialReferenceFrame::Type::TrueEquatorAndEquinox) {
        EphemerisMetadataMerger::markCorrectionApplied(metadata, EphemerisCorrectionFlags::precessionNutation());
    } else if (sourceFrame != targetFrame) {
        EphemerisMetadataMerger::markCorrectionApplied(metadata, EphemerisCorrectionFlags::earthOrientation());
    }

    return metadata;
}

}  // namespace

EphemerisRequest ApparentPlaceTestFixture::request(const EphemerisCorrectionFlags correctionFlags)
{
    EphemerisRequest request;
    request.context = HighPrecisionEngineFixture::context();
    request.epoch = {
        .julianDatePart1 = 2'460'310.0,
        .julianDatePart2 = 0.5,
        .timeScale = skygate::core::TimeScale::Tdb,
    };
    request.options.setEngineKind(skygate::ephemeris::EphemerisEngineKind::Type::HighPrecision);
    request.options.setCorrectionFlags(correctionFlags);
    request.options.setEnableAtmosphericRefraction(false);
    return request;
}

OwnGalaxyCelestialBody ApparentPlaceTestFixture::sunBody()
{
    return body("sun", "sun");
}

OwnGalaxyCelestialBody ApparentPlaceTestFixture::body(const std::string_view id, const std::string_view bodyType)
{
    OwnGalaxyCelestialBody body;
    body.id = std::string(id);
    if (bodyType == "sun") {
        body.displayName = "Sun";
        body.kind = BaseCelestialBody::Kind::Sun;
        return body;
    }
    if (bodyType == "moon") {
        body.displayName = "Moon";
        body.kind = BaseCelestialBody::Kind::Moon;
        return body;
    }

    body.displayName = body.id;
    body.kind = BaseCelestialBody::Kind::Planet;
    return body;
}

HighPrecisionComputationInput ApparentPlaceTestFixture::input(const EphemerisRequest& request)
{
    static const OwnGalaxyCelestialBody kBody = sunBody();
    return {
        .request = request,
        .body = kBody,
        .bodyIndex = 0U,
    };
}

HighPrecisionComputationInput
ApparentPlaceTestFixture::input(const EphemerisRequest& request, const BaseCelestialBody& body)
{
    return {
        .request = request,
        .body = body,
        .bodyIndex = 0U,
    };
}

HighPrecisionCalculatorResult ApparentPlaceTestFixture::calculatorResult()
{
    HighPrecisionCalculatorResult result;
    result.equatorial = EquatorialCoordinate{
        .rightAscensionHours = 0.0,
        .declinationDeg = 0.0,
    };
    result.metadata.dataSourceProvenance = "unit-test source";
    return result;
}

HighPrecisionCalculatorResult ApparentPlaceTestFixture::solarSystemCalculatorResult(const Vector3d& positionAu)
{
    HighPrecisionCalculatorResult result = calculatorResult();
    result.observerRelativePositionAu = positionAu;
    return result;
}

CelestialFrameTransformResult ApparentPlaceTestFixture::frameTransformResult(
    const CelestialReferenceFrame::Type sourceFrame,
    const CelestialReferenceFrame::Type targetFrame,
    const Vector3d& vector
)
{
    return {
        .vector = vector,
        .metadata = frameTransformMetadata(sourceFrame, targetFrame),
    };
}

CelestialFrameTransformResult ApparentPlaceTestFixture::transformSingleVector(
    const IFrameTransformer& transformer,
    const CelestialReferenceFrame::Type sourceFrame,
    const CelestialReferenceFrame::Type targetFrame,
    const skygate::core::AstronomicalEpoch& epoch,
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

}  // namespace skygate::ephemeris::tests
