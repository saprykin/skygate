#pragma once

#include "skygate/ephemeris/EphemerisDataSnapshot.hpp"
#include "skygate/ephemeris/Types.hpp"

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace skygate::ephemeris {

enum class LeapSecondTableStatus : std::uint8_t {
    Available,
    Missing,
    Malformed,
    Stale
};

[[nodiscard]] constexpr std::string_view displayName(const LeapSecondTableStatus status) noexcept
{
    switch (status) {
    case LeapSecondTableStatus::Available:
        return "available";
    case LeapSecondTableStatus::Missing:
        return "missing";
    case LeapSecondTableStatus::Malformed:
        return "malformed";
    case LeapSecondTableStatus::Stale:
        return "stale";
    }

    return {};
}

struct LeapSecondTableEntry {
    CivilDateTime effectiveUtcDate;
    AstronomicalEpoch effectiveUtcEpoch;
    int taiMinusUtcSeconds = 0;
};

struct LeapSecondTableInfo {
    std::string version;
    std::string provenance;
    LeapSecondTableStatus status = LeapSecondTableStatus::Missing;
    std::string diagnosticText;
    std::optional<EphemerisDateRange> validityRange;
    std::optional<AstronomicalEpoch> expiresAt;

    [[nodiscard]] bool isUsable() const noexcept
    {
        return status == LeapSecondTableStatus::Available || status == LeapSecondTableStatus::Stale;
    }
};

class ILeapSecondProvider {
public:
    virtual ~ILeapSecondProvider() = default;

    [[nodiscard]] virtual const LeapSecondTableInfo& tableInfo() const noexcept = 0;
    [[nodiscard]] virtual std::span<const LeapSecondTableEntry> entries() const noexcept = 0;
    [[nodiscard]] virtual std::optional<int> taiMinusUtcSeconds(const AstronomicalEpoch& utcEpoch) const noexcept = 0;
};

class TableBackedLeapSecondProvider final : public ILeapSecondProvider {
public:
    TableBackedLeapSecondProvider(LeapSecondTableInfo tableInfo, std::vector<LeapSecondTableEntry> entries);

    [[nodiscard]] const LeapSecondTableInfo& tableInfo() const noexcept override;
    [[nodiscard]] std::span<const LeapSecondTableEntry> entries() const noexcept override;
    [[nodiscard]] std::optional<int> taiMinusUtcSeconds(const AstronomicalEpoch& utcEpoch) const noexcept override;

private:
    LeapSecondTableInfo m_tableInfo;
    std::vector<LeapSecondTableEntry> m_entries;
};

struct LeapSecondTableLoadOptions {
    std::optional<AstronomicalEpoch> referenceEpoch;
};

struct LeapSecondTableLoadResult {
    std::shared_ptr<const ILeapSecondProvider> provider;
    LeapSecondTableInfo tableInfo;

    [[nodiscard]] bool isSuccess() const noexcept
    {
        return provider != nullptr && tableInfo.isUsable();
    }
};

[[nodiscard]] LeapSecondTableLoadResult
loadLeapSecondTableFromSnapshot(const IEphemerisDataSnapshot& snapshot, const LeapSecondTableLoadOptions& options = {});

[[nodiscard]] LeapSecondTableLoadResult
loadLeapSecondTableFromTextAsset(const EphemerisTextDataAsset& asset, const LeapSecondTableLoadOptions& options = {});

}  // namespace skygate::ephemeris
