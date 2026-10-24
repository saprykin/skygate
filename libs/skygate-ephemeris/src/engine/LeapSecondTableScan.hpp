#pragma once

#include "ILeapSecondProvider.hpp"

#include <optional>
#include <span>

namespace skygate::ephemeris {

/// Finds the leap-second step that is in force at one epoch. The table-backed
/// provider scans the rows in the UTC domain and the offset resolver scans
/// them in the TAI domain; both use this one step lookup.
class LeapSecondTableScan final {
public:
    LeapSecondTableScan() = delete;

    /// TAI-UTC offset of the row with the latest effective UTC epoch that is
    /// not after `requestedUtcKey`.
    [[nodiscard]] static std::optional<int>
    offsetAtUtcKey(std::span<const ILeapSecondProvider::TableEntry> entries, double requestedUtcKey) noexcept;

    /// TAI-UTC offset in force at a TAI epoch: the row whose effective UTC
    /// epoch moved by its own offset is the latest one that is not after
    /// `requestedTaiKey`.
    [[nodiscard]] static std::optional<int>
    offsetAtTaiKey(std::span<const ILeapSecondProvider::TableEntry> entries, double requestedTaiKey) noexcept;
};

}  // namespace skygate::ephemeris
