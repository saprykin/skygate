#pragma once

#include "EphemerisStagedUpdateVerificationStatus.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

struct EphemerisStagedUpdateVerificationResult {
    EphemerisStagedUpdateVerificationStatus status = EphemerisStagedUpdateVerificationStatus::InvalidRequest;
    std::vector<std::string> verifiedAssetIds;
    std::vector<std::string> diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept;
};

}  // namespace skygate::ephemeris
