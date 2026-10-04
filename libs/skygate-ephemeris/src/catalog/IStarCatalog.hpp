#pragma once

#include "BaseCelestialBody.hpp"
#include "CelestialBodyCatalog.hpp"

#include <span>

namespace skygate::ephemeris {

// Eager, immutable catalog snapshot provider.
//
// A provider materializes a complete in-memory snapshot of catalog bodies once
// during loading or construction. Loading and provider substitution end at
// that snapshot; downstream consumers read the common body view exposed by
// bodies() and do not depend on how a provider stores or obtains the bodies.
//
// The provider owns the bodies for its lifetime. The span returned by bodies()
// and the indexes it yields remain valid for the lifetime of the provider and
// are not invalidated by ownership transfers within the provider.
//
// catalog() exposes the concrete CelestialBodyCatalog representation with its
// typed storage domains and order entries. That representation-specific access
// is reserved for model construction and serialization; ordinary consumers
// should use bodies().
//
// This interface intentionally supports only eager materialization: every
// provider must be able to produce the complete snapshot. Lazy, paged, or
// remote storage that cannot materialize the full snapshot is outside the
// current contract.
class IStarCatalog {
public:
    virtual ~IStarCatalog() = default;
    [[nodiscard]] virtual const CelestialBodyCatalog& catalog() const noexcept = 0;
    [[nodiscard]] virtual std::span<const BaseCelestialBody* const> bodies() const = 0;
};

}  // namespace skygate::ephemeris
