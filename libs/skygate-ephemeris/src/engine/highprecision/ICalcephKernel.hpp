#pragma once

#include "HighPrecisionTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace skygate::ephemeris::highprecision {

class ICalcephKernel {
public:
    enum class Status : std::uint8_t {
        Ready,
        CalcephUnavailable,
        MissingManifestProfile,
        MissingKernelAsset,
        InvalidKernelAsset,
        MissingKernelFile,
        ChecksumMismatch,
        OpenFailed,
        OutOfRange
    };

    struct Info {
        std::string id;
        std::string profileId;
        std::string version;
        std::string sourceUrl;
        std::string provenance;
        std::filesystem::path activePath;
        EphemerisDateRange validityRange;
        bool optional = false;
        bool longRange = false;
    };

    virtual ~ICalcephKernel() = default;

    [[nodiscard]] virtual Status status() const noexcept = 0;
    [[nodiscard]] virtual const std::vector<std::string>& diagnostics() const noexcept = 0;
    [[nodiscard]] virtual const std::optional<Info>& kernelInfo() const noexcept = 0;
    [[nodiscard]] virtual Status statusForEpoch(const AstronomicalEpoch& epoch) const noexcept = 0;
    [[nodiscard]] virtual SolarSystemKernelStateResult
    compute(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const = 0;
};

}  // namespace skygate::ephemeris::highprecision
