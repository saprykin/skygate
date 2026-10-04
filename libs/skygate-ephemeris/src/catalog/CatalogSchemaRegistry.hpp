#pragma once

#include "CatalogSchemaDescriptor.hpp"

#include <memory>
#include <string>
#include <vector>

namespace skygate::ephemeris {

// Single source of truth for the supported catalog schemas. Header detection,
// parser selection, and parse diagnostics all read from this descriptor table,
// so adding a schema is one descriptor registration rather than independent
// updates to detection, loader, and diagnostic switches.
class CatalogSchemaRegistry final {
public:
    CatalogSchemaRegistry() = delete;

    // All registered schemas in detection precedence order.
    [[nodiscard]] static const std::vector<CatalogSchemaDescriptor>& descriptors() noexcept;

    // Descriptor for a schema, or nullptr when the type is not a schema.
    [[nodiscard]] static const CatalogSchemaDescriptor* find(CatalogSourceType type) noexcept;

    // Diagnostic label for a schema, or "unknown" when it has no descriptor.
    [[nodiscard]] static std::string diagnosticName(CatalogSourceType type);

    // Constructs the parser adapter for a schema, or nullptr when unknown.
    [[nodiscard]] static std::unique_ptr<ICatalogParser> createParser(CatalogSourceType type);

    // Registers an additional schema so detection, loading, and diagnostics
    // recognize it. Intended for test adapters and future production sources;
    // callers must not register during concurrent detection or parsing.
    static void registerSchema(CatalogSchemaDescriptor descriptor);
};

}  // namespace skygate::ephemeris
