#include "AstronomicalEpochRange.hpp"

namespace skygate::core {

bool AstronomicalEpochRange::isValid() const noexcept
{
    return start.isFinite() && end.isFinite() && start.sortKey() <= end.sortKey();
}

bool AstronomicalEpochRange::isEmpty() const noexcept
{
    return !start.hasExplicit() && !end.hasExplicit();
}

bool AstronomicalEpochRange::contains(const AstronomicalEpoch& epoch) const noexcept
{
    if (isEmpty() || !isValid() || !epoch.isFinite()) {
        return false;
    }

    const double key = epoch.sortKey();
    return key >= start.sortKey() && key <= end.sortKey();
}

bool AstronomicalEpochRange::contains(const AstronomicalEpochRange& other) const noexcept
{
    if (isEmpty() || !isValid() || other.isEmpty() || !other.isValid()) {
        return false;
    }

    return contains(other.start) && contains(other.end);
}

}  // namespace skygate::core
