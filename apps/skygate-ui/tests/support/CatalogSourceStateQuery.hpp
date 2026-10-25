#pragma once

#include "catalog/SkyCatalogManager.hpp"

#include <QString>

#include <algorithm>

namespace skygate::ui::tests {

// The manager publishes its configured sources as the collection rows the
// source list shows, so the suites read per-source state from the same rows
// instead of a separate per-instance accessor.
[[nodiscard]] inline bool isCatalogSourceEnabled(const SkyCatalogManager& manager, const QString& instanceId)
{
    const QVector<SkyCatalogManager::SourceViewEntry> entries = manager.sourceViewEntries();
    const auto entry = std::find_if(entries.cbegin(), entries.cend(), [&instanceId](const auto& candidate) {
        return candidate.instanceId == instanceId;
    });
    return entry != entries.cend() && entry->enabled;
}

}  // namespace skygate::ui::tests
