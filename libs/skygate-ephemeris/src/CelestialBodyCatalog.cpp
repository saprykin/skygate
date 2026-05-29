#include "CelestialBodyCatalog.hpp"
#include "DeepSkyObjectInfo.hpp"
#include "EquatorialCoordinate.hpp"
#include "catalog/CatalogStarAstrometry.hpp"

#include <utility>

namespace skygate::ephemeris {
namespace {

[[nodiscard]] OwnGalaxyCelestialBody toOwnGalaxyBody(const BaseCelestialBody& body)
{
    OwnGalaxyCelestialBody ownGalaxyBody;
    ownGalaxyBody.id = body.id;
    ownGalaxyBody.displayName = body.displayName;
    ownGalaxyBody.kind = body.kind;
    ownGalaxyBody.visualMagnitude = body.visualMagnitude;
    if (const skygate::core::EquatorialCoordinate* fixedEquatorial = body.fixedEquatorialCoordinate()) {
        ownGalaxyBody.fixedEquatorial = *fixedEquatorial;
    }
    if (const CatalogStarAstrometry* starAstrometry = body.catalogStarAstrometry()) {
        ownGalaxyBody.starAstrometry = *starAstrometry;
    }
    return ownGalaxyBody;
}

[[nodiscard]] DistantCelestialBody toDistantBody(const BaseCelestialBody& body)
{
    DistantCelestialBody distantBody;
    distantBody.id = body.id;
    distantBody.displayName = body.displayName;
    distantBody.kind = body.kind;
    distantBody.visualMagnitude = body.visualMagnitude;
    if (const skygate::core::EquatorialCoordinate* fixedEquatorial = body.fixedEquatorialCoordinate()) {
        distantBody.fixedEquatorial = *fixedEquatorial;
    }
    if (const DeepSkyObjectInfo* deepSkyObject = body.deepSkyObjectInfo()) {
        distantBody.deepSkyObject = *deepSkyObject;
    }
    return distantBody;
}

}  // namespace

CelestialBodyCatalog::CelestialBodyCatalog(const CelestialBodyCatalog& other)
    : m_ownGalaxyBodies(other.m_ownGalaxyBodies), m_distantBodies(other.m_distantBodies),
      m_orderedBodyIndexes(other.m_orderedBodyIndexes)
{
    rebuildOrderedBodies();
}

CelestialBodyCatalog& CelestialBodyCatalog::operator=(const CelestialBodyCatalog& other)
{
    if (this == &other) {
        return *this;
    }

    m_ownGalaxyBodies = other.m_ownGalaxyBodies;
    m_distantBodies = other.m_distantBodies;
    m_orderedBodyIndexes = other.m_orderedBodyIndexes;
    rebuildOrderedBodies();
    return *this;
}

CelestialBodyCatalog::CelestialBodyCatalog(CelestialBodyCatalog&& other) noexcept
    : m_ownGalaxyBodies(std::move(other.m_ownGalaxyBodies)), m_distantBodies(std::move(other.m_distantBodies)),
      m_orderedBodyIndexes(std::move(other.m_orderedBodyIndexes))
{
    rebuildOrderedBodies();
}

CelestialBodyCatalog& CelestialBodyCatalog::operator=(CelestialBodyCatalog&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_ownGalaxyBodies = std::move(other.m_ownGalaxyBodies);
    m_distantBodies = std::move(other.m_distantBodies);
    m_orderedBodyIndexes = std::move(other.m_orderedBodyIndexes);
    rebuildOrderedBodies();
    return *this;
}

CelestialBodyCatalog::~CelestialBodyCatalog() = default;

CelestialBodyCatalog::CelestialBodyCatalog(const std::span<const BaseCelestialBody* const> bodies)
{
    m_orderedBodyIndexes.reserve(bodies.size());
    for (const BaseCelestialBody* body : bodies) {
        if (body == nullptr) {
            continue;
        }

        if (body->kind == BaseCelestialBody::Kind::DeepSkyObject) {
            m_orderedBodyIndexes.push_back(
                OrderEntry{.domain = BodyDomain::Distant, .bodyIndex = m_distantBodies.size()}
            );
            m_distantBodies.push_back(toDistantBody(*body));
            continue;
        }

        m_orderedBodyIndexes.push_back(
            OrderEntry{.domain = BodyDomain::OwnGalaxy, .bodyIndex = m_ownGalaxyBodies.size()}
        );
        m_ownGalaxyBodies.push_back(toOwnGalaxyBody(*body));
    }
    rebuildOrderedBodies();
}

CelestialBodyCatalog::CelestialBodyCatalog(const std::span<const OwnGalaxyCelestialBody> ownGalaxyBodies)
    : m_ownGalaxyBodies(ownGalaxyBodies.begin(), ownGalaxyBodies.end())
{
    rebuildSequentialOrder();
    rebuildOrderedBodies();
}

CelestialBodyCatalog::CelestialBodyCatalog(std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies)
    : m_ownGalaxyBodies(std::move(ownGalaxyBodies))
{
    rebuildSequentialOrder();
    rebuildOrderedBodies();
}

CelestialBodyCatalog::CelestialBodyCatalog(
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies, std::vector<DistantCelestialBody> distantBodies
)
    : m_ownGalaxyBodies(std::move(ownGalaxyBodies)), m_distantBodies(std::move(distantBodies))
{
    rebuildSequentialOrder();
    rebuildOrderedBodies();
}

CelestialBodyCatalog::CelestialBodyCatalog(
    std::vector<OwnGalaxyCelestialBody> ownGalaxyBodies,
    std::vector<DistantCelestialBody> distantBodies,
    std::vector<OrderEntry> orderedBodyIndexes
)
    : m_ownGalaxyBodies(std::move(ownGalaxyBodies)), m_distantBodies(std::move(distantBodies)),
      m_orderedBodyIndexes(std::move(orderedBodyIndexes))
{
    rebuildOrderedBodies();
}

std::size_t CelestialBodyCatalog::size() const noexcept
{
    return m_orderedBodies.size();
}

bool CelestialBodyCatalog::empty() const noexcept
{
    return m_orderedBodies.empty();
}

std::span<const BaseCelestialBody* const> CelestialBodyCatalog::bodies() const noexcept
{
    return m_orderedBodies;
}

std::span<const OwnGalaxyCelestialBody> CelestialBodyCatalog::ownGalaxyBodies() const noexcept
{
    return m_ownGalaxyBodies;
}

std::span<const DistantCelestialBody> CelestialBodyCatalog::distantBodies() const noexcept
{
    return m_distantBodies;
}

const BaseCelestialBody& CelestialBodyCatalog::bodyAt(const std::size_t bodyIndex) const noexcept
{
    return *m_orderedBodies[bodyIndex];
}

void CelestialBodyCatalog::rebuildOrderedBodies()
{
    m_orderedBodies.clear();
    m_orderedBodies.reserve(m_orderedBodyIndexes.size());

    for (const OrderEntry& orderEntry : m_orderedBodyIndexes) {
        if (orderEntry.domain == BodyDomain::Distant) {
            m_orderedBodies.push_back(&m_distantBodies[orderEntry.bodyIndex]);
            continue;
        }
        m_orderedBodies.push_back(&m_ownGalaxyBodies[orderEntry.bodyIndex]);
    }
}

void CelestialBodyCatalog::rebuildSequentialOrder()
{
    m_orderedBodyIndexes.clear();
    m_orderedBodyIndexes.reserve(m_ownGalaxyBodies.size() + m_distantBodies.size());

    for (std::size_t bodyIndex = 0; bodyIndex < m_ownGalaxyBodies.size(); ++bodyIndex) {
        m_orderedBodyIndexes.push_back(OrderEntry{.domain = BodyDomain::OwnGalaxy, .bodyIndex = bodyIndex});
    }
    for (std::size_t bodyIndex = 0; bodyIndex < m_distantBodies.size(); ++bodyIndex) {
        m_orderedBodyIndexes.push_back(OrderEntry{.domain = BodyDomain::Distant, .bodyIndex = bodyIndex});
    }
}

}  // namespace skygate::ephemeris
