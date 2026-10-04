#pragma once

#include "catalog/CatalogSourceType.hpp"

namespace skygate::ephemeris::tests {

// Test-only catalog schema adapter deliberately different from the production
// HYG and OpenNGC schemas.
//
// The adapter uses a tab delimiter, unrelated column names, a mixed
// star/deep-sky payload shape, and its own canonical ID convention
// (star_<key> / dso_<key>) while producing the same body kinds and
// cross-identifiers (hip, hyg, messier, ngc, ic) as the production parsers.
// This lets integration tests prove that parsing, containers, composition,
// provenance, cache restore, and the render/search/inspector/ephemeris
// consumers do not depend on which catalog supplied an object.
//
// The adapter lives only in the test-support library; there is no production
// source using this schema.
class CatalogSemanticFixtureAdapter final {
public:
    // Registers the semantic schema with CatalogSchemaRegistry exactly once per
    // process and returns its stable CatalogSourceType.
    [[nodiscard]] static skygate::ephemeris::CatalogSourceType schemaType() noexcept;
};

}  // namespace skygate::ephemeris::tests
