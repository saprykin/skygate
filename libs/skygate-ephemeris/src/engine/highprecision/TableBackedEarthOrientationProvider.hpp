#pragma once

#include "IEarthOrientationProvider.hpp"

#include <vector>

namespace skygate::ephemeris {

class TableBackedEarthOrientationProvider final : public IEarthOrientationProvider {
public:
    TableBackedEarthOrientationProvider(DataInfo dataInfo, std::vector<TableEntry> entries);

    [[nodiscard]] const DataInfo& dataInfo() const noexcept override;
    [[nodiscard]] std::span<const TableEntry> entries() const noexcept override;

private:
    DataInfo m_dataInfo;
    std::vector<TableEntry> m_entries;
};

}  // namespace skygate::ephemeris
