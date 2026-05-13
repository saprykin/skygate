#pragma once

#include "skygate/ephemeris/EphemerisDataSnapshot.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

enum class EarthOrientationDataStatus : std::uint8_t {
    Available,
    Missing,
    Malformed,
    Stale
};

[[nodiscard]] constexpr std::string_view displayName(const EarthOrientationDataStatus status) noexcept
{
    switch (status) {
    case EarthOrientationDataStatus::Available:
        return "available";
    case EarthOrientationDataStatus::Missing:
        return "missing";
    case EarthOrientationDataStatus::Malformed:
        return "malformed";
    case EarthOrientationDataStatus::Stale:
        return "stale";
    }

    return {};
}

struct EarthOrientationTableEntry {
    CivilDateTime effectiveUtcDate;
    AstronomicalEpoch effectiveUtcEpoch;
    double ut1MinusUtcSeconds = 0.0;
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    bool predicted = false;
};

enum class EarthOrientationSampleStatus : std::uint8_t {
    Valid,
    Degraded,
    Failed
};

[[nodiscard]] constexpr std::string_view displayName(const EarthOrientationSampleStatus status) noexcept
{
    switch (status) {
    case EarthOrientationSampleStatus::Valid:
        return "valid";
    case EarthOrientationSampleStatus::Degraded:
        return "degraded";
    case EarthOrientationSampleStatus::Failed:
        return "failed";
    }

    return {};
}

enum class EarthOrientationSampleWarningCode : std::uint8_t {
    StaleData,
    PredictedData,
    MissingData,
    EpochOutsideRange,
    InvalidInput
};

[[nodiscard]] constexpr std::string_view earthOrientationSampleWarningText(const EarthOrientationSampleWarningCode code
) noexcept
{
    switch (code) {
    case EarthOrientationSampleWarningCode::StaleData:
        return "Earth-orientation data is stale for the requested epoch.";
    case EarthOrientationSampleWarningCode::PredictedData:
        return "Earth-orientation data uses a prediction for the requested epoch.";
    case EarthOrientationSampleWarningCode::MissingData:
        return "Earth-orientation data is unavailable.";
    case EarthOrientationSampleWarningCode::EpochOutsideRange:
        return "The requested epoch is outside the Earth-orientation data range.";
    case EarthOrientationSampleWarningCode::InvalidInput:
        return "The requested Earth-orientation input is invalid.";
    }

    return "Earth-orientation warning.";
}

[[nodiscard]] constexpr std::uint32_t earthOrientationSampleWarningMask(const EarthOrientationSampleWarningCode code
) noexcept
{
    return 1U << static_cast<std::uint8_t>(code);
}

struct EarthOrientationSampleOptions {
    bool allowOutOfRangeNearestSampleFallback = true;
    bool allowMissingDataZeroFallback = false;
};

struct EarthOrientationSample {
    AstronomicalEpoch requestedUtcEpoch;
    double ut1MinusUtcSeconds = 0.0;
    double polarMotionXArcseconds = 0.0;
    double polarMotionYArcseconds = 0.0;
    bool predicted = false;
    EarthOrientationSampleStatus status = EarthOrientationSampleStatus::Failed;
    std::uint32_t warningCodeMask = 0U;
    std::string diagnosticText;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return status == EarthOrientationSampleStatus::Valid || status == EarthOrientationSampleStatus::Degraded;
    }

    void addWarning(const EarthOrientationSampleWarningCode code) noexcept
    {
        warningCodeMask |= earthOrientationSampleWarningMask(code);
    }

    [[nodiscard]] bool hasWarning(const EarthOrientationSampleWarningCode code) const noexcept
    {
        return (warningCodeMask & earthOrientationSampleWarningMask(code)) != 0U;
    }
};

struct EarthOrientationDataInfo {
    std::string version;
    std::string provenance;
    EarthOrientationDataStatus status = EarthOrientationDataStatus::Missing;
    std::string diagnosticText;
    std::optional<EphemerisDateRange> validityRange;
    std::optional<EphemerisDateRange> predictionRange;
    std::optional<AstronomicalEpoch> expiresAt;

    [[nodiscard]] bool isUsable() const noexcept
    {
        return status == EarthOrientationDataStatus::Available || status == EarthOrientationDataStatus::Stale;
    }
};

class IEarthOrientationProvider {
public:
    virtual ~IEarthOrientationProvider() = default;

    [[nodiscard]] virtual const EarthOrientationDataInfo& dataInfo() const noexcept = 0;
    [[nodiscard]] virtual std::span<const EarthOrientationTableEntry> entries() const noexcept = 0;
};

class TableBackedEarthOrientationProvider final : public IEarthOrientationProvider {
public:
    TableBackedEarthOrientationProvider(
        EarthOrientationDataInfo dataInfo, std::vector<EarthOrientationTableEntry> entries
    );

    [[nodiscard]] const EarthOrientationDataInfo& dataInfo() const noexcept override;
    [[nodiscard]] std::span<const EarthOrientationTableEntry> entries() const noexcept override;

private:
    EarthOrientationDataInfo m_dataInfo;
    std::vector<EarthOrientationTableEntry> m_entries;
};

struct EarthOrientationDataLoadOptions {
    std::optional<AstronomicalEpoch> referenceEpoch;
};

struct EarthOrientationDataLoadResult {
    std::shared_ptr<const IEarthOrientationProvider> provider;
    EarthOrientationDataInfo dataInfo;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return provider != nullptr && dataInfo.isUsable();
    }
};

[[nodiscard]] EarthOrientationDataLoadResult loadEarthOrientationDataFromSnapshot(
    const IEphemerisDataSnapshot& snapshot, const EarthOrientationDataLoadOptions& options = {}
);

[[nodiscard]] EarthOrientationDataLoadResult loadEarthOrientationDataFromTextAsset(
    const EphemerisTextDataAsset& asset, const EarthOrientationDataLoadOptions& options = {}
);

[[nodiscard]] EarthOrientationSample sampleEarthOrientation(
    const IEarthOrientationProvider* provider,
    const AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampleOptions& options = {}
);

[[nodiscard]] EarthOrientationSample sampleEarthOrientation(
    const std::shared_ptr<const IEarthOrientationProvider>& provider,
    const AstronomicalEpoch& utcEpoch,
    const EarthOrientationSampleOptions& options = {}
);

}  // namespace skygate::ephemeris
