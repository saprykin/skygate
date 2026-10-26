#pragma once

#include "EphemerisTableDataInfo.hpp"
#include "EphemerisTableDataStatus.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <optional>
#include <span>

namespace skygate::ephemeris {

class ILeapSecondProvider {
public:
    /// The provider keeps its established name for the shared status enum.
    using TableStatus = EphemerisTableDataStatus;

    struct TableEntry {
        skygate::core::AstronomicalEpoch effectiveUtcEpoch;
        int taiMinusUtcSeconds = 0;
    };

    struct TableInfo final : EphemerisTableDataInfo {};

    virtual ~ILeapSecondProvider() = default;

    [[nodiscard]] virtual const TableInfo& tableInfo() const noexcept = 0;
    [[nodiscard]] virtual std::span<const TableEntry> entries() const noexcept = 0;
    [[nodiscard]] virtual std::optional<int>
    taiMinusUtcSeconds(const skygate::core::AstronomicalEpoch& utcEpoch) const noexcept = 0;
};

}  // namespace skygate::ephemeris
