#include <QtTest>

#include "SettingsTestFixture.hpp"
#include "SkyLogging.hpp"

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <thread>
#include <vector>

Q_LOGGING_CATEGORY(skygateLoggingTestLog, "skygate.logging.test")

class SkyLoggingTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void defaultConfigurationUsesTerminalOnly();
    void parsesLevelText();
    void writesFileOnlyLogs();
    void infoFileThresholdIncludesWarningsAndErrors();
    void respectsDisabledOutputs();
    void reconfiguresWithoutDuplicateHandler();
    void usesCustomAndDefaultLogFilePaths();
    void blankPathFallsBackToDefault();
    void invalidLogDirectoryFailsGracefully();
    void appendsAcrossReinitialization();
    void rotatesAndShiftsBackups();
    void formatsTimestampSeverityAndCategory();
    void escapesMultilineMessages();
    void concurrentWritesRemainLineOriented();
    void warningWritesAreFlushed();
    void summarizesOutputConfiguration();

private:
    [[nodiscard]] QString logPath(const QString& fileName = QStringLiteral("skygate.log")) const;
    [[nodiscard]] QString readText(const QString& path) const;
    [[nodiscard]] skygate::ui::SkyLoggingConfiguration fileOnlyConfig(const QString& path) const;

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyLoggingTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkyLoggingTests")));
}

void SkyLoggingTests::init()
{
    skygate::ui::SkyLogging::uninstall();
    const QStringList entries = QDir(m_settings.path()).entryList(QDir::Files);
    for (const QString& entry : entries) {
        QFile::remove(QDir(m_settings.path()).filePath(entry));
    }
}

void SkyLoggingTests::cleanup()
{
    skygate::ui::SkyLogging::uninstall();
}

QString SkyLoggingTests::logPath(const QString& fileName) const
{
    return m_settings.filePath(fileName);
}

