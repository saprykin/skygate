#include "SkyLogging.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageLogContext>
#include <QMutex>
#include <QStandardPaths>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace skygate::ui {
namespace {

QMutex s_mutex;
SkyLoggingConfiguration s_configuration = SkyLogging::defaultConfiguration();
QtMessageHandler s_previousHandler = nullptr;
std::unique_ptr<QFile> s_logFile;
bool s_installed = false;
bool s_fileOpenFailed = false;
bool s_fileFailureReported = false;

int severityRank(const QtMsgType type) noexcept
{
    switch (type) {
    case QtDebugMsg:
        return 0;
    case QtInfoMsg:
        return 1;
    case QtWarningMsg:
        return 2;
    case QtCriticalMsg:
        return 3;
    case QtFatalMsg:
        return 4;
    }

    return 1;
}

bool shouldLog(const QtMsgType type, const QtMsgType minimumType) noexcept
{
    if (type == QtFatalMsg) {
        return true;
    }
    return severityRank(type) >= severityRank(minimumType);
}

QString severityLabel(const QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }

    return QStringLiteral("INFO");
}

QString normalizedMessage(QString message)
{
    message.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    message.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    message.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    return message;
}

QString categoryFromContext(const QMessageLogContext& context)
{
    if (context.category == nullptr || context.category[0] == '\0') {
        return QStringLiteral("default");
    }
    return QString::fromUtf8(context.category);
}

QString timestampText()
{
    const QDateTime now = QDateTime::currentDateTime();
    const int offsetSeconds = now.offsetFromUtc();
    const QChar sign = offsetSeconds < 0 ? QLatin1Char('-') : QLatin1Char('+');
    const int absoluteOffsetSeconds = std::abs(offsetSeconds);
    return QStringLiteral("%1%2%3:%4").arg(
        now.toString(QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz")),
        QString(sign),
        QStringLiteral("%1").arg(absoluteOffsetSeconds / 3600, 2, 10, QLatin1Char('0')),
        QStringLiteral("%1").arg((absoluteOffsetSeconds % 3600) / 60, 2, 10, QLatin1Char('0'))
    );
}

QString formatLine(
    const QtMsgType type,
    const QMessageLogContext& context,
    const QString& message
)
{
    return QStringLiteral("%1 %2 %3 %4\n").arg(
        timestampText(),
        severityLabel(type),
        categoryFromContext(context),
        normalizedMessage(message)
    );
}

QString backupPath(const QString& filePath, const int backupIndex)
{
    const QFileInfo info(filePath);
    const QString suffix = info.suffix();
    const QString fileName = suffix.isEmpty()
        ? QStringLiteral("%1.%2").arg(info.fileName()).arg(backupIndex)
        : QStringLiteral("%1.%2.%3").arg(info.completeBaseName()).arg(backupIndex).arg(suffix);
    return QDir(info.absolutePath()).filePath(fileName);
}

void rotateLogFileLocked(const QString& filePath, const int backupFileCount)
{
    if (backupFileCount <= 0) {
        QFile::remove(filePath);
        return;
    }

    QFile::remove(backupPath(filePath, backupFileCount));
    for (int index = backupFileCount - 1; index >= 1; --index) {
        const QString from = backupPath(filePath, index);
        const QString to = backupPath(filePath, index + 1);
        if (QFileInfo::exists(from)) {
            QFile::remove(to);
            QFile::rename(from, to);
        }
    }

    QFile::remove(backupPath(filePath, 1));
    QFile::rename(filePath, backupPath(filePath, 1));
}

bool openLogFileLocked()
{
    if (s_logFile != nullptr && s_logFile->isOpen()) {
        return true;
    }

    s_fileOpenFailed = false;
    const QString filePath = s_configuration.logFilePath.isEmpty()
        ? SkyLogging::defaultLogFilePath()
        : s_configuration.logFilePath;
    if (filePath.isEmpty()) {
        s_fileOpenFailed = true;
        return false;
    }

    const QFileInfo info(filePath);
    QDir dir(info.absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        s_fileOpenFailed = true;
        return false;
    }

    if (
        info.exists()
        && s_configuration.maxFileBytes > 0
        && info.size() >= s_configuration.maxFileBytes
    ) {
        rotateLogFileLocked(filePath, s_configuration.backupFileCount);
    }

    s_logFile = std::make_unique<QFile>(filePath);
    if (!s_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        s_logFile.reset();
        s_fileOpenFailed = true;
        return false;
    }

    return true;
}

void closeLogFileLocked()
{
    if (s_logFile == nullptr) {
        return;
    }

    if (s_logFile->isOpen()) {
        s_logFile->flush();
        s_logFile->close();
    }
    s_logFile.reset();
}

void writeTerminalLine(const QString& line)
{
    const QByteArray encoded = line.toUtf8();
    std::fwrite(encoded.constData(), 1U, static_cast<std::size_t>(encoded.size()), stderr);
    std::fflush(stderr);
}

void messageHandler(
    const QtMsgType type,
    const QMessageLogContext& context,
    const QString& message
)
{
    const QString line = formatLine(type, context, message);
    QMutexLocker locker(&s_mutex);
    const SkyLoggingConfiguration configuration = s_configuration;

    if (configuration.logToTerminal && shouldLog(type, configuration.terminalMinimumType)) {
        writeTerminalLine(line);
    }

    if (configuration.logToFile && shouldLog(type, configuration.fileMinimumType)) {
        if (openLogFileLocked()) {
            const QByteArray encoded = line.toUtf8();
            s_logFile->write(encoded);
            if (severityRank(type) >= severityRank(QtWarningMsg)) {
                s_logFile->flush();
            }

            if (
                configuration.maxFileBytes > 0
                && s_logFile->size() >= configuration.maxFileBytes
            ) {
                closeLogFileLocked();
                rotateLogFileLocked(configuration.logFilePath, configuration.backupFileCount);
                (void)openLogFileLocked();
            }
        } else if (!s_fileFailureReported && configuration.logToTerminal) {
            s_fileFailureReported = true;
            writeTerminalLine(QStringLiteral(
                "%1 WARN skygate.app Failed to open SkyGate log file\n"
            ).arg(timestampText()));
        }
    }

    if (type == QtFatalMsg && s_logFile != nullptr && s_logFile->isOpen()) {
        s_logFile->flush();
    }
}

}  // namespace

