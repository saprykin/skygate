#pragma once

#include <QVariantList>

#include "SkyContextController.hpp"

#include "skygate/core/IProjection.hpp"
#include "skygate/core/Types.hpp"
#include "skygate/ephemeris/ConstellationData.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <vector>

class SkyRenderPointBuilder final {
public:
    [[nodiscard]] std::vector<SkyContextController::SkyRenderPoint> buildPoints(
        const skygate::ephemeris::SkySnapshot& snapshot,
        const skygate::core::IProjection& projection,
        const skygate::core::ProjectionParams& projectionParams,
        double magnitudeCutoff
    ) const;
};

class SkyConstellationRenderBuilder final {
public:
    [[nodiscard]] std::vector<SkyContextController::SkyRenderLine> buildLines(
        const skygate::ephemeris::SkySnapshot& snapshot,
        const skygate::core::IProjection& projection,
        const skygate::core::ProjectionParams& projectionParams,
        const std::vector<skygate::ephemeris::ConstellationLineRef>& lineRefs,
        double viewportWidth,
        double viewportHeight
    ) const;

    [[nodiscard]] QVariantList buildLabels(
        const skygate::ephemeris::SkySnapshot& snapshot,
        const skygate::core::IProjection& projection,
        const skygate::core::ProjectionParams& projectionParams,
        const std::vector<skygate::ephemeris::ConstellationLabelRef>& labelRefs,
        double viewportWidth,
        double viewportHeight
    ) const;
};
