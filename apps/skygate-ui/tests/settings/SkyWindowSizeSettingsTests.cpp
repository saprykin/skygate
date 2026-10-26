#include "settings/SkyWindowSizeSettings.hpp"
#include "support/SettingsTestFixture.hpp"

#include <QRegularExpression>
#include <QSettings>
#include <QtTest/QtTest>

using skygate::ui::internal::SkyWindowSizeSettings;

class SkyWindowSizeSettingsTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultsToPinnedWindowSize();
    void savesAndLoadsStoredWindowSize();
    void replacesSizeBelowMinimumWithDefault();
    void malformedStoredDimensionFallsBackToDefault();

private:
    skygate::ui::tests::SettingsTestFixture m_settings;
};

void SkyWindowSizeSettingsTests::initTestCase()
{
    QVERIFY(m_settings.initialize(QStringLiteral("SkygateUiWindowSizeSettingsTests")));
}

void SkyWindowSizeSettingsTests::defaultsToPinnedWindowSize()
{
    QSettings settings;
    settings.clear();

    QCOMPARE(SkyWindowSizeSettings::load(), QSize(1100, 760));
}

void SkyWindowSizeSettingsTests::savesAndLoadsStoredWindowSize()
{
    QSettings settings;
    settings.clear();

    QVERIFY(SkyWindowSizeSettings::save(QSize(1280, 820)));
    QCOMPARE(settings.value("app/mainWindowWidth").toInt(), 1280);
    QCOMPARE(settings.value("app/mainWindowHeight").toInt(), 820);
    QCOMPARE(SkyWindowSizeSettings::load(), QSize(1280, 820));
}

void SkyWindowSizeSettingsTests::replacesSizeBelowMinimumWithDefault()
{
    QSettings settings;
    settings.clear();

    QVERIFY(SkyWindowSizeSettings::save(QSize(300, 300)));
    QCOMPARE(settings.value("app/mainWindowWidth").toInt(), 1100);
    QCOMPARE(settings.value("app/mainWindowHeight").toInt(), 760);
    QCOMPARE(SkyWindowSizeSettings::load(), QSize(1100, 760));

    // A size exactly at the minimum stays usable, while one dimension below
    // its minimum falls back alone.
    QVERIFY(SkyWindowSizeSettings::save(QSize(560, 420)));
    QCOMPARE(SkyWindowSizeSettings::load(), QSize(560, 420));
    QVERIFY(SkyWindowSizeSettings::save(QSize(560, 300)));
    QCOMPARE(SkyWindowSizeSettings::load(), QSize(560, 760));
}

void SkyWindowSizeSettingsTests::malformedStoredDimensionFallsBackToDefault()
{
    QSettings settings;
    settings.clear();
    settings.setValue("app/mainWindowWidth", "wide");
    settings.setValue("app/mainWindowHeight", 12);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Invalid integer setting app/mainWindowWidth .*"));
    QCOMPARE(SkyWindowSizeSettings::load(), QSize(1100, 760));
}

QTEST_GUILESS_MAIN(SkyWindowSizeSettingsTests)

#include "SkyWindowSizeSettingsTests.moc"
