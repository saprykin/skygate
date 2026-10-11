#include "PlanetIdentity.hpp"

#include "NaifBodyIds.hpp"
#include "StringUtilities.hpp"

#include <array>
#include <string_view>

namespace skygate::ephemeris {
namespace {

using skygate::core::NaifBodyIds;

struct PlanetIdentityEntry final {
    std::string_view canonicalId;
    int naifBodyCenterId = 0;
    int naifBarycenterId = 0;
    bool hasDe440sBodyCenter = false;
};

constexpr std::array<PlanetIdentityEntry, 8> kPlanetIdentityEntries = {{
    {.canonicalId = "mercury",
     .naifBodyCenterId = NaifBodyIds::kMercury,
     .naifBarycenterId = NaifBodyIds::kMercuryBarycenter,
     .hasDe440sBodyCenter = true},
    {.canonicalId = "venus",
     .naifBodyCenterId = NaifBodyIds::kVenus,
     .naifBarycenterId = NaifBodyIds::kVenusBarycenter,
     .hasDe440sBodyCenter = true},
    {.canonicalId = "mars", .naifBodyCenterId = NaifBodyIds::kMars, .naifBarycenterId = NaifBodyIds::kMarsBarycenter},
    {.canonicalId = "jupiter",
     .naifBodyCenterId = NaifBodyIds::kJupiter,
     .naifBarycenterId = NaifBodyIds::kJupiterBarycenter},
    {.canonicalId = "saturn",
     .naifBodyCenterId = NaifBodyIds::kSaturn,
     .naifBarycenterId = NaifBodyIds::kSaturnBarycenter},
    {.canonicalId = "uranus",
     .naifBodyCenterId = NaifBodyIds::kUranus,
     .naifBarycenterId = NaifBodyIds::kUranusBarycenter},
    {.canonicalId = "neptune",
     .naifBodyCenterId = NaifBodyIds::kNeptune,
     .naifBarycenterId = NaifBodyIds::kNeptuneBarycenter},
    {.canonicalId = "pluto",
     .naifBodyCenterId = NaifBodyIds::kPluto,
     .naifBarycenterId = NaifBodyIds::kPlutoBarycenter},
}};

}  // namespace

std::optional<PlanetIdentity> PlanetIdentity::fromBodyId(const std::string_view bodyId) noexcept
{
    for (const PlanetIdentityEntry& entry : kPlanetIdentityEntries) {
        if (!StringUtilities::equalsIgnoreAsciiCase(bodyId, entry.canonicalId)) {
            continue;
        }
        return PlanetIdentity{
            entry.canonicalId, entry.naifBodyCenterId, entry.naifBarycenterId, entry.hasDe440sBodyCenter
        };
    }
    return std::nullopt;
}

std::string_view PlanetIdentity::canonicalId() const noexcept
{
    return m_canonicalId;
}

int PlanetIdentity::naifBodyCenterId() const noexcept
{
    return m_naifBodyCenterId;
}

int PlanetIdentity::naifBarycenterId() const noexcept
{
    return m_naifBarycenterId;
}

bool PlanetIdentity::hasDe440sBodyCenter() const noexcept
{
    return m_hasDe440sBodyCenter;
}

}  // namespace skygate::ephemeris
