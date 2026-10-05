#pragma once

#include "catalog/CatalogSelectionOptions.hpp"
#include "catalog/CatalogSourceType.hpp"

#include <QString>

// Focused parse options carried from a configured source instance to the core
// catalog payload parser. Archive member selection names one member of a
// multi-catalog download; the schema hint states the schema the descriptor
// promises, so a changed provider payload fails explicitly instead of being
// parsed as whatever detection reports.
struct CatalogParseOptions final {
    skygate::ephemeris::CatalogSelectionOptions selectionOptions;
    // Descriptor archiveSelector. Empty means unspecified: the core selector
    // then accepts a single supported member and reports ambiguity otherwise.
    // Applies to ZIP payloads only; other containers ignore it.
    QString archiveMember;
    // Descriptor schemaHint. CatalogSourceType::Unknown means unspecified. A
    // concrete hint must match the schema detected in the decoded payload or
    // the parse fails; it never overrides detection.
    skygate::ephemeris::CatalogSourceType schemaHint = skygate::ephemeris::CatalogSourceType::Unknown;
};
