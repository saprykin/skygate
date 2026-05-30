#pragma once

#include "CelestialReferenceFrame.hpp"
#include "engine/EphemerisEngineQueryResult.hpp"
#include "engine/EphemerisEngineWarning.hpp"
#include "math/Vector3d.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace skygate::ephemeris::highprecision {

struct CelestialFrameTransformResult {
    struct Stage {
        CelestialReferenceFrame::Type sourceFrame = CelestialReferenceFrame::Type::Gcrs;
        CelestialReferenceFrame::Type targetFrame = CelestialReferenceFrame::Type::Cirs;
        bool applied = false;
        EphemerisEngineQueryResult metadata;
    };

    [[nodiscard]] static CelestialFrameTransformResult
    makeFailed(EphemerisEngineWarning::Code warningCode, std::string_view dataSourceProvenance);
    [[nodiscard]] static CelestialFrameTransformResult makeIdentity(
        CelestialReferenceFrame::Type sourceFrame,
        CelestialReferenceFrame::Type targetFrame,
        const skygate::core::Vector3d& vector,
        std::string_view dataSourceProvenance
    );

    std::optional<skygate::core::Vector3d> vector;
    EphemerisEngineQueryResult metadata;
    std::vector<Stage> stages;
};

}  // namespace skygate::ephemeris::highprecision
