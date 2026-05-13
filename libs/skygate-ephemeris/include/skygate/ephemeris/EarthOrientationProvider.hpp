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

}  // namespace skygate::ephemeris
