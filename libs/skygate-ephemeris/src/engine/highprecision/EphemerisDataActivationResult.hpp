#pragma once

#include "EphemerisDataActivationStatus.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace skygate::ephemeris {

struct EphemerisDataActivationResult {
    EphemerisDataActivationStatus status = EphemerisDataActivationStatus::InvalidRequest;
    std::filesystem::path activePath;
    std::vector<std::string> diagnostics;

    [[nodiscard]] bool isSuccess() const noexcept;
};

}  // namespace skygate::ephemeris
