#include "EphemerisDateRange.hpp"

namespace skygate::ephemeris {

bool EphemerisDateRange::isValid() const noexcept
{
    return start.isFinite() && end.isFinite() && start.sortKey() <= end.sortKey();
}

bool EphemerisDateRange::contains(const skygate::core::AstronomicalEpoch& epoch) const noexcept
{
    if (!epoch.isFinite() || !isValid()) {
        return false;
    }

    const double key = epoch.sortKey();
    return key >= start.sortKey() && key <= end.sortKey();
}

}  // namespace skygate::ephemeris