SkyLoggingConfiguration SkyLogging::defaultConfiguration()
{
    SkyLoggingConfiguration configuration;
    configuration.logFilePath = defaultLogFilePath();
    return configuration;
}

QString SkyLogging::defaultLogFilePath()
{
    const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appDataPath.isEmpty()) {
        return QDir(appDataPath).filePath(QStringLiteral("skygate.log"));
    }

    const QString applicationDirPath = QCoreApplication::applicationDirPath();
    if (!applicationDirPath.isEmpty()) {
        return QDir(applicationDirPath).filePath(QStringLiteral("skygate.log"));
    }

    return QDir::current().filePath(QStringLiteral("skygate.log"));
}

SkyLoggingConfiguration SkyLogging::configuration()
{
    QMutexLocker locker(&s_mutex);
    return s_configuration;
}

std::optional<QtMsgType> SkyLogging::messageTypeFromLevelText(const QString& levelText)
{
    const QString normalizedLevel = levelText.trimmed().toLower();
    if (normalizedLevel == QStringLiteral("debug")) {
        return QtDebugMsg;
    }
    if (normalizedLevel == QStringLiteral("info")) {
        return QtInfoMsg;
    }
    if (
        normalizedLevel == QStringLiteral("warning")
        || normalizedLevel == QStringLiteral("warn")
    ) {
        return QtWarningMsg;
    }
    if (
        normalizedLevel == QStringLiteral("error")
        || normalizedLevel == QStringLiteral("critical")
    ) {
        return QtCriticalMsg;
    }
    if (normalizedLevel == QStringLiteral("fatal")) {
        return QtFatalMsg;
    }
    return std::nullopt;
}

QString SkyLogging::levelSummary(const QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("debug");
    case QtInfoMsg:
        return QStringLiteral("info");
    case QtWarningMsg:
        return QStringLiteral("warning");
    case QtCriticalMsg:
        return QStringLiteral("error");
    case QtFatalMsg:
        return QStringLiteral("fatal");
    }

    return QStringLiteral("info");
}

QString SkyLogging::outputSummary(const SkyLoggingConfiguration& configuration)
{
    if (configuration.logToTerminal && configuration.logToFile) {
        return QStringLiteral("both");
    }
    if (configuration.logToTerminal) {
        return QStringLiteral("terminal");
    }
    if (configuration.logToFile) {
        return QStringLiteral("file");
    }
    return QStringLiteral("none");
}

QString SkyLogging::configurationSummary(const SkyLoggingConfiguration& configuration)
{
    QString summary = QStringLiteral("outputs=%1").arg(outputSummary(configuration));
    if (configuration.logToTerminal) {
        summary += QStringLiteral(" terminalLevel=%1").arg(
            levelSummary(configuration.terminalMinimumType)
        );
    }
    if (configuration.logToFile) {
        summary += QStringLiteral(" fileLevel=%1 logFile=%2").arg(
            levelSummary(configuration.fileMinimumType),
            configuration.logFilePath.trimmed().isEmpty()
                ? defaultLogFilePath()
                : configuration.logFilePath.trimmed()
        );
    }
    return summary;
}

bool SkyLogging::isInstalled()
{
    QMutexLocker locker(&s_mutex);
    return s_installed;
}

void SkyLogging::install(const SkyLoggingConfiguration& configuration)
{
    configure(configuration);
    QMutexLocker locker(&s_mutex);
    if (s_installed) {
        return;
    }

    s_previousHandler = qInstallMessageHandler(&messageHandler);
    s_installed = true;
}

void SkyLogging::configure(const SkyLoggingConfiguration& configuration)
{
    QMutexLocker locker(&s_mutex);
    SkyLoggingConfiguration normalizedConfiguration = configuration;
    if (normalizedConfiguration.logFilePath.trimmed().isEmpty()) {
        normalizedConfiguration.logFilePath = defaultLogFilePath();
    }
    normalizedConfiguration.maxFileBytes = std::max<qint64>(normalizedConfiguration.maxFileBytes, 0);
    normalizedConfiguration.backupFileCount = std::max(normalizedConfiguration.backupFileCount, 0);

    const bool filePathChanged =
        normalizedConfiguration.logFilePath != s_configuration.logFilePath;
    const bool fileOutputChanged =
        normalizedConfiguration.logToFile != s_configuration.logToFile;
    s_configuration = normalizedConfiguration;

    if (filePathChanged || fileOutputChanged || !s_configuration.logToFile) {
        closeLogFileLocked();
        s_fileOpenFailed = false;
        s_fileFailureReported = false;
    }
}

void SkyLogging::uninstall()
{
    QMutexLocker locker(&s_mutex);
    closeLogFileLocked();
    if (s_installed) {
        qInstallMessageHandler(s_previousHandler);
    }
    s_previousHandler = nullptr;
    s_configuration = defaultConfiguration();
    s_fileOpenFailed = false;
    s_fileFailureReported = false;
    s_installed = false;
}

}  // namespace skygate::ui
