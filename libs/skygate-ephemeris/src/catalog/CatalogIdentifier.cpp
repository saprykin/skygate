#include "CatalogIdentifier.hpp"

#include "StringUtilities.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {
namespace {

bool isNumericIdentifierNamespace(const std::string_view namespaceName)
{
    return namespaceName == "hip" || namespaceName == "hyg" || namespaceName == "ngc" || namespaceName == "ic"
           || namespaceName == "messier";
}

std::string stripLeadingZeros(std::string value)
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

    normalized = stripLeadingZeros(std::move(normalized));
    if (namespaceName == "messier" && normalized.size() < 3U) {
        normalized.insert(0U, 3U - normalized.size(), '0');
    }
    return normalized;
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
