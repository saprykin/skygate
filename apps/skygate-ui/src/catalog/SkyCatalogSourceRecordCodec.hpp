#pragma once

#include "SkyCatalogSourceCacheRecord.hpp"

#include <QString>
#include <QStringList>

class QSettings;

namespace skygate::ui::internal {

// Reads and writes one source's settings record, together with the payload
// sidecar references it carries. Records written before the record format
// gained a parse contract load with the defined defaults, and a stored policy
// or schema hint this build does not know falls back instead of failing the
// source.
class SkyCatalogSourceRecordCodec final {
public:
    // Root group of every stored source record. Records written before
    // generations were published keep their group directly in it; committed
    // generations nest their records one level deeper.
    [[nodiscard]] static QString sourcesGroup();
    // A non-numeric group under catalogSources is a sidecar-named flat record,
    // so its presence proves the pre-generation records are still readable.
    [[nodiscard]] static bool hasFlatRecords(QSettings& settings);

    [[nodiscard]] static QString sourceGroup(const QString& parentGroup, const QString& instanceId);

    static void saveRecord(
        QSettings& settings,
        const QString& group,
        const SkyCatalogSourceCacheRecord& record,
        const QString& rawPayloadPath,
        const QString& binaryPayloadPath
    );
    [[nodiscard]] static SkyCatalogSourceCacheRecord
    loadRecord(QSettings& settings, const QString& group, QStringList& unreadablePayloadInstanceIds);
    // Drops the payload references and the related dataset they carry while the
    // configuration fields of the record stay durable.
    static void evictPayload(QSettings& settings, const QString& group);
};

}  // namespace skygate::ui::internal
