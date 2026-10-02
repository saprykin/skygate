#pragma once

#include "ILeapSecondProvider.hpp"

#include <vector>

namespace skygate::ephemeris {

class TableBackedLeapSecondProvider final : public ILeapSecondProvider {
public:
    TableBackedLeapSecondProvider(TableInfo tableInfo, std::vector<TableEntry> entries);

    [[nodiscard]] const TableInfo& tableInfo() const noexcept override;
    [[nodiscard]] std::span<const TableEntry> entries() const noexcept override;
    [[nodiscard]] std::optional<int>
    taiMinusUtcSeconds(const skygate::core::AstronomicalEpoch& utcEpoch) const noexcept override;

private:
    TableInfo m_tableInfo;
    std::vector<TableEntry> m_entries;
};

}  // namespace skygate::ephemeris
