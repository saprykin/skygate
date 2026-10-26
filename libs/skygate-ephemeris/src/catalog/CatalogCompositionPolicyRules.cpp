#include "CatalogCompositionPolicyRules.hpp"

namespace skygate::ephemeris {

bool CatalogCompositionPolicyRules::isKnown(const CatalogCompositionPolicy policy) noexcept
{
    switch (policy) {
    case CatalogCompositionPolicy::Merge:
    case CatalogCompositionPolicy::DeepSkyOnly:
    case CatalogCompositionPolicy::AugmentCore:
    case CatalogCompositionPolicy::DeepSkyFallback:
        return true;
    }
    return false;
}

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
    return policy == CatalogCompositionPolicy::DeepSkyOnly || policy == CatalogCompositionPolicy::DeepSkyFallback;
}

bool CatalogCompositionPolicyRules::isGapFill(const CatalogCompositionPolicy policy) noexcept
{
    switch (policy) {
    case CatalogCompositionPolicy::AugmentCore:
    case CatalogCompositionPolicy::DeepSkyFallback:
        return true;
    case CatalogCompositionPolicy::Merge:
    case CatalogCompositionPolicy::DeepSkyOnly:
        return false;
    }
    return false;
}

bool CatalogCompositionPolicyRules::enablesBundledStarFallback(const CatalogCompositionPolicy policy) noexcept
{
    switch (policy) {
    case CatalogCompositionPolicy::AugmentCore:
        return true;
    case CatalogCompositionPolicy::Merge:
    case CatalogCompositionPolicy::DeepSkyOnly:
    case CatalogCompositionPolicy::DeepSkyFallback:
        return false;
    }
    return false;
}

}  // namespace skygate::ephemeris
