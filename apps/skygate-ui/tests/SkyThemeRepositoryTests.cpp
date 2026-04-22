#include <QtTest>

#include "SkyTheme.hpp"

#include <QFile>
#include <QTemporaryDir>

using namespace skygate::ui::internal;

class SkyThemeRepositoryTests final : public QObject {
    Q_OBJECT

private slots:
    void loadsBundledThemes();
    void missingThemeIdFallsBackToDefaultTheme();
    void malformedThemeDefinitionFallsBackToBuiltInDefault();
};

void SkyThemeRepositoryTests::loadsBundledThemes()
{
    const SkyThemeRepository repository;
    const SkyThemeDefinition& defaultTheme = repository.themeById("default");
    const SkyThemeDefinition& nightTheme = repository.themeById("night-vision");

    QCOMPARE(defaultTheme.id, QString("default"));
    QCOMPARE(defaultTheme.displayName, QString("Default"));
    QCOMPARE(defaultTheme.ui.windowBackground, QColor("#171b30"));
    QCOMPARE(defaultTheme.render.skyGradientTop, QColor("#081022"));

    QCOMPARE(nightTheme.id, QString("night-vision"));
    QCOMPARE(nightTheme.displayName, QString("Night Vision"));
    QCOMPARE(nightTheme.ui.windowBackground, QColor("#150707"));
    QCOMPARE(nightTheme.render.skyGradientTop, QColor("#140404"));
}

void SkyThemeRepositoryTests::missingThemeIdFallsBackToDefaultTheme()
{
    const SkyThemeRepository repository;
    const SkyThemeDefinition& defaultTheme = repository.defaultTheme();
    const SkyThemeDefinition& resolvedTheme = repository.themeById("missing-theme");

    QCOMPARE(resolvedTheme.id, defaultTheme.id);
    QCOMPARE(resolvedTheme.ui.windowBackground, defaultTheme.ui.windowBackground);
}

void SkyThemeRepositoryTests::malformedThemeDefinitionFallsBackToBuiltInDefault()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString invalidThemePath = tempDir.filePath("broken-theme.json");
    QFile invalidThemeFile(invalidThemePath);
    QVERIFY(invalidThemeFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalidThemeFile.write(R"({
        "id": "broken-theme",
        "displayName": "Broken Theme",
        "ui": {
            "windowBackground": "not-a-color"
        },
        "render": {
            "skyGradientTop": "#000000"
        }
    })");
    invalidThemeFile.close();

    const QString manifestPath = tempDir.filePath("manifest.json");
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    manifestFile.write(QString(R"({
        "defaultThemeId": "broken-theme",
        "themes": [
            {
                "id": "broken-theme",
                "resourcePath": "%1"
            }
        ]
    })")
                           .arg(invalidThemePath)
                           .toUtf8());
    manifestFile.close();

    const SkyThemeRepository repository(manifestPath);
    const SkyThemeDefinition& fallbackTheme = repository.defaultTheme();

    QCOMPARE(fallbackTheme.id, QString("default"));
    QCOMPARE(fallbackTheme.ui.windowBackground, QColor("#171b30"));
}

QTEST_GUILESS_MAIN(SkyThemeRepositoryTests)

#include "SkyThemeRepositoryTests.moc"
