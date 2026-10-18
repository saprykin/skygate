#pragma once

#include "EphemerisTableDataInfo.hpp"
#include "EphemerisTableDataStatus.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace skygate::ephemeris {

class IDeltaTProvider {
public:
    /// The provider keeps its established name for the shared status enum.
    using DataStatus = EphemerisTableDataStatus;

    enum class EstimateStatus : std::uint8_t {
        Available,
        Degraded,
        Unavailable
    };

    struct TableEntry {
        skygate::core::AstronomicalEpoch effectiveUtcEpoch;
        double deltaTSeconds = 0.0;
    };

    struct DataInfo final : EphemerisTableDataInfo {
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

        std::optional<FallbackModelInfo> ancientFallbackModel;
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
    [[nodiscard]] virtual Estimate deltaTSeconds(const skygate::core::AstronomicalEpoch& epoch) const = 0;
};

}  // namespace skygate::ephemeris
