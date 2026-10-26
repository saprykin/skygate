#pragma once

#include <string_view>

namespace skygate::ephemeris::tests {

// Reusable semantic fixture corpus for the catalog interchangeability proof.
//
// The same four astronomical objects (two stars and two deep-sky objects) are
// encoded in three deliberately different schemas:
//
//   - HYG CSV for the two stars,
//   - OpenNGC CSV for the two deep-sky objects,
//   - the semantic tab-delimited schema (CatalogSemanticFixtureAdapter) for
//     all four objects in one mixed payload.
//
// Every encoding preserves the same consumer-visible fields (kind, magnitude,
// coordinates, astrometry or deep-sky extent, display name, common aliases,
// and namespaced cross-identifiers) while using a different canonical ID
// convention. Tests parse each encoding and assert the fields agree without
// catalog-specific consumer branches.
struct CatalogSemanticFixtureCorpus final {
    [[nodiscard]] static std::string_view hygStarPayload() noexcept;
    [[nodiscard]] static std::string_view openNgcDeepSkyPayload() noexcept;
    [[nodiscard]] static std::string_view semanticPayload() noexcept;

    // gzip-compressed semanticPayload(). Deterministic (mtime 0) so the bytes
    // are stable across test runs.
    [[nodiscard]] static std::string_view semanticGzipPayload() noexcept;
};

}  // namespace skygate::ephemeris::tests
