#pragma once

#include "time/EphemerisDateRange.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CivilDateTime.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace skygate::ephemeris {

class IDeltaTProvider {
public:
    enum class DataStatus : std::uint8_t {
        Available,
        Missing,
        Malformed,
        Stale
    };

    enum class EstimateStatus : std::uint8_t {
        Available,
        Degraded,
        Unavailable
    };

    struct TableEntry {
        CivilDateTime effectiveUtcDate;
        AstronomicalEpoch effectiveUtcEpoch;
        double deltaTSeconds = 0.0;
    };

    struct DataInfo {
        struct FallbackModelInfo {
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

        std::string version;
        std::string provenance;
        DataStatus status = DataStatus::Missing;
        std::string diagnosticText;
        std::optional<EphemerisDateRange> validityRange;
        std::optional<AstronomicalEpoch> expiresAt;
        std::optional<FallbackModelInfo> ancientFallbackModel;

        [[nodiscard]] bool isUsable() const noexcept
        {
            return status == DataStatus::Available || status == DataStatus::Stale;
        }
    };

    struct Estimate {
        EstimateStatus status = EstimateStatus::Unavailable;
        std::optional<double> deltaTSeconds;
        std::optional<double> estimatedUncertaintySeconds;
        std::string diagnosticText;
        std::string provenance;

        [[nodiscard]] bool isUsable() const noexcept
        {
            return status == EstimateStatus::Available || status == EstimateStatus::Degraded;
        }
    };

    virtual ~IDeltaTProvider() = default;

    [[nodiscard]] virtual const DataInfo& dataInfo() const noexcept = 0;
    [[nodiscard]] virtual std::span<const TableEntry> entries() const noexcept = 0;
    [[nodiscard]] virtual Estimate deltaTSeconds(const AstronomicalEpoch& epoch) const = 0;
};

}  // namespace skygate::ephemeris
