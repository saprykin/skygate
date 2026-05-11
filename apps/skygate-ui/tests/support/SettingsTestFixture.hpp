#pragma once

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace skygate::ui::tests {

class SettingsTestFixture final {
public:
    [[nodiscard]] bool initialize(
        const QString& applicationName,
        const QString& organizationName = QStringLiteral("SkygateTests")
    )
    {
        if (!m_settingsDir.isValid()) {
            return false;
        }

        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
        QCoreApplication::setOrganizationName(organizationName);
        QCoreApplication::setApplicationName(applicationName);
        return true;
    }

    void clearSettings() const
    {
        QSettings settings;
        settings.clear();
    }

    void resetCatalogCachePaths(
        const QString& catalogFileName = QStringLiteral("catalog-cache.txt"),
        const QString& deepSkyCatalogFileName = QStringLiteral("deep-sky-catalog-cache.txt")
    ) const
    {
        setCatalogCachePaths(filePath(catalogFileName), filePath(deepSkyCatalogFileName));
    }

    void setCatalogCachePaths(
        const QString& catalogCachePath,
        const QString& deepSkyCatalogCachePath
    ) const
    {
        QSettings settings;
        settings.setValue(
            QStringLiteral("skyContext/catalogCachePath"),
            catalogCachePath
        );
        settings.setValue(
            QStringLiteral("skyContext/deepSkyCatalogCachePath"),
            deepSkyCatalogCachePath
        );
    }

    void resetSettingsWithCatalogCachePaths(
        const QString& catalogFileName = QStringLiteral("catalog-cache.txt"),
        const QString& deepSkyCatalogFileName = QStringLiteral("deep-sky-catalog-cache.txt")
    ) const
    {
        clearSettings();
        resetCatalogCachePaths(catalogFileName, deepSkyCatalogFileName);
    }

    void resetForCurrentTest() const
    {
        clearSettings();
        setCatalogCachePaths(
            currentTestFilePath(QStringLiteral("star-cache.csv")),
            currentTestFilePath(QStringLiteral("deep-sky-cache.csv"))
        );
    }

    [[nodiscard]] QString filePath(const QString& fileName) const
    {
        return m_settingsDir.filePath(fileName);
    }

    [[nodiscard]] QString path() const
    {
        return m_settingsDir.path();
    }

    [[nodiscard]] QString currentTestFilePath(const QString& fileName) const
    {
        const QString testName = QString::fromUtf8(QTest::currentTestFunction()).replace(':', '_');
        return filePath(testName + QStringLiteral("-") + fileName);
    }

private:
    QTemporaryDir m_settingsDir;
};

}  // namespace skygate::ui::tests
