#pragma once

#include "BaseCelestialBody.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace skygate::ephemeris {

// Incremental identity index used to resolve canonical and external
// identifiers, and deep-sky aliases, to a stable result position.
//
// Each registered body contributes three families of keys:
//
//   - canonical id keys (case/whitespace-normalized body.id plus every
//     retained canonical id of its CatalogObjectIdentity),
//   - external identifier keys ("<namespace>:<value>", see CatalogIdentifier),
//   - deep-sky alias keys (alphanumeric-normalized alias strings plus the
//     alphanumeric-normalized canonical id).
//
// Retained canonical ids stay in the authoritative canonical family: a
// replacement keeps its earlier canonical keys resolvable without demoting
// them to weak display aliases.
//
// Display names and sky positions are deliberately excluded so unrelated
// same-named objects stay distinct. A key can map to several result positions:
// a deep-sky alias is legitimately shared, while a canonical id or external
// identifier shared by several positions indicates contradictory source data.
// resolve() reports those cases separately so callers can merge, keep objects
// distinct, or record a diagnostic.
//
// The index is meant to stay current while a merge proceeds: add() unions the
// keys of merged bodies, and remove() drops every key of a position that a
// replacement survivor vacated. A stale entry would let a later row resolve to
// a body that no longer survives the merge.
class CatalogIdentityIndex final {
public:
    struct Resolution {
        enum class MatchKind {
            None,
            CanonicalId,
            ExternalIdentifier,
            Alias,
            AmbiguousAuthoritative,
            AmbiguousAlias
        };

        MatchKind kind = MatchKind::None;
        // Position of the single matched body; meaningful when kind is
        // CanonicalId, ExternalIdentifier, or Alias.
        std::size_t index = 0;
        // Distinct positions matched when the resolution is ambiguous.
        std::vector<std::size_t> candidates;

        [[nodiscard]] bool hasSingleMatch() const noexcept;
        [[nodiscard]] bool isAuthoritative() const noexcept;
        [[nodiscard]] bool isAmbiguous() const noexcept;
    };

    // An authoritative identity key registered for several result positions.
    struct DuplicateIdentity {
        std::string key;
        std::vector<std::size_t> positions;
    };

    // Registers `body` under `resultIndex`. Keys already present for the same
    // result position are not duplicated, so a merged body can be re-added
    // safely after its identity union has grown.
    void add(const BaseCelestialBody& body, std::size_t resultIndex);

    // Removes every key registered for `resultIndex`, so a vacated position
    // can no longer be matched by later rows. Removing a position that was
    // never registered is a no-op.
    void remove(std::size_t resultIndex);

    // Pre-sizes the internal key maps for the expected number of registered
    // bodies. Registration without an up-front reserve remains correct; this
    // only avoids repeated rehashing when a large catalog is indexed at once.
    void reserve(std::size_t bodyCount);

    // Resolves `body` against the bodies registered so far. Authoritative keys
    // (canonical id, retained canonical ids, and external identifiers) are
    // preferred over deep-sky aliases.
    [[nodiscard]] Resolution resolve(const BaseCelestialBody& body) const;

    // Reports the authoritative keys registered for several positions, sorted
    // by key then by position list so validation diagnostics are deterministic.
    [[nodiscard]] std::vector<DuplicateIdentity> duplicateAuthoritativeIdentities() const;

private:
    using KeyMap = std::unordered_map<std::string, std::vector<std::size_t>>;

    enum class KeyFamily {
        Canonical,
        ExternalIdentifier,
        Alias
    };

    struct RegisteredKey {
        KeyFamily family = KeyFamily::Canonical;
        std::string key;
    };

    [[nodiscard]] KeyMap& keyMap(KeyFamily family) noexcept;
    void registerKey(std::size_t resultIndex, KeyFamily family, const std::string& key);

    KeyMap m_canonicalIndex;
    KeyMap m_identifierIndex;
    KeyMap m_aliasIndex;
    // Keys registered for each result position, so remove() drops a vacated
    // position without scanning the key maps.
    std::vector<std::vector<RegisteredKey>> m_keysByPosition;
};

}  // namespace skygate::ephemeris
