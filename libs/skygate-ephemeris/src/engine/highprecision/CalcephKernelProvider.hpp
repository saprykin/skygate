#pragma once

#include "engine/highprecision/ICalcephKernelProvider.hpp"

#include "engine/highprecision/EphemerisDataManifest.hpp"
#include "engine/highprecision/EphemerisDataSnapshot.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris::highprecision {

enum class CalcephKernelProviderStatus : std::uint8_t {
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

[[nodiscard]] constexpr std::string_view displayName(const CalcephKernelProviderStatus status) noexcept
{
    switch (status) {
    case CalcephKernelProviderStatus::Ready:
        return "ready";
    case CalcephKernelProviderStatus::CalcephUnavailable:
        return "calceph-unavailable";
    case CalcephKernelProviderStatus::MissingManifestProfile:
        return "missing-manifest-profile";
    case CalcephKernelProviderStatus::MissingKernelAsset:
        return "missing-kernel-asset";
    case CalcephKernelProviderStatus::InvalidKernelAsset:
        return "invalid-kernel-asset";
    case CalcephKernelProviderStatus::MissingKernelFile:
        return "missing-kernel-file";
    case CalcephKernelProviderStatus::ChecksumMismatch:
        return "checksum-mismatch";
    case CalcephKernelProviderStatus::OpenFailed:
        return "open-failed";
    case CalcephKernelProviderStatus::OutOfRange:
        return "out-of-range";
    }

    return {};
}

struct CalcephKernelSelectionOptions {
    std::string preferredProfileId;
    bool preferLongRange = false;
    bool verifyChecksum = true;
};

struct CalcephKernelInfo {
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

class ICalcephKernelHandle {
public:
    virtual ~ICalcephKernelHandle() = default;

    [[nodiscard]] virtual std::optional<SolarSystemKernelVector>
    computeGeometricState(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const
    {
        static_cast<void>(epoch);
        static_cast<void>(targetNaifId);
        static_cast<void>(centerNaifId);
        return std::nullopt;
    }

    [[nodiscard]] virtual SolarSystemKernelStateResult
    computeGeometricStateWithVelocity(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const
    {
        SolarSystemKernelStateResult result;
        result.positionAu = computeGeometricState(epoch, targetNaifId, centerNaifId);
        return result;
    }
};

struct CalcephKernelOpenResult {
    std::unique_ptr<ICalcephKernelHandle> handle;
    std::string diagnostic;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return handle != nullptr;
    }
};

class ICalcephKernelRuntime {
public:
    virtual ~ICalcephKernelRuntime() = default;

    [[nodiscard]] virtual bool isAvailable() const noexcept = 0;
    [[nodiscard]] virtual CalcephKernelOpenResult openKernel(const std::filesystem::path& path) const = 0;
};

class CalcephKernelProvider final : public ICalcephKernelProvider {
public:
    CalcephKernelProvider(
        const IEphemerisDataSnapshot& snapshot,
        const EphemerisDataManifest& manifest,
        CalcephKernelSelectionOptions options = {},
        std::shared_ptr<const ICalcephKernelRuntime> runtime = {}
    );
    ~CalcephKernelProvider() override;

    CalcephKernelProvider(const CalcephKernelProvider&) = delete;
    CalcephKernelProvider& operator=(const CalcephKernelProvider&) = delete;
    CalcephKernelProvider(CalcephKernelProvider&&) noexcept;
    CalcephKernelProvider& operator=(CalcephKernelProvider&&) noexcept;

    [[nodiscard]] CalcephKernelProviderStatus status() const noexcept;
    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] const std::vector<std::string>& diagnostics() const noexcept;
    [[nodiscard]] const std::optional<CalcephKernelInfo>& kernelInfo() const noexcept;
    [[nodiscard]] CalcephKernelProviderStatus statusForEpoch(const AstronomicalEpoch& epoch) const noexcept;
    [[nodiscard]] SolarSystemKernelStateResult
    computeGeometricState(const AstronomicalEpoch& epoch, int targetNaifId, int centerNaifId) const override;

private:
    CalcephKernelProviderStatus m_status = CalcephKernelProviderStatus::MissingKernelAsset;
    std::vector<std::string> m_diagnostics;
    std::optional<CalcephKernelInfo> m_kernelInfo;
    std::unique_ptr<ICalcephKernelHandle> m_kernelHandle;
};

[[nodiscard]] std::shared_ptr<const ICalcephKernelRuntime> defaultCalcephKernelRuntime();

}  // namespace skygate::ephemeris::highprecision
