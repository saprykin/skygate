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

// Every canonical key the body is known by: its public canonical id plus the
// canonical ids a replacement retained as equivalent. Retained ids resolve
// like canonical ids, so they stay authoritative instead of degrading to a
// weak display alias.
std::vector<std::string> canonicalKeys(const BaseCelestialBody& body)
{
    std::vector<std::string> keys;
    keys.reserve(body.identity.retainedCanonicalIds.size() + 1U);
    keys.push_back(StringUtilities::normalizedLookupKey(body.id));
    for (const std::string& retainedId : body.identity.retainedCanonicalIds) {
        keys.push_back(StringUtilities::normalizedLookupKey(retainedId));
    }
    return keys;
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

CatalogIdentityIndex::KeyMap& CatalogIdentityIndex::keyMap(const KeyFamily family) noexcept
{
    switch (family) {
    case KeyFamily::Canonical:
        return m_canonicalIndex;
    case KeyFamily::ExternalIdentifier:
        return m_identifierIndex;
    case KeyFamily::Alias:
        return m_aliasIndex;
    }
    return m_canonicalIndex;
}

void CatalogIdentityIndex::registerKey(const std::size_t resultIndex, const KeyFamily family, const std::string& key)
{
    if (key.empty()) {
        return;
    }

    if (m_keysByPosition.size() <= resultIndex) {
        m_keysByPosition.resize(resultIndex + 1U);
    }

    std::vector<RegisteredKey>& registered = m_keysByPosition[resultIndex];
    const bool alreadyRegistered =
        std::any_of(registered.begin(), registered.end(), [&family, &key](const RegisteredKey& entry) {
            return entry.family == family && entry.key == key;
        });
    if (alreadyRegistered) {
        return;
    }

    registered.push_back(RegisteredKey{.family = family, .key = key});
    keyMap(family)[key].push_back(resultIndex);
}

void CatalogIdentityIndex::add(const BaseCelestialBody& body, const std::size_t resultIndex)
{
    for (const std::string& key : canonicalKeys(body)) {
        registerKey(resultIndex, KeyFamily::Canonical, key);
    }
    for (const std::string& key : externalIdentifierKeys(body)) {
        registerKey(resultIndex, KeyFamily::ExternalIdentifier, key);
    }
    if (body.kind == BaseCelestialBody::Kind::DeepSkyObject) {
        for (const std::string& key : aliasKeys(body)) {
            registerKey(resultIndex, KeyFamily::Alias, key);
        }
    }
}

void CatalogIdentityIndex::remove(const std::size_t resultIndex)
{
    if (resultIndex >= m_keysByPosition.size()) {
        return;
    }

    for (const RegisteredKey& registered : m_keysByPosition[resultIndex]) {
        KeyMap& map = keyMap(registered.family);
        const auto found = map.find(registered.key);
        if (found == map.end()) {
            continue;
        }

        std::vector<std::size_t>& positions = found->second;
        positions.erase(std::remove(positions.begin(), positions.end(), resultIndex), positions.end());
        if (positions.empty()) {
            map.erase(found);
        }
    }
    m_keysByPosition[resultIndex].clear();
}

void CatalogIdentityIndex::reserve(const std::size_t bodyCount)
{
    m_canonicalIndex.reserve(bodyCount);
    m_identifierIndex.reserve(bodyCount * 2U);
    m_aliasIndex.reserve(bodyCount);
    m_keysByPosition.reserve(bodyCount);
}

CatalogIdentityIndex::Resolution CatalogIdentityIndex::resolve(const BaseCelestialBody& body) const
{
    Resolution resolution;

    std::vector<std::size_t> authoritative;
    collectMatches(m_canonicalIndex, canonicalKeys(body), authoritative);
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

std::vector<CatalogIdentityIndex::DuplicateIdentity> CatalogIdentityIndex::duplicateAuthoritativeIdentities() const
{
    std::vector<DuplicateIdentity> duplicates;
    const auto collectDuplicates = [&duplicates](const KeyMap& map) {
        for (const auto& [key, positions] : map) {
            if (positions.size() > 1U) {
                duplicates.push_back(DuplicateIdentity{.key = key, .positions = positions});
            }
        }
    };
    collectDuplicates(m_canonicalIndex);
    collectDuplicates(m_identifierIndex);

    for (DuplicateIdentity& duplicate : duplicates) {
        dedupeAndSort(duplicate.positions);
    }
    std::sort(duplicates.begin(), duplicates.end(), [](const DuplicateIdentity& lhs, const DuplicateIdentity& rhs) {
        if (lhs.key != rhs.key) {
            return lhs.key < rhs.key;
        }
        return lhs.positions < rhs.positions;
    });
    return duplicates;
}

}  // namespace skygate::ephemeris
