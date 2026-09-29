#include "TableBackedEarthOrientationProvider.hpp"

#include <utility>

namespace skygate::ephemeris {

TableBackedEarthOrientationProvider::TableBackedEarthOrientationProvider(
    IEarthOrientationProvider::DataInfo dataInfo, std::vector<IEarthOrientationProvider::TableEntry> entries
)
    : m_dataInfo(std::move(dataInfo)), m_entries(std::move(entries))
{
}

const IEarthOrientationProvider::DataInfo& TableBackedEarthOrientationProvider::dataInfo() const noexcept
{
    return m_dataInfo;
}

std::span<const IEarthOrientationProvider::TableEntry> TableBackedEarthOrientationProvider::entries() const noexcept
{
    return m_entries;
}

}  // namespace skygate::ephemeris