QString SkyLoggingTests::readText(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

skygate::ui::SkyLoggingConfiguration SkyLoggingTests::fileOnlyConfig(const QString& path) const
{
    skygate::ui::SkyLoggingConfiguration configuration =
        skygate::ui::SkyLogging::defaultConfiguration();
    configuration.logToTerminal = false;
    configuration.logToFile = true;
    configuration.logFilePath = path;
    configuration.fileMinimumType = QtDebugMsg;
    configuration.maxFileBytes = 1024 * 1024;
    return configuration;
}

void SkyLoggingTests::defaultConfigurationUsesTerminalOnly()
{
    const skygate::ui::SkyLoggingConfiguration configuration =
        skygate::ui::SkyLogging::defaultConfiguration();
    QVERIFY(configuration.logToTerminal);
    QVERIFY(!configuration.logToFile);
    QVERIFY(!configuration.logFilePath.isEmpty());
    QCOMPARE(configuration.terminalMinimumType, QtInfoMsg);
    QCOMPARE(configuration.fileMinimumType, QtInfoMsg);
}

void SkyLoggingTests::parsesLevelText()
{
    QCOMPARE(*skygate::ui::SkyLogging::messageTypeFromLevelText(QStringLiteral("debug")), QtDebugMsg);
    QCOMPARE(*skygate::ui::SkyLogging::messageTypeFromLevelText(QStringLiteral("INFO")), QtInfoMsg);
    QCOMPARE(*skygate::ui::SkyLogging::messageTypeFromLevelText(QStringLiteral("warn")), QtWarningMsg);
    QCOMPARE(
        *skygate::ui::SkyLogging::messageTypeFromLevelText(QStringLiteral("warning")),
        QtWarningMsg
    );
    QCOMPARE(*skygate::ui::SkyLogging::messageTypeFromLevelText(QStringLiteral("error")), QtCriticalMsg);
    QCOMPARE(
        *skygate::ui::SkyLogging::messageTypeFromLevelText(QStringLiteral("critical")),
        QtCriticalMsg
    );
    QCOMPARE(*skygate::ui::SkyLogging::messageTypeFromLevelText(QStringLiteral("fatal")), QtFatalMsg);
    QVERIFY(!skygate::ui::SkyLogging::messageTypeFromLevelText(QStringLiteral("verbose")).has_value());
}

void SkyLoggingTests::writesFileOnlyLogs()
{
    const QString path = logPath();
    skygate::ui::SkyLogging::install(fileOnlyConfig(path));

    qCInfo(skygateLoggingTestLog) << "file-only-info";
    qCWarning(skygateLoggingTestLog) << "file-only-warning";

    const QString text = readText(path);
    QVERIFY(text.contains(QStringLiteral("INFO skygate.logging.test file-only-info")));
    QVERIFY(text.contains(QStringLiteral("WARN skygate.logging.test file-only-warning")));
}

void SkyLoggingTests::infoFileThresholdIncludesWarningsAndErrors()
{
    const QString path = logPath();
    skygate::ui::SkyLoggingConfiguration configuration = fileOnlyConfig(path);
    configuration.fileMinimumType = QtInfoMsg;
    skygate::ui::SkyLogging::install(configuration);

    qDebug() << "debug-below-info";
    qCInfo(skygateLoggingTestLog) << "info-at-threshold";
    qCWarning(skygateLoggingTestLog) << "warning-above-info";
    qCritical() << "critical-above-info";

    const QString text = readText(path);
    QVERIFY(!text.contains(QStringLiteral("debug-below-info")));
    QVERIFY(text.contains(QStringLiteral("INFO skygate.logging.test info-at-threshold")));
    QVERIFY(text.contains(QStringLiteral("WARN skygate.logging.test warning-above-info")));
    QVERIFY(text.contains(QStringLiteral("ERROR default critical-above-info")));
}

void SkyLoggingTests::respectsDisabledOutputs()
{
    skygate::ui::SkyLoggingConfiguration configuration = fileOnlyConfig(logPath());
    configuration.logToFile = false;
    configuration.logToTerminal = false;
    skygate::ui::SkyLogging::install(configuration);

    qCWarning(skygateLoggingTestLog) << "disabled-output-warning";

    QVERIFY(!QFileInfo::exists(logPath()));
}

void SkyLoggingTests::reconfiguresWithoutDuplicateHandler()
{
    const QString path = logPath();
    skygate::ui::SkyLogging::install(fileOnlyConfig(path));
    skygate::ui::SkyLogging::install(fileOnlyConfig(path));
    skygate::ui::SkyLogging::configure(fileOnlyConfig(path));

    qCWarning(skygateLoggingTestLog) << "single-warning";

    const QString text = readText(path);
    QCOMPARE(text.count(QStringLiteral("single-warning")), 1);
}

void SkyLoggingTests::usesCustomAndDefaultLogFilePaths()
{
    const QString path = logPath(QStringLiteral("custom.log"));
    skygate::ui::SkyLogging::install(fileOnlyConfig(path));
    qCWarning(skygateLoggingTestLog) << "custom-path-warning";
    QVERIFY(readText(path).contains(QStringLiteral("custom-path-warning")));

    const QString defaultPath = skygate::ui::SkyLogging::defaultLogFilePath();
    QVERIFY(!defaultPath.isEmpty());
    QVERIFY(defaultPath.endsWith(QStringLiteral("skygate.log")));
}

void SkyLoggingTests::blankPathFallsBackToDefault()
{
    skygate::ui::SkyLoggingConfiguration configuration = fileOnlyConfig(QString());
    skygate::ui::SkyLogging::configure(configuration);

    const skygate::ui::SkyLoggingConfiguration normalizedConfiguration =
        skygate::ui::SkyLogging::configuration();
    QVERIFY(!normalizedConfiguration.logFilePath.isEmpty());
    QCOMPARE(normalizedConfiguration.logFilePath, skygate::ui::SkyLogging::defaultLogFilePath());
}

void SkyLoggingTests::invalidLogDirectoryFailsGracefully()
{
    const QString parentFilePath = logPath(QStringLiteral("not-a-directory"));
    QFile parentFile(parentFilePath);
    QVERIFY(parentFile.open(QIODevice::WriteOnly));
    parentFile.write("blocking file");
    parentFile.close();

    const QString impossiblePath = QDir(parentFilePath).filePath(QStringLiteral("skygate.log"));
    skygate::ui::SkyLogging::install(fileOnlyConfig(impossiblePath));
    qCWarning(skygateLoggingTestLog) << "unwritable-warning";

    QVERIFY(!QFileInfo::exists(impossiblePath));
}

void SkyLoggingTests::appendsAcrossReinitialization()
{
    const QString path = logPath();
    skygate::ui::SkyLogging::install(fileOnlyConfig(path));
    qCWarning(skygateLoggingTestLog) << "first-run";
    skygate::ui::SkyLogging::uninstall();

    skygate::ui::SkyLogging::install(fileOnlyConfig(path));
    qCWarning(skygateLoggingTestLog) << "second-run";

    const QString text = readText(path);
    QVERIFY(text.contains(QStringLiteral("first-run")));
    QVERIFY(text.contains(QStringLiteral("second-run")));
}

void SkyLoggingTests::rotatesAndShiftsBackups()
{
    const QString path = logPath();
    QFile::remove(path);
    QFile::remove(logPath(QStringLiteral("skygate.1.log")));
    QFile::remove(logPath(QStringLiteral("skygate.2.log")));
    QFile::remove(logPath(QStringLiteral("skygate.3.log")));
    {
        QFile current(path);
        QVERIFY(current.open(QIODevice::WriteOnly));
        current.write(QByteArray(200, 'x'));
    }
    {
        QFile backup1(logPath(QStringLiteral("skygate.1.log")));
        QVERIFY(backup1.open(QIODevice::WriteOnly));
        backup1.write("old-one");
    }
    {
        QFile backup2(logPath(QStringLiteral("skygate.2.log")));
        QVERIFY(backup2.open(QIODevice::WriteOnly));
        backup2.write("old-two");
    }
    {
        QFile backup3(logPath(QStringLiteral("skygate.3.log")));
        QVERIFY(backup3.open(QIODevice::WriteOnly));
        backup3.write("old-three");
    }

    skygate::ui::SkyLoggingConfiguration configuration = fileOnlyConfig(path);
    configuration.maxFileBytes = 128;
    configuration.backupFileCount = 3;
    skygate::ui::SkyLogging::install(configuration);
    qCWarning(skygateLoggingTestLog) << "after-rotation";

    QVERIFY(readText(path).contains(QStringLiteral("after-rotation")));
    QCOMPARE(
        readText(logPath(QStringLiteral("skygate.1.log"))),
        QString::fromLatin1(QByteArray(200, 'x'))
    );
    QCOMPARE(readText(logPath(QStringLiteral("skygate.2.log"))), QStringLiteral("old-one"));
    QCOMPARE(readText(logPath(QStringLiteral("skygate.3.log"))), QStringLiteral("old-two"));
}

void SkyLoggingTests::formatsTimestampSeverityAndCategory()
{
    const QString path = logPath();
    skygate::ui::SkyLogging::install(fileOnlyConfig(path));
    qDebug() << "debug-line";
    qInfo() << "info-line";
    qCWarning(skygateLoggingTestLog) << "warning-line";
    qCritical() << "critical-line";

    const QString text = readText(path);
    const QRegularExpression linePattern(QStringLiteral(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}[-+]\d{2}:\d{2} (DEBUG|INFO|WARN|ERROR) [^ ]+ .+$)"
    ));
    for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        QVERIFY2(linePattern.match(line).hasMatch(), qPrintable(line));
    }
    QVERIFY(text.contains(QStringLiteral("DEBUG default debug-line")));
    QVERIFY(text.contains(QStringLiteral("INFO default info-line")));
    QVERIFY(text.contains(QStringLiteral("WARN skygate.logging.test warning-line")));
    QVERIFY(text.contains(QStringLiteral("ERROR default critical-line")));
}

