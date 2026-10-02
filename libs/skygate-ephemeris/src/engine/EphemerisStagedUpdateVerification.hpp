#pragma once

#include "EphemerisStagedUpdateVerificationRequest.hpp"
#include "EphemerisStagedUpdateVerificationResult.hpp"

namespace skygate::ephemeris {

class EphemerisStagedUpdateVerification final {
public:
    [[nodiscard]] static EphemerisStagedUpdateVerificationResult
    verify(const EphemerisStagedUpdateVerificationRequest& request);
};

}  // namespace skygate::ephemeris
