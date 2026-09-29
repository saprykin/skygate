#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>

namespace skygate::ephemeris {

struct EphemerisDataManifestAsset;

struct EphemerisDataActivationRequest {
    const EphemerisDataManifestAsset* asset = nullptr;
    std::filesystem::path bundledResourceRoot;
    std::filesystem::path writableCacheRoot;
    bool allowQtResourceKernelAssets = false;
    std::uint64_t largeKernelResourceThresholdBytes = 128ULL * 1024ULL * 1024ULL;
    std::function<bool()> cancellationRequested;
};

}  // namespace skygate::ephemeris
