#pragma once

#include "IDeltaTProvider.hpp"

#include <vector>

namespace skygate::ephemeris {

class TableBackedDeltaTProvider final : public IDeltaTProvider {
public:
    TableBackedDeltaTProvider(DataInfo dataInfo, std::vector<TableEntry> entries);

    [[nodiscard]] const DataInfo& dataInfo() const noexcept override;
    [[nodiscard]] std::span<const TableEntry> entries() const noexcept override;
    [[nodiscard]] Estimate deltaTSeconds(const AstronomicalEpoch& epoch) const override;

private:
    DataInfo m_dataInfo;
    std::vector<TableEntry> m_entries;
};

}  // namespace skygate::ephemeris