void SkyLoggingTests::escapesMultilineMessages()
{
    const QString path = logPath();
    skygate::ui::SkyLogging::install(fileOnlyConfig(path));
    qCWarning(skygateLoggingTestLog).noquote() << "first line\nsecond line";

    const QString text = readText(path);
    QVERIFY(text.contains(QStringLiteral("first line\\nsecond line")));
    QCOMPARE(text.split(QLatin1Char('\n'), Qt::SkipEmptyParts).size(), 1);
}

void SkyLoggingTests::concurrentWritesRemainLineOriented()
{
    const QString path = logPath();
    skygate::ui::SkyLogging::install(fileOnlyConfig(path));

    std::vector<std::thread> threads;
    for (int threadIndex = 0; threadIndex < 4; ++threadIndex) {
        threads.emplace_back([threadIndex] {
            for (int messageIndex = 0; messageIndex < 25; ++messageIndex) {
                qCWarning(skygateLoggingTestLog)
                    << "thread" << threadIndex << "message" << messageIndex;
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    const QStringList lines = readText(path).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), 100);
    for (const QString& line : lines) {
        QVERIFY(line.contains(QStringLiteral("WARN skygate.logging.test thread")));
    }
}

void SkyLoggingTests::warningWritesAreFlushed()
{
    const QString path = logPath();
    skygate::ui::SkyLogging::install(fileOnlyConfig(path));
    qCWarning(skygateLoggingTestLog) << "flush-now";

    QVERIFY(readText(path).contains(QStringLiteral("flush-now")));
}

void SkyLoggingTests::summarizesOutputConfiguration()
{
    skygate::ui::SkyLoggingConfiguration configuration =
        skygate::ui::SkyLogging::defaultConfiguration();
    configuration.logToTerminal = true;
    configuration.logToFile = false;
    QCOMPARE(skygate::ui::SkyLogging::outputSummary(configuration), QStringLiteral("terminal"));
    QVERIFY(skygate::ui::SkyLogging::configurationSummary(configuration).contains(
        QStringLiteral("terminalLevel=info")
    ));

    configuration.logToFile = true;
    configuration.logFilePath = logPath();
    configuration.fileMinimumType = QtWarningMsg;
    QCOMPARE(skygate::ui::SkyLogging::outputSummary(configuration), QStringLiteral("both"));
    QVERIFY(skygate::ui::SkyLogging::configurationSummary(configuration).contains(
        QStringLiteral("outputs=both")
    ));
    QVERIFY(skygate::ui::SkyLogging::configurationSummary(configuration).contains(
        QStringLiteral("fileLevel=warning")
    ));
    QVERIFY(skygate::ui::SkyLogging::configurationSummary(configuration).contains(logPath()));

    configuration.logToTerminal = false;
    QCOMPARE(skygate::ui::SkyLogging::outputSummary(configuration), QStringLiteral("file"));

    configuration.logToFile = false;
    QCOMPARE(skygate::ui::SkyLogging::outputSummary(configuration), QStringLiteral("none"));
}

QTEST_GUILESS_MAIN(SkyLoggingTests)

#include "SkyLoggingTests.moc"
