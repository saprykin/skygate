#include "EphemerisDataStaleness.hpp"

namespace skygate::ephemeris {

bool EphemerisDataStaleness::isStale(
    const std::optional<skygate::core::AstronomicalEpoch>& referenceEpoch,
    const std::optional<skygate::core::AstronomicalEpoch>& expiresAt
) noexcept
{
    if (!referenceEpoch.has_value() || !expiresAt.has_value() || !referenceEpoch->isFiniteUtc()) {
        return false;
    }

    return referenceEpoch->sortKey() > expiresAt->sortKey();
}

}  // namespace skygate::ephemeris
