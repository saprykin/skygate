#include "CatalogIdentifier.hpp"

#include "StringUtilities.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {
namespace {

bool isNumericIdentifierNamespace(const std::string_view namespaceName)
{
    return namespaceName == "hip" || namespaceName == "hyg"
           || CatalogIdentifier::isDeepSkyDesignationNamespace(namespaceName);
}

}  // namespace

CatalogIdentifier CatalogIdentifier::make(std::string namespaceName, std::string value)
{
    std::string normalizedNamespace = StringUtilities::toLowerAscii(namespaceName);
    std::string normalizedValue = normalizeValue(normalizedNamespace, value);
    return CatalogIdentifier{
        .namespaceName = std::move(normalizedNamespace),
        .value = std::move(normalizedValue),
    };
}

std::string CatalogIdentifier::normalizeValue(const std::string_view namespaceName, const std::string_view value)
{
    std::string normalized(StringUtilities::trimAsciiWhitespace(value));
    if (!isNumericIdentifierNamespace(namespaceName)) {
        return normalized;
    }

    // Catalog numbers are case-insensitive: NGC 1234A and ngc1234a denote the
    // same object, so fold the alphabetic component suffix together with the
    // namespace before padding/stripping is applied.
    normalized = StringUtilities::toLowerAscii(normalized);
    normalized = stripLeadingZeros(std::move(normalized));
    if (namespaceName == "messier" && normalized.size() < 3U) {
        normalized.insert(0U, 3U - normalized.size(), '0');
    }
    return normalized;
}

std::string CatalogIdentifier::bodyId(std::string namespaceName, std::string value)
{
    const CatalogIdentifier identifier = make(std::move(namespaceName), std::move(value));
    if (identifier.empty()) {
        return {};
    }
    return identifier.namespaceName + "_" + identifier.value;
}

bool CatalogIdentifier::isDeepSkyDesignationNamespace(const std::string_view namespaceName) noexcept
{
    return StringUtilities::equalsIgnoreAsciiCase(namespaceName, "ngc")
           || StringUtilities::equalsIgnoreAsciiCase(namespaceName, "ic")
           || StringUtilities::equalsIgnoreAsciiCase(namespaceName, "messier");
}

std::optional<CatalogIdentifier> CatalogIdentifier::designationFromCanonicalId(const std::string_view canonicalId)
{
    const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(canonicalId);
    const std::size_t separator = trimmed.find('_');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    const std::string_view namespaceName = trimmed.substr(0U, separator);
    const std::string_view value = trimmed.substr(separator + 1U);
    if (value.empty() || !isDeepSkyDesignationNamespace(namespaceName)) {
        return std::nullopt;
    }
    return make(std::string(namespaceName), std::string(value));
}

bool CatalogIdentifier::appendUnique(std::vector<CatalogIdentifier>& identifiers, CatalogIdentifier identifier)
{
    if (identifier.empty()) {
        return false;
    }
    if (std::find(identifiers.begin(), identifiers.end(), identifier) != identifiers.end()) {
        return false;
    }

    identifiers.push_back(std::move(identifier));
    return true;
}

std::string CatalogIdentifier::stripLeadingZeros(std::string value)
{
    if (value.empty()) {
        return value;
    }

    const std::size_t firstNonZero = value.find_first_not_of('0');
    if (firstNonZero == std::string::npos) {
        return "0";
    }
    if (firstNonZero > 0U) {
        value.erase(0U, firstNonZero);
    }
    return value;
}

std::string CatalogIdentifier::key() const
{
    return namespaceName + ":" + value;
}

bool CatalogIdentifier::empty() const noexcept
{
    return namespaceName.empty() || value.empty();
}

bool CatalogIdentifier::operator<(const CatalogIdentifier& other) const
{
    if (namespaceName != other.namespaceName) {
        return namespaceName < other.namespaceName;
    }
    return value < other.value;
}

}  // namespace skygate::ephemeris
