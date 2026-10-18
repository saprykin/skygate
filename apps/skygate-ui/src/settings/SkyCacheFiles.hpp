#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace skygate::ui::internal {

class SkyCacheFiles final {
public:
    enum class ReadState {
        Absent,
        Unreadable,
        Ok,
    };

    struct ReadResult final {
        ReadState state = ReadState::Absent;
        QByteArray payload;
        QString errorString;
    };

    [[nodiscard]] static QString appDataFilePath(const QString& fileName);
    static void appendUniquePath(QStringList& paths, const QString& path);
    [[nodiscard]] static bool removeFiles(const QStringList& paths, const QString& logNoun);
    [[nodiscard]] static bool
    writeFileAtomically(const QString& path, const QByteArray& payload, const QString& logNoun);
    [[nodiscard]] static ReadResult readFile(const QString& path);
};

}  // namespace skygate::ui::internal
