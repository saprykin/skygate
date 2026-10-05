#pragma once

#include "SkyCatalogSourceDescriptor.hpp"

#include <QString>
#include <QStringList>

namespace skygate::ui::internal {

// A configured source instance. instanceId is the durable identity allocated
// when the instance is created; descriptorId records which preset template, if
// any, the instance was configured from. Nothing in the instance ID is derived
// from the descriptor, URL, version, or title, so two instances of one
// descriptor and two versions or member selections at one URL remain distinct.
struct SkyCatalogSourceInstance final {
    QString instanceId;
    QString descriptorId;
    QString title;
    QString version;
    QStringList urls;
    skygate::ephemeris::CatalogSourceType schemaHint = skygate::ephemeris::CatalogSourceType::Unknown;
    QString archiveSelector;
    QStringList relatedDatasetUrls;
    QString attribution;

    // Allocates a fresh durable instance ID. The ID is opaque and stays fixed
    // for the lifetime of the configured instance: reload, reorder, cache,
    // provenance, and restart preserve it instead of re-deriving it from
    // source metadata.
    [[nodiscard]] static QString allocateInstanceId();

    // Deterministic identity adopted for a record migrated from pre-durable-ID
    // persistence. The legacy preset/URL derivation is applied exactly once at
    // this migration boundary so repeated migration keeps the same IDs.
    [[nodiscard]] static QString migratedLegacyInstanceId(const SkyCatalogSourceInstance& instance);

    [[nodiscard]] static SkyCatalogSourceInstance fromDescriptor(const SkyCatalogSourceDescriptor& descriptor);
    [[nodiscard]] static SkyCatalogSourceInstance createCustom(const QString& urlText, const QString& version = {});

    [[nodiscard]] bool isCustom() const noexcept;
};

}  // namespace skygate::ui::internal
