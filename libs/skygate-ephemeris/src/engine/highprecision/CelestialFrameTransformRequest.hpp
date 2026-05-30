#pragma once

#include "CelestialReferenceFrame.hpp"
#include "math/Vector3d.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <span>

namespace skygate::ephemeris::highprecision {

struct CelestialFrameTransformRequest {
    CelestialReferenceFrame::Type sourceFrame = CelestialReferenceFrame::Type::Gcrs;
    CelestialReferenceFrame::Type targetFrame = CelestialReferenceFrame::Type::Cirs;
    AstronomicalEpoch epoch;
    std::span<const skygate::core::Vector3d> vectors;
};

}  // namespace skygate::ephemeris::highprecision
