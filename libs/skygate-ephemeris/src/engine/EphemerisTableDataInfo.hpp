#pragma once

#include "EphemerisTableDataStatus.hpp"
#include "time/AstronomicalEpoch.hpp"
#include "time/EphemerisDateRange.hpp"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>

namespace skygate::ephemeris {

/// Fields common to the infos of the ephemeris table providers. The usable
/// status set of an info decides which loading outcomes still serve data, so
/// a provider states that difference instead of redefining isUsable().
class EphemerisTableDataInfo {
public:
    EphemerisTableDataInfo() noexcept = default;

    explicit EphemerisTableDataInfo(std::initializer_list<EphemerisTableDataStatus> usableStatuses) noexcept
        : m_usableStatuses(usableStatuses)
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
        return m_usableStatuses.contains(status);
    }

private:
    class UsableStatuses final {
    public:
        constexpr UsableStatuses() noexcept = default;

        constexpr UsableStatuses(std::initializer_list<EphemerisTableDataStatus> statuses) noexcept
        {
            for (const EphemerisTableDataStatus status : statuses) {
                m_bits = static_cast<std::uint8_t>(m_bits | bitFor(status));
            }
        }

        [[nodiscard]] constexpr bool contains(const EphemerisTableDataStatus status) const noexcept
        {
            return (m_bits & bitFor(status)) != 0U;
        }

    private:
        [[nodiscard]] static constexpr std::uint8_t bitFor(const EphemerisTableDataStatus status) noexcept
        {
            return static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(status));
        }

        std::uint8_t m_bits = 0U;
    };

    UsableStatuses m_usableStatuses{EphemerisTableDataStatus::Available, EphemerisTableDataStatus::Stale};
};

}  // namespace skygate::ephemeris
