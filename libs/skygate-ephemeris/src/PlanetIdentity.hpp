#pragma once

#include <optional>
#include <string_view>

namespace skygate::ephemeris {

// Canonical planet identities shared by the reference and high-precision
// engines. A body id denotes a planet when it equals the canonical id, matched
// ASCII case-insensitively; the vocabulary, the matching rule, and the NAIF
// mapping live here so both engines recognize the same ids.
class PlanetIdentity final {
public:
    [[nodiscard]] static std::optional<PlanetIdentity> fromBodyId(std::string_view bodyId) noexcept;

    [[nodiscard]] std::string_view canonicalId() const noexcept;
    [[nodiscard]] int naifBodyCenterId() const noexcept;
    [[nodiscard]] int naifBarycenterId() const noexcept;
    // True when the DE440s kernel carries this planet's body center.
    [[nodiscard]] bool hasDe440sBodyCenter() const noexcept;

private:
    constexpr PlanetIdentity(
        const std::string_view canonicalId,
        const int naifBodyCenterId,
        const int naifBarycenterId,
        const bool hasDe440sBodyCenter
    ) noexcept
        : m_canonicalId(canonicalId), m_naifBodyCenterId(naifBodyCenterId), m_naifBarycenterId(naifBarycenterId),
          m_hasDe440sBodyCenter(hasDe440sBodyCenter)
    {
    }

    std::string_view m_canonicalId;
    int m_naifBodyCenterId = 0;
    int m_naifBarycenterId = 0;
    bool m_hasDe440sBodyCenter = false;
};

}  // namespace skygate::ephemeris
