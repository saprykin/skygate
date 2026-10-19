#pragma once

#include "CelestialReferenceFrame.hpp"
#include "engine/EphemerisCorrectionFlags.hpp"

#include <cstdint>

namespace skygate::ephemeris::highprecision {

// Reads a request's correction flags to decide how far the apparent place
// passes have to go: which frame the primary transform targets and which
// correction a missing transform has to report.
class ApparentPlaceRequestPolicy final {
public:
    enum class Mode : std::uint8_t {
        Geometric,
        Astrometric,
        Apparent,
        Topocentric
    };

    ApparentPlaceRequestPolicy() = delete;

    [[nodiscard]] static constexpr Mode modeForCorrections(const EphemerisCorrectionFlags flags) noexcept
    {
        if (!flags.hasCorrections()) {
            return Mode::Geometric;
        }
        if (EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::diurnalParallax())) {
            return Mode::Topocentric;
        }
        if (EphemerisCorrectionFlags::has(flags, EphemerisCorrectionFlags::precessionNutation())) {
            return Mode::Apparent;
        }

        return Mode::Astrometric;
    }

    // Geometric and astrometric answers stay in GCRS, an apparent answer lands
    // on the true equator and equinox, and a topocentric answer lands on ITRS.
    [[nodiscard]] static constexpr CelestialReferenceFrame::Type targetFrame(const Mode mode) noexcept
    {
        switch (mode) {
        case Mode::Geometric:
        case Mode::Astrometric:
            return CelestialReferenceFrame::Type::Gcrs;
        case Mode::Apparent:
            return CelestialReferenceFrame::Type::TrueEquatorAndEquinox;
        case Mode::Topocentric:
            return CelestialReferenceFrame::Type::Itrs;
        }

        return CelestialReferenceFrame::Type::Gcrs;
    }

    // The primary transform always rotates with precession and nutation. A
    // transform that reaches the terrestrial frame also rotates with Earth
    // orientation.
    [[nodiscard]] static constexpr EphemerisCorrectionFlags
    primaryTransformUnavailableCorrection(const CelestialReferenceFrame::Type frame) noexcept
    {
        EphemerisCorrectionFlags correction = EphemerisCorrectionFlags::precessionNutation();
        if (frame == CelestialReferenceFrame::Type::Itrs) {
            correction |= EphemerisCorrectionFlags::earthOrientation();
        }

        return correction;
    }
};

}  // namespace skygate::ephemeris::highprecision
