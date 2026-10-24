#include "TableBackedLeapSecondProvider.hpp"
#include "LeapSecondTableScan.hpp"

#include <optional>
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

    return LeapSecondTableScan::offsetAtUtcKey(m_entries, utcEpoch.sortKey());
}

}  // namespace skygate::ephemeris
