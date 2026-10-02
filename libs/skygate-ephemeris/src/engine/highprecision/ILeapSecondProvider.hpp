#pragma once

#include "time/EphemerisDateRange.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/CivilDateTime.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace skygate::ephemeris {

class ILeapSecondProvider {
public:
    enum class TableStatus : std::uint8_t {
        Available,
        Missing,
        Malformed,
        Stale
    };

    struct TableEntry {
        CivilDateTime effectiveUtcDate;
        AstronomicalEpoch effectiveUtcEpoch;
        int taiMinusUtcSeconds = 0;
    };

    struct TableInfo {
        std::string version;
        std::string provenance;
        TableStatus status = TableStatus::Missing;
        std::string diagnosticText;
        std::optional<EphemerisDateRange> validityRange;
        std::optional<AstronomicalEpoch> expiresAt;

        [[nodiscard]] bool isUsable() const noexcept
        {
            return status == TableStatus::Available || status == TableStatus::Stale;
        }
    };

    virtual ~ILeapSecondProvider() = default;

    [[nodiscard]] virtual const TableInfo& tableInfo() const noexcept = 0;
    [[nodiscard]] virtual std::span<const TableEntry> entries() const noexcept = 0;
    [[nodiscard]] virtual std::optional<int> taiMinusUtcSeconds(const AstronomicalEpoch& utcEpoch) const noexcept = 0;
};

}  // namespace skygate::ephemeris
