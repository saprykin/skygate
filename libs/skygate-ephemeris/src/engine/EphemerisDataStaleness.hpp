#pragma once

#include "time/AstronomicalEpoch.hpp"

#include <optional>

namespace skygate::ephemeris {

class EphemerisDataStaleness final {
public:
    EphemerisDataStaleness() = delete;

    [[nodiscard]] static bool isStale(
        const std::optional<skygate::core::AstronomicalEpoch>& referenceEpoch,
        const std::optional<skygate::core::AstronomicalEpoch>& expiresAt
    ) noexcept;
};

}  // namespace skygate::ephemeris
