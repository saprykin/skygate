#include "SkyCacheFiles.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateCacheFilesLog, "skygate.catalog.cache")

}  // namespace

QString SkyCacheFiles::appDataFilePath(const QString& fileName)
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataPath.isEmpty()) {
        return {};
    }

    return QDir(appDataPath).filePath(fileName);
}

void SkyCacheFiles::appendUniquePath(QStringList& paths, const QString& path)
{
    if (!path.isEmpty() && !paths.contains(path)) {
        paths.push_back(path);
    }
}

bool SkyCacheFiles::removeFiles(const QStringList& paths, const QString& logNoun)
{
    bool removedAllFiles = true;
    for (const QString& path : paths) {
        QFile file(path);
        if (file.exists() && !file.remove()) {
            qCWarning(skygateCacheFilesLog).noquote()
                << "Failed to remove" << logNoun << "file" << path << file.errorString();
            removedAllFiles = false;
        }
    }
    return removedAllFiles;
}

bool SkyCacheFiles::writeFileAtomically(const QString& path, const QByteArray& payload, const QString& logNoun)
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo targetInfo(path);
    QDir targetDir(targetInfo.absolutePath());
    if (!targetDir.mkpath(".")) {
        qCWarning(skygateCacheFilesLog).noquote()
            << "Failed to create" << logNoun << "directory" << targetDir.absolutePath();
        return false;
    }

    QSaveFile cacheFile(path);
    if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(skygateCacheFilesLog).noquote()
            << "Failed to open" << logNoun << "for writing" << path << cacheFile.errorString();
        return false;
    }

    const qint64 writtenBytes = cacheFile.write(payload);
    if (writtenBytes != payload.size()) {
        qCWarning(skygateCacheFilesLog).noquote() << "Failed to write complete" << logNoun << path << "written"
                                                  << writtenBytes << "expected" << payload.size();
        cacheFile.cancelWriting();
        return false;
    }

    if (!cacheFile.commit()) {
        qCWarning(skygateCacheFilesLog).noquote() << "Failed to commit" << logNoun << path << cacheFile.errorString();
        return false;
    }
    return true;
}

SkyCacheFiles::ReadResult SkyCacheFiles::readFile(const QString& path)
{
    ReadResult result;
    if (path.isEmpty()) {
        return result;
    }

    QFile cacheFile(path);
    if (!cacheFile.exists()) {
        return result;
    }
    if (!cacheFile.open(QIODevice::ReadOnly)) {
        result.state = ReadState::Unreadable;
        result.errorString = cacheFile.errorString();
        return result;
    }

    result.state = ReadState::Ok;
    result.payload = cacheFile.readAll();
    return result;
}

}  // namespace skygate::ui::internal
