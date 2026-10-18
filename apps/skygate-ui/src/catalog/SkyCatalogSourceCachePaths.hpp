#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>

class QSettings;

namespace skygate::ui::internal {

// Names used by the durable catalog collection cache: the directory that holds
// the committed manifest and the generation-scoped payload sidecars, the
// sidecar paths of one source instance, and the parts of a sidecar name a
// cleanup pass needs to read back.
class SkyCatalogSourceCachePaths final {
public:
    [[nodiscard]] static QString defaultDirectory();
    // Configured directory, or the default when the setting is missing or
    // empty.
    [[nodiscard]] static QString directory(QSettings& settings);

    [[nodiscard]] static QString idDigest(const QString& instanceId);
    [[nodiscard]] static QString fileStem(const QString& instanceId);
    // Sidecar names embed the generation that produced them, so staging a new
    // snapshot never overwrites the files the committed records still
    // reference.
    [[nodiscard]] static QString generationFileStem(const QString& instanceId, quint64 generation);
    [[nodiscard]] static QString
    rawSidecarPath(const QString& directory, const QString& instanceId, quint64 generation);
    [[nodiscard]] static QString
    binarySidecarPath(const QString& directory, const QString& instanceId, quint64 generation);

    // Entry filter matching every payload sidecar of a collection directory.
    [[nodiscard]] static QString sidecarEntryPattern();
    // Generation a sidecar name carries, as written by generationFileStem.
    [[nodiscard]] static std::optional<quint64> generationFromSidecarName(const QString& fileName);
};

}  // namespace skygate::ui::internal
