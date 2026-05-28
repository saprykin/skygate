#pragma once

#include "EphemerisDataSnapshot.hpp"
#include "engine/EphemerisDateRange.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CivilDateTime.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

enum class DeltaTDataStatus : std::uint8_t {
    Available,
    Missing,
    Malformed,
    Stale
};

[[nodiscard]] constexpr std::string_view displayName(const DeltaTDataStatus status) noexcept
{
    switch (status) {
    case DeltaTDataStatus::Available:
        return "available";
    case DeltaTDataStatus::Missing:
        return "missing";
    case DeltaTDataStatus::Malformed:
        return "malformed";
    case DeltaTDataStatus::Stale:
        return "stale";
    }

    return {};
}

enum class DeltaTEstimateStatus : std::uint8_t {
    Available,
    Degraded,
    Unavailable
};

[[nodiscard]] constexpr std::string_view displayName(const DeltaTEstimateStatus status) noexcept
{
    switch (status) {
    case DeltaTEstimateStatus::Available:
        return "available";
    case DeltaTEstimateStatus::Degraded:
        return "degraded";
    case DeltaTEstimateStatus::Unavailable:
        return "unavailable";
    }

    return {};
}

struct DeltaTTableEntry {
    CivilDateTime effectiveUtcDate;
    AstronomicalEpoch effectiveUtcEpoch;
    double deltaTSeconds = 0.0;
};

struct DeltaTFallbackModelInfo {
    std::string id;
    std::string displayName;
    std::string provenance;
    EphemerisDateRange validityRange;
    std::optional<double> representativeDeltaTSeconds;
    std::optional<double> estimatedUncertaintySeconds;

    [[nodiscard]] bool hasRepresentativeEstimate() const noexcept
    {
        return representativeDeltaTSeconds.has_value();
    }
};

struct DeltaTDataInfo {
    std::string version;
    std::string provenance;
    DeltaTDataStatus status = DeltaTDataStatus::Missing;
    std::string diagnosticText;
    std::optional<EphemerisDateRange> validityRange;
    std::optional<AstronomicalEpoch> expiresAt;
    std::optional<DeltaTFallbackModelInfo> ancientFallbackModel;

    [[nodiscard]] bool isUsable() const noexcept
    {
        return status == DeltaTDataStatus::Available || status == DeltaTDataStatus::Stale;
    }
};

struct DeltaTEstimate {
    DeltaTEstimateStatus status = DeltaTEstimateStatus::Unavailable;
    std::optional<double> deltaTSeconds;
    std::optional<double> estimatedUncertaintySeconds;
    std::string diagnosticText;
    std::string provenance;

    [[nodiscard]] bool isUsable() const noexcept
    {
        return status == DeltaTEstimateStatus::Available || status == DeltaTEstimateStatus::Degraded;
    }
};

class IDeltaTProvider {
public:
    virtual ~IDeltaTProvider() = default;

    [[nodiscard]] virtual const DeltaTDataInfo& dataInfo() const noexcept = 0;
    [[nodiscard]] virtual std::span<const DeltaTTableEntry> entries() const noexcept = 0;
    [[nodiscard]] virtual DeltaTEstimate deltaTSeconds(const AstronomicalEpoch& epoch) const = 0;
};

class TableBackedDeltaTProvider final : public IDeltaTProvider {
public:
    TableBackedDeltaTProvider(DeltaTDataInfo dataInfo, std::vector<DeltaTTableEntry> entries);

    [[nodiscard]] const DeltaTDataInfo& dataInfo() const noexcept override;
    [[nodiscard]] std::span<const DeltaTTableEntry> entries() const noexcept override;
    [[nodiscard]] DeltaTEstimate deltaTSeconds(const AstronomicalEpoch& epoch) const override;

private:
    DeltaTDataInfo m_dataInfo;
    std::vector<DeltaTTableEntry> m_entries;
};

struct DeltaTDataLoadOptions {
    std::optional<AstronomicalEpoch> referenceEpoch;
};

struct DeltaTDataLoadResult {
    std::shared_ptr<const IDeltaTProvider> provider;
    DeltaTDataInfo dataInfo;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return provider != nullptr && dataInfo.isUsable();
    }
};

[[nodiscard]] DeltaTDataLoadResult
loadDeltaTDataFromSnapshot(const IEphemerisDataSnapshot& snapshot, const DeltaTDataLoadOptions& options = {});

[[nodiscard]] DeltaTDataLoadResult
loadDeltaTDataFromTextAsset(const EphemerisTextDataAsset& asset, const DeltaTDataLoadOptions& options = {});

}  // namespace skygate::ephemeris
