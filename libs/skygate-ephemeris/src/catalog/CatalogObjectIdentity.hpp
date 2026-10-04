#pragma once

#include "catalog/CatalogIdentifier.hpp"

#include <string>
#include <vector>

namespace skygate::ephemeris {

// Source-record identity and cross-identifiers attached to a composed body.
//
// BaseCelestialBody::id is the canonical object identity: the stable domain
// key used by rendering, search, and ephemeris consumers. It is assigned from
// the highest-priority source identifier at parse time (hip_<n>, hyg_<n>,
// ngc_<n>, messier_<nnn>, or a documented fallback) and is never derived from
// a display name or sky position. Planet IDs such as "mercury" remain their
// domain identity.
//
// This record preserves the source facts separately from the canonical id:
//
//   - sourceRecordId: the raw source record key (the HYG "id" column, the
//     OpenNGC "Name" column, or a bundled entry id). It is stable when the
//     same source is reloaded and stays distinct from any composed canonical
//     id assigned during parsing or composition.
//
//   - externalIdentifiers: namespaced cross-identifiers (hip, hyg, ngc, ic,
//     messier, ...). Values are normalized per namespace so equivalent
//     spellings match and equal numerics in different namespaces never
//     collide.
//
//   - aliases: display aliases (proper names, common names, designations).
//     For deep-sky objects DeepSkyObjectInfo::aliases mirrors this list so
//     existing DSO consumers keep reading the legacy field; the body-level
//     list is authoritative for identity resolution and merging.
struct CatalogObjectIdentity {
    std::string sourceRecordId;
    std::vector<CatalogIdentifier> externalIdentifiers;
    std::vector<std::string> aliases;
};

}  // namespace skygate::ephemeris
