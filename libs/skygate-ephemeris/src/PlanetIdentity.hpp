#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace skygate::ephemeris {

// Canonical planet identities shared by the reference and high-precision
// engines. A body id denotes a planet when it equals a catalog body id from
// catalog/CelestialBodyIds.hpp, matched ASCII case-insensitively; the
// vocabulary, the matching rule, and the NAIF mapping live here so both
// engines recognize the same ids as the bundled catalog.
class PlanetIdentity final {
public:
    enum class Key : std::uint8_t {
        Mercury,
        Venus,
        Mars,
        Jupiter,
        Saturn,
        Uranus,
        Neptune,
        Pluto
    };

    [[nodiscard]] static std::optional<PlanetIdentity> fromBodyId(std::string_view bodyId) noexcept;

    [[nodiscard]] Key key() const noexcept;
    [[nodiscard]] int naifBodyCenterId() const noexcept;
    [[nodiscard]] int naifBarycenterId() const noexcept;
    // True when the DE440s kernel carries this planet's body center.
    [[nodiscard]] bool hasDe440sBodyCenter() const noexcept;

private:
    constexpr PlanetIdentity(
        const Key key, const int naifBodyCenterId, const int naifBarycenterId, const bool hasDe440sBodyCenter
    ) noexcept
        : m_key(key), m_naifBodyCenterId(naifBodyCenterId), m_naifBarycenterId(naifBarycenterId),
          m_hasDe440sBodyCenter(hasDe440sBodyCenter)
    {
    }

    Key m_key;
    int m_naifBodyCenterId = 0;
    int m_naifBarycenterId = 0;
    bool m_hasDe440sBodyCenter = false;
};

}  // namespace skygate::ephemeris
