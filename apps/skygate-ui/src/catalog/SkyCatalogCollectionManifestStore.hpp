#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <cstdint>
#include <optional>

class QSettings;

namespace skygate::ui::internal {

// Owns the generation manifest of the durable catalog collection: the file that
// names the committed generation, the durable proof that a manifest was
// published, and the removal of the files and records a newer commit left
// unreferenced. The manifest is published only after every payload file and
// record of its generation is durable, so its presence alone decides which
// snapshot is active.
class SkyCatalogCollectionManifestStore final {
public:
    enum class State : std::uint8_t {
        Absent,
        Unreadable,
        Readable,
    };

    struct Manifest final {
        int schemaVersion = 0;
        int binarySchemaVersion = 0;
        quint64 generation = 0;
    };

    struct ReadResult final {
        State state = State::Absent;
        Manifest manifest;
    };

    [[nodiscard]] static QString path(const QString& directory);
    [[nodiscard]] static ReadResult read(const QString& directory);
    [[nodiscard]] static bool publish(const QString& directory, const Manifest& manifest);

    // The generation a save must use next. A save interrupted between its
    // staging steps can leave records or sidecar files from a generation the
    // manifest does not name yet; reusing such a number would overwrite staged
    // data, so every number still observable is skipped.
    [[nodiscard]] static quint64
    nextGeneration(const QString& directory, QSettings& settings, quint64 committedGeneration);

    // Settings group holding the records of one generation.
    [[nodiscard]] static QString generationGroup(quint64 generation);
    // Clears leftover data of the generation and marks it as holding stored
    // records. The marker is what proves the records of a generation were
    // written before the manifest named it.
    static void stageGeneration(QSettings& settings, quint64 generation);
    static void discardGeneration(QSettings& settings, quint64 generation);
    [[nodiscard]] static bool hasRecordsStored(QSettings& settings, quint64 generation);

    // Generation-format records live in a numeric group under catalogSources,
    // while the flat groups written before generations were published are named
    // after their sidecar. A numeric group therefore marks the settings file as
    // generation-format even when the manifest is gone.
    [[nodiscard]] static bool hasGenerationRecords(QSettings& settings);
    // Generation-format data exists: records in a numeric group or sidecar
    // files that carry a generation. Leftovers of an interrupted save count,
    // because the caller reports them instead of assuming first use.
    [[nodiscard]] static bool hasGenerationArtifacts(const QString& directory, QSettings& settings);

    // Durable proof that a manifest was published at least once, with the last
    // committed generation as the recorded recovery point. It is recorded after
    // publication, so a staged generation that was never published cannot
    // masquerade as committed configuration.
    [[nodiscard]] static std::optional<quint64> committedGeneration(QSettings& settings);
    static void recordCommittedGeneration(QSettings& settings, quint64 generation);
    static void forgetCommittedGeneration(QSettings& settings);

    // Every cache file the collection owns in one directory: the sidecars of
    // every generation and the manifest.
    [[nodiscard]] static QStringList cacheFilePaths(const QString& directory);
    // Drops the sidecar files and record groups the committed generation does
    // not reference. Leftovers cannot change which generation is committed, so
    // removal stays best effort.
    static void removeObsolete(
        QSettings& settings, const QString& directory, quint64 committedGeneration, const QStringList& referencedPaths
    );
};

}  // namespace skygate::ui::internal
