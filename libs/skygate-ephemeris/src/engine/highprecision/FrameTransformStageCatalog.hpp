#pragma once

#include "CelestialReferenceFrame.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"

#include <array>
#include <cstdint>

namespace skygate::ephemeris::highprecision {

// Ordered transitions between adjacent celestial reference ranks. Each row
// names the frame pair it connects, the correction the pair applies, and the
// matrix the transform needs for it. Apparent equator and equinox transforms
// name their own row because they connect a geocentric frame with the true
// equator and equinox directly.
class FrameTransformStageCatalog final {
public:
    enum class MatrixKind : std::uint8_t {
        CelestialIntermediate,
        EarthRotation,
        PolarMotion,
        PrecessionNutation,
    };

    struct Row {
        std::uint8_t lowerRank = 0U;
        CelestialReferenceFrame::Type sourceFrame = CelestialReferenceFrame::Type::Gcrs;
        CelestialReferenceFrame::Type targetFrame = CelestialReferenceFrame::Type::Cirs;
        EphemerisCorrectionFlags correction = EphemerisCorrectionFlags::noCorrections();
        MatrixKind matrixKind = MatrixKind::CelestialIntermediate;
    };

    FrameTransformStageCatalog() = delete;

    [[nodiscard]] static constexpr const Row& rowForLowerRank(const std::uint8_t lowerRank) noexcept
    {
        return kRows[lowerRank];
    }

    [[nodiscard]] static constexpr const Row& apparentEquatorAndEquinoxRow() noexcept
    {
        return kApparentEquatorAndEquinoxRow;
    }

private:
    inline static constexpr std::array<Row, 3U> kRows{{
        {
            .lowerRank = 0U,
            .sourceFrame = CelestialReferenceFrame::Type::Gcrs,
            .targetFrame = CelestialReferenceFrame::Type::Cirs,
            .correction = EphemerisCorrectionFlags::precessionNutation(),
            .matrixKind = MatrixKind::CelestialIntermediate,
        },
        {
            .lowerRank = 1U,
            .sourceFrame = CelestialReferenceFrame::Type::Cirs,
            .targetFrame = CelestialReferenceFrame::Type::Tirs,
            .correction = EphemerisCorrectionFlags::earthOrientation(),
            .matrixKind = MatrixKind::EarthRotation,
        },
        {
            .lowerRank = 2U,
            .sourceFrame = CelestialReferenceFrame::Type::Tirs,
            .targetFrame = CelestialReferenceFrame::Type::Itrs,
            .correction = EphemerisCorrectionFlags::earthOrientation(),
            .matrixKind = MatrixKind::PolarMotion,
        },
    }};

    inline static constexpr Row kApparentEquatorAndEquinoxRow{
        .lowerRank = 0U,
        .sourceFrame = CelestialReferenceFrame::Type::Gcrs,
        .targetFrame = CelestialReferenceFrame::Type::TrueEquatorAndEquinox,
        .correction = EphemerisCorrectionFlags::precessionNutation(),
        .matrixKind = MatrixKind::PrecessionNutation,
    };
};

}  // namespace skygate::ephemeris::highprecision
