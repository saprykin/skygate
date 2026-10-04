#include "CatalogIdentityIndex.hpp"
#include "StringUtilities.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace skygate::ephemeris {
namespace {

using KeyMap = std::unordered_map<std::string, std::vector<std::size_t>>;

void dedupeAndSort(std::vector<std::size_t>& values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

std::string canonicalKey(const BaseCelestialBody& body)
{
    return StringUtilities::normalizedLookupKey(body.id);
}

std::vector<std::string> externalIdentifierKeys(const BaseCelestialBody& body)
{
    std::vector<std::string> keys;
    keys.reserve(body.identity.externalIdentifiers.size());
    for (const CatalogIdentifier& identifier : body.identity.externalIdentifiers) {
        if (!identifier.empty()) {
            keys.push_back(identifier.key());
        }
    }
    return keys;
}

std::vector<std::string> aliasKeys(const BaseCelestialBody& body)
{
    std::vector<std::string> keys;
    const auto appendKey = [&keys](const std::string_view value) {
        const std::string key = StringUtilities::normalizedAlnumKey(value);
        if (!key.empty()) {
            keys.push_back(key);
        }
    };

    appendKey(body.id);
    for (const std::string& alias : body.identity.aliases) {
        appendKey(alias);
    }
    if (const DeepSkyObjectInfo* deepSkyObject = body.deepSkyObjectInfo()) {
        for (const std::string& alias : deepSkyObject->aliases) {
            appendKey(alias);
        }
    }
    return keys;
}

void addKey(KeyMap& map, const std::string& key, const std::size_t resultIndex)
{
    if (key.empty()) {
        return;
    }

    std::vector<std::size_t>& entries = map[key];
    if (std::find(entries.begin(), entries.end(), resultIndex) == entries.end()) {
        entries.push_back(resultIndex);
    }
}

void addKeys(KeyMap& map, const std::vector<std::string>& keys, const std::size_t resultIndex)
{
    for (const std::string& key : keys) {
        addKey(map, key, resultIndex);
    }
}

void collectMatches(const KeyMap& map, const std::vector<std::string>& keys, std::vector<std::size_t>& matches)
{
    for (const std::string& key : keys) {
        const auto found = map.find(key);
        if (found == map.end()) {
            continue;
        }
        matches.insert(matches.end(), found->second.begin(), found->second.end());
    }
    dedupeAndSort(matches);
}

}  // namespace

bool CatalogIdentityIndex::Resolution::hasSingleMatch() const noexcept
{
    return kind == MatchKind::CanonicalId || kind == MatchKind::ExternalIdentifier || kind == MatchKind::Alias;
}

bool CatalogIdentityIndex::Resolution::isAuthoritative() const noexcept
{
    return kind == MatchKind::CanonicalId || kind == MatchKind::ExternalIdentifier
           || kind == MatchKind::AmbiguousAuthoritative;
}

bool CatalogIdentityIndex::Resolution::isAmbiguous() const noexcept
{
    return kind == MatchKind::AmbiguousAuthoritative || kind == MatchKind::AmbiguousAlias;
}

void CatalogIdentityIndex::add(const BaseCelestialBody& body, const std::size_t resultIndex)
{
    addKey(m_canonicalIndex, canonicalKey(body), resultIndex);
    addKeys(m_identifierIndex, externalIdentifierKeys(body), resultIndex);
    if (body.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        addKeys(m_aliasIndex, aliasKeys(body), resultIndex);
    }
}

CatalogIdentityIndex::Resolution CatalogIdentityIndex::resolve(const BaseCelestialBody& body) const
{
    Resolution resolution;

    std::vector<std::size_t> authoritative;
    collectMatches(m_canonicalIndex, {canonicalKey(body)}, authoritative);
    const bool canonicalMatched = !authoritative.empty();
    collectMatches(m_identifierIndex, externalIdentifierKeys(body), authoritative);

    if (authoritative.size() == 1U) {
        resolution.kind =
            canonicalMatched ? Resolution::MatchKind::CanonicalId : Resolution::MatchKind::ExternalIdentifier;
        resolution.index = authoritative.front();
        return resolution;
    }
    if (authoritative.size() > 1U) {
        resolution.kind = Resolution::MatchKind::AmbiguousAuthoritative;
        resolution.candidates = std::move(authoritative);
        return resolution;
    }

    if (body.kind != BaseCelestialBody::Kind::DeepSkyObject) {
        return resolution;
    }

    std::vector<std::size_t> aliases;
    collectMatches(m_aliasIndex, aliasKeys(body), aliases);
    if (aliases.size() == 1U) {
        resolution.kind = Resolution::MatchKind::Alias;
        resolution.index = aliases.front();
        return resolution;
    }
    if (aliases.size() > 1U) {
        resolution.kind = Resolution::MatchKind::AmbiguousAlias;
        resolution.candidates = std::move(aliases);
    }
    return resolution;
}

}  // namespace skygate::ephemeris
