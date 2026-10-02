#include "TableBackedLeapSecondProvider.hpp"

#include <utility>

namespace skygate::ephemeris {

TableBackedLeapSecondProvider::TableBackedLeapSecondProvider(
    ILeapSecondProvider::TableInfo tableInfo, std::vector<ILeapSecondProvider::TableEntry> entries
)
    : m_tableInfo(std::move(tableInfo)), m_entries(std::move(entries))
{
}

const ILeapSecondProvider::TableInfo& TableBackedLeapSecondProvider::tableInfo() const noexcept
{
    return m_tableInfo;
}

std::span<const ILeapSecondProvider::TableEntry> TableBackedLeapSecondProvider::entries() const noexcept
{
    return m_entries;
}

std::optional<int>
TableBackedLeapSecondProvider::taiMinusUtcSeconds(const skygate::core::AstronomicalEpoch& utcEpoch) const noexcept
{
    if (!utcEpoch.isFiniteUtc()) {
        return std::nullopt;
    }

    const double requestedEpochKey = utcEpoch.sortKey();
    std::optional<int> offset;
    for (const ILeapSecondProvider::TableEntry& entry : m_entries) {
        if (entry.effectiveUtcEpoch.sortKey() > requestedEpochKey) {
            break;
        }
        offset = entry.taiMinusUtcSeconds;
    }

    return offset;
}

}  // namespace skygate::ephemeris
