#include "LeapSecondTableScan.hpp"
#include "time/AstronomicalEpoch.hpp"

#include <optional>

namespace skygate::ephemeris {

namespace {

/// One step lookup over the table rows: the offset of the last row whose key,
/// expressed in the domain of the request, is not after the requested key.
template <typename KeyOfEntry>
[[nodiscard]] std::optional<int> offsetAtKey(
    const std::span<const ILeapSecondProvider::TableEntry> entries, const double requestedKey, KeyOfEntry keyOfEntry
) noexcept
{
    std::optional<int> offset;
    for (const ILeapSecondProvider::TableEntry& entry : entries) {
        if (keyOfEntry(entry) > requestedKey) {
            break;
        }
        offset = entry.taiMinusUtcSeconds;
    }

    return offset;
}

}  // namespace

std::optional<int> LeapSecondTableScan::offsetAtUtcKey(
    const std::span<const ILeapSecondProvider::TableEntry> entries, const double requestedUtcKey
) noexcept
{
    return offsetAtKey(entries, requestedUtcKey, [](const ILeapSecondProvider::TableEntry& entry) {
        return entry.effectiveUtcEpoch.sortKey();
    });
}

std::optional<int> LeapSecondTableScan::offsetAtTaiKey(
    const std::span<const ILeapSecondProvider::TableEntry> entries, const double requestedTaiKey
) noexcept
{
    return offsetAtKey(entries, requestedTaiKey, [](const ILeapSecondProvider::TableEntry& entry) {
        return entry.effectiveUtcEpoch
            .addSeconds(static_cast<double>(entry.taiMinusUtcSeconds), skygate::core::TimeScale::Tai)
            .sortKey();
    });
}

}  // namespace skygate::ephemeris
