#include "CatalogCompositionPolicyRules.hpp"

namespace skygate::ephemeris {

bool CatalogCompositionPolicyRules::participates(
    const CatalogCompositionPolicy policy, const BaseCelestialBody::Kind kind
) noexcept
{
    switch (policy) {
    case CatalogCompositionPolicy::DeepSkyOnly:
    case CatalogCompositionPolicy::DeepSkyFallback:
        return kind == BaseCelestialBody::Kind::DeepSkyObject;
    case CatalogCompositionPolicy::AugmentCore:
        return kind != BaseCelestialBody::Kind::DeepSkyObject;
    case CatalogCompositionPolicy::Merge:
        return true;
    }
    return true;
}

bool CatalogCompositionPolicyRules::participates(
    const CatalogCompositionPolicy policy, const BaseCelestialBody& body
) noexcept
{
    return participates(policy, body.kind);
}

bool CatalogCompositionPolicyRules::contributesDeepSkyObjects(const CatalogCompositionPolicy policy) noexcept
{
    return participates(policy, BaseCelestialBody::Kind::DeepSkyObject);
}

}  // namespace skygate::ephemeris
