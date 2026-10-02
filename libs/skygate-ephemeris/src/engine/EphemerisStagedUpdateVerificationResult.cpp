#include "EphemerisStagedUpdateVerificationResult.hpp"

namespace skygate::ephemeris {

bool EphemerisStagedUpdateVerificationResult::isSuccess() const noexcept
{
    return status == EphemerisStagedUpdateVerificationStatus::Verified;
}

}  // namespace skygate::ephemeris
