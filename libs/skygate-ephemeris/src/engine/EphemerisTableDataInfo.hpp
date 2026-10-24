#pragma once

#include "BitFlagSet.hpp"
#include "EphemerisTableDataStatus.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"

#include <initializer_list>
#include <optional>
#include <string>

namespace skygate::ephemeris {

/// Fields common to the infos of the ephemeris table providers. The usable
/// status set of an info decides which loading outcomes still serve data, so
/// a provider states that difference instead of redefining isUsable().
class EphemerisTableDataInfo {
public:
    /// Statuses of one table info, held as an engine flag set.
    using UsableStatuses = BitFlagSet<EphemerisTableDataStatus>;

    EphemerisTableDataInfo() noexcept = default;

    explicit EphemerisTableDataInfo(std::initializer_list<EphemerisTableDataStatus> usableStatuses) noexcept
        : m_usableStatuses(statusSet(usableStatuses))
    {
    }

    std::string version;
    std::string provenance;
    EphemerisTableDataStatus status = EphemerisTableDataStatus::Missing;
    std::string diagnosticText;
    std::optional<EphemerisDateRange> validityRange;
    std::optional<skygate::core::AstronomicalEpoch> expiresAt;

    [[nodiscard]] bool isUsable() const noexcept
    {
        return m_usableStatuses.has(UsableStatuses::bit(status));
    }

    /// Flag set of the given statuses.
    [[nodiscard]] static constexpr UsableStatuses
    statusSet(std::initializer_list<EphemerisTableDataStatus> statuses) noexcept
    {
        UsableStatuses::Bits bits = 0U;
        for (const EphemerisTableDataStatus usableStatus : statuses) {
            bits |= UsableStatuses::bit(usableStatus);
        }

        return UsableStatuses::fromBits(bits);
    }

private:
    UsableStatuses m_usableStatuses{statusSet({EphemerisTableDataStatus::Available, EphemerisTableDataStatus::Stale})};
};

}  // namespace skygate::ephemeris
