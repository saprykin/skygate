#pragma once

#include "time/EphemerisDateRange.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CivilDateTime.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace skygate::ephemeris {

class IEarthOrientationProvider {
public:
    enum class DataStatus : std::uint8_t {
        Available,
        Missing,
        Malformed,
        Stale,
        Estimated
    };

    struct TableEntry {
        CivilDateTime effectiveUtcDate;
        AstronomicalEpoch effectiveUtcEpoch;
        double ut1MinusUtcSeconds = 0.0;
        double polarMotionXArcseconds = 0.0;
        double polarMotionYArcseconds = 0.0;
        bool predicted = false;
        bool estimated = false;
    };

    struct DataInfo {
        std::string version;
        std::string provenance;
        DataStatus status = DataStatus::Missing;
        std::string diagnosticText;
        std::optional<EphemerisDateRange> validityRange;
        std::optional<EphemerisDateRange> predictionRange;
        std::optional<AstronomicalEpoch> expiresAt;

        [[nodiscard]] bool isUsable() const noexcept
        {
            return status == DataStatus::Available || status == DataStatus::Stale || status == DataStatus::Estimated;
        }
    };

    virtual ~IEarthOrientationProvider() = default;

    [[nodiscard]] virtual const DataInfo& dataInfo() const noexcept = 0;
    [[nodiscard]] virtual std::span<const TableEntry> entries() const noexcept = 0;
};

}  // namespace skygate::ephemeris
