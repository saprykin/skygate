#include "CelestialFrameTransformResult.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"

#include <utility>

namespace skygate::ephemeris::highprecision {

CelestialFrameTransformResult CelestialFrameTransformResult::makeFailed(
    const EphemerisEngineWarning::Code warningCode, const std::string_view dataSourceProvenance
)
{
    CelestialFrameTransformResult result;
    result.metadata.status = EphemerisEngineQueryStatus::Type::Failed;
    result.metadata.addWarning(warningCode);
    result.metadata.dataSourceProvenance = dataSourceProvenance;
    return result;
}

CelestialFrameTransformResult CelestialFrameTransformResult::makeIdentity(
    const CelestialReferenceFrame::Type sourceFrame,
    const CelestialReferenceFrame::Type targetFrame,
    const skygate::core::Vector3d& vector,
    const std::string_view dataSourceProvenance
)
{
    CelestialFrameTransformResult result;
    result.vector = vector;
    result.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
    result.metadata.appliedCorrections = EphemerisCorrectionFlags::noCorrections();
    result.metadata.dataSourceProvenance = dataSourceProvenance;
    if (sourceFrame != targetFrame) {
        CelestialFrameTransformResult::Stage stage;
        stage.sourceFrame = sourceFrame;
        stage.targetFrame = targetFrame;
        stage.applied = false;
        stage.metadata.status = EphemerisEngineQueryStatus::Type::Valid;
        stage.metadata.appliedCorrections = EphemerisCorrectionFlags::noCorrections();
        stage.metadata.dataSourceProvenance = dataSourceProvenance;
        result.stages.push_back(std::move(stage));
    }
    return result;
}

}  // namespace skygate::ephemeris::highprecision
