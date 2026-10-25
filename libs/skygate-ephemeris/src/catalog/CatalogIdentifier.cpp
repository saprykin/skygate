#include "CatalogIdentifier.hpp"

#include "StringUtilities.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace skygate::ephemeris {
namespace {

struct DesignationPrefix final {
    std::string_view text;
    std::string_view namespaceName;
};

constexpr DesignationPrefix kDesignationPrefixes[] = {
    {.text = "NGC", .namespaceName = CatalogIdentifier::kNgcNamespace},
    {.text = "IC", .namespaceName = CatalogIdentifier::kIcNamespace},
    {.text = "M", .namespaceName = CatalogIdentifier::kMessierNamespace},
};

[[nodiscard]] bool isNumericIdentifierNamespace(const std::string_view namespaceName)
{
    return namespaceName == CatalogIdentifier::kHipNamespace || namespaceName == CatalogIdentifier::kHygNamespace
           || CatalogIdentifier::isDeepSkyDesignationNamespace(namespaceName);
}

[[nodiscard]] bool isAsciiDigit(const char character) noexcept
{
    return character >= '0' && character <= '9';
}

[[nodiscard]] bool isAsciiLetter(const char character) noexcept
{
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
}

}  // namespace

CatalogIdentifier CatalogIdentifier::make(const std::string_view namespaceName, const std::string_view value)
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
    if (namespaceName == kMessierNamespace && normalized.size() < 3U) {
        normalized.insert(0U, 3U - normalized.size(), '0');
    }
    return normalized;
}

std::string CatalogIdentifier::bodyId(const std::string_view namespaceName, const std::string_view value)
{
    const CatalogIdentifier identifier = make(namespaceName, value);
    if (identifier.empty()) {
        return {};
    }
    return identifier.namespaceName + "_" + identifier.value;
}

std::string CatalogIdentifier::hipBodyId(const std::uint64_t hipNumber)
{
    return bodyId(kHipNamespace, std::to_string(hipNumber));
}

bool CatalogIdentifier::isDeepSkyDesignationNamespace(const std::string_view namespaceName) noexcept
{
    return StringUtilities::equalsIgnoreAsciiCase(namespaceName, kNgcNamespace)
           || StringUtilities::equalsIgnoreAsciiCase(namespaceName, kIcNamespace)
           || StringUtilities::equalsIgnoreAsciiCase(namespaceName, kMessierNamespace);
}

std::optional<CatalogIdentifier> CatalogIdentifier::designationFromText(const std::string_view text)
{
    const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(text);
    for (const DesignationPrefix& prefix : kDesignationPrefixes) {
        if (trimmed.size() <= prefix.text.size()
            || !StringUtilities::equalsIgnoreAsciiCase(trimmed.substr(0U, prefix.text.size()), prefix.text)) {
            continue;
        }

        std::string_view remainder = trimmed.substr(prefix.text.size());
        if (!remainder.empty() && remainder.front() == ' ') {
            remainder.remove_prefix(1U);
        }
        if (remainder.empty() || !isAsciiDigit(remainder.front())) {
            continue;
        }

        std::size_t index = 0U;
        while (index < remainder.size() && isAsciiDigit(remainder[index])) {
            ++index;
        }
        while (index < remainder.size() && isAsciiLetter(remainder[index])) {
            ++index;
        }
        if (index != remainder.size()) {
            continue;
        }

        return make(prefix.namespaceName, remainder);
    }

    return std::nullopt;
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
    if (value.empty()) {
        return std::nullopt;
    }

    const std::string_view prefix = designationPrefix(namespaceName);
    if (prefix.empty()) {
        return std::nullopt;
    }

    std::string designationText(prefix);
    designationText.push_back(' ');
    designationText.append(value);
    return designationFromText(designationText);
}

std::string_view CatalogIdentifier::designationPrefix(const std::string_view namespaceName) noexcept
{
    for (const DesignationPrefix& prefix : kDesignationPrefixes) {
        if (StringUtilities::equalsIgnoreAsciiCase(namespaceName, prefix.namespaceName)) {
            return prefix.text;
        }
    }
    return {};
}

std::string CatalogIdentifier::normalizedDesignationText(const std::string_view text)
{
    const std::string_view trimmed = StringUtilities::trimAsciiWhitespace(text);
    const std::optional<CatalogIdentifier> designation = designationFromText(trimmed);
    if (!designation.has_value()) {
        return std::string(trimmed);
    }

    const std::string_view prefix = designationPrefix(designation->namespaceName);
    std::string_view suffix = trimmed.substr(prefix.size());
    if (!suffix.empty() && suffix.front() == ' ') {
        suffix.remove_prefix(1U);
    }
    if (suffix.empty()) {
        return std::string(trimmed);
    }

    std::string normalized(prefix);
    normalized.push_back(' ');
    normalized.append(stripLeadingZeros(std::string(suffix)));
    return normalized;
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

}  // namespace skygate::ephemeris
