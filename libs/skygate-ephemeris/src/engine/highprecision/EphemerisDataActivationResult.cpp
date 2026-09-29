#include "EphemerisDataActivationResult.hpp"

namespace skygate::ephemeris {

bool EphemerisDataActivationResult::isSuccess() const noexcept
{
    return status == EphemerisDataActivationStatus::Activated || status == EphemerisDataActivationStatus::AlreadyActive;
}

}  // namespace skygate::ephemeris
