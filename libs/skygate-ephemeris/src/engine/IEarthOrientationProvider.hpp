#pragma once

#include "EphemerisTableDataInfo.hpp"
#include "EphemerisTableDataStatus.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"

#include <optional>
#include <span>

namespace skygate::ephemeris {

class IEarthOrientationProvider {
public:
    /// The provider keeps its established name for the shared status enum.
    using DataStatus = EphemerisTableDataStatus;

    struct TableEntry {
        skygate::core::AstronomicalEpoch effectiveUtcEpoch;
        double ut1MinusUtcSeconds = 0.0;
        double polarMotionXArcseconds = 0.0;
        double polarMotionYArcseconds = 0.0;
        bool predicted = false;
        bool estimated = false;
    };

    struct DataInfo final : EphemerisTableDataInfo {
        DataInfo() noexcept
            : EphemerisTableDataInfo({EphemerisTableDataStatus::Available, EphemerisTableDataStatus::Stale})
        {
        }

        std::optional<EphemerisDateRange> predictionRange;
    };

    virtual ~IEarthOrientationProvider() = default;

    [[nodiscard]] virtual const DataInfo& dataInfo() const noexcept = 0;
    [[nodiscard]] virtual std::span<const TableEntry> entries() const noexcept = 0;
};

}  // namespace skygate::ephemeris
