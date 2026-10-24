#include "PlanetIdentity.hpp"

#include "NaifBodyIds.hpp"
#include "StringUtilities.hpp"
#include "catalog/CelestialBodyIds.hpp"

#include <array>
#include <string_view>

namespace skygate::ephemeris {
namespace {

using skygate::core::NaifBodyIds;

struct PlanetIdentityEntry final {
    PlanetIdentity::Key key;
    std::string_view bodyId;
    int naifBodyCenterId = 0;
    int naifBarycenterId = 0;
    bool hasDe440sBodyCenter = false;
};

// The body ids come from the catalog vocabulary so the engines and the
// bundled catalog cannot drift apart. Pluto has no bundled-catalog row and
// keeps a literal id that extends the catalog vocabulary for the engines.
constexpr std::array<PlanetIdentityEntry, 8> kPlanetIdentityEntries = {{
    {.key = PlanetIdentity::Key::Mercury,
     .bodyId = CelestialBodyIds::kMercury,
     .naifBodyCenterId = NaifBodyIds::kMercury,
     .naifBarycenterId = NaifBodyIds::kMercuryBarycenter,
     .hasDe440sBodyCenter = true},
    {.key = PlanetIdentity::Key::Venus,
     .bodyId = CelestialBodyIds::kVenus,
     .naifBodyCenterId = NaifBodyIds::kVenus,
     .naifBarycenterId = NaifBodyIds::kVenusBarycenter,
     .hasDe440sBodyCenter = true},
    {.key = PlanetIdentity::Key::Mars,
     .bodyId = CelestialBodyIds::kMars,
     .naifBodyCenterId = NaifBodyIds::kMars,
     .naifBarycenterId = NaifBodyIds::kMarsBarycenter},
    {.key = PlanetIdentity::Key::Jupiter,
     .bodyId = CelestialBodyIds::kJupiter,
     .naifBodyCenterId = NaifBodyIds::kJupiter,
     .naifBarycenterId = NaifBodyIds::kJupiterBarycenter},
    {.key = PlanetIdentity::Key::Saturn,
     .bodyId = CelestialBodyIds::kSaturn,
     .naifBodyCenterId = NaifBodyIds::kSaturn,
     .naifBarycenterId = NaifBodyIds::kSaturnBarycenter},
    {.key = PlanetIdentity::Key::Uranus,
     .bodyId = CelestialBodyIds::kUranus,
     .naifBodyCenterId = NaifBodyIds::kUranus,
     .naifBarycenterId = NaifBodyIds::kUranusBarycenter},
    {.key = PlanetIdentity::Key::Neptune,
     .bodyId = CelestialBodyIds::kNeptune,
     .naifBodyCenterId = NaifBodyIds::kNeptune,
     .naifBarycenterId = NaifBodyIds::kNeptuneBarycenter},
    {.key = PlanetIdentity::Key::Pluto,
     .bodyId = "pluto",
     .naifBodyCenterId = NaifBodyIds::kPluto,
     .naifBarycenterId = NaifBodyIds::kPlutoBarycenter},
}};

}  // namespace

std::optional<PlanetIdentity> PlanetIdentity::fromBodyId(const std::string_view bodyId) noexcept
{
    for (const PlanetIdentityEntry& entry : kPlanetIdentityEntries) {
        if (!StringUtilities::equalsIgnoreAsciiCase(bodyId, entry.bodyId)) {
            continue;
        }
        return PlanetIdentity{entry.key, entry.naifBodyCenterId, entry.naifBarycenterId, entry.hasDe440sBodyCenter};
    }
    return std::nullopt;
}

PlanetIdentity::Key PlanetIdentity::key() const noexcept
{
    return m_key;
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
