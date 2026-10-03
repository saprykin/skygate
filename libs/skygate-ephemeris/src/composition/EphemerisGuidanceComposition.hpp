#pragma once

#include "engine/IEphemerisGuidanceStrategy.hpp"

#include <memory>

namespace skygate::ephemeris {

/// Resolves the concrete default guidance strategy at composition time.
///
/// The algorithm layer only depends on IEphemerisGuidanceStrategy; the
/// SimpleEphemerisGuidanceStrategy default is selected here so calculators can
/// treat a null strategy as "no guidance".
class EphemerisGuidanceComposition final {
public:
    EphemerisGuidanceComposition() = delete;

    [[nodiscard]] static std::shared_ptr<IEphemerisGuidanceStrategy> defaultGuidanceStrategy();
};

}  // namespace skygate::ephemeris
