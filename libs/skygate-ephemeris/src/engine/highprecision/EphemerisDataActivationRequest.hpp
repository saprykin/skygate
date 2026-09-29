#pragma once

#include "engine/highprecision/EphemerisDataManifest.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>

namespace skygate::ephemeris {

struct EphemerisDataActivationRequest {
    const EphemerisDataManifest::Asset* asset = nullptr;
    std::filesystem::path bundledResourceRoot;
    std::filesystem::path writableCacheRoot;
    bool allowQtResourceKernelAssets = false;
    std::uint64_t largeKernelResourceThresholdBytes = 128ULL * 1024ULL * 1024ULL;
    std::function<bool()> cancellationRequested;
};

}  // namespace skygate::ephemeris
