#include "theme/SkyThemeDefinition.hpp"
#include "theme/SkyThemeJsonCodec.hpp"
#include "theme/SkyThemeRenderPalette.hpp"
#include "theme/SkyThemeUiPalette.hpp"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QtTest>

#include <utility>

using namespace skygate::ui::internal;

namespace {

using NamedColor = std::pair<QString, QColor>;

QList<NamedColor> uiPaletteEntries(const SkyThemeUiPalette& palette)
{
    QList<NamedColor> entries;
#define SKYGATE_COLLECT_UI_COLOR(colorName) entries.append(NamedColor{QStringLiteral(#colorName), palette.colorName});
    SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_COLLECT_UI_COLOR)
#undef SKYGATE_COLLECT_UI_COLOR
    return entries;
}

QList<NamedColor> renderPaletteEntries(const SkyThemeRenderPalette& palette)
{
    QList<NamedColor> entries;
#define SKYGATE_COLLECT_RENDER_COLOR(colorName)                                                                        \
    entries.append(NamedColor{QStringLiteral(#colorName), palette.colorName});
    SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_COLLECT_RENDER_COLOR)
#undef SKYGATE_COLLECT_RENDER_COLOR
    return entries;
}

QJsonObject uiPaletteObject(const SkyThemeUiPalette& palette)
{
    QJsonObject object;
#define SKYGATE_STORE_UI_COLOR(colorName)                                                                              \
    object.insert(QStringLiteral(#colorName), palette.colorName.name(QColor::HexArgb));
    SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_STORE_UI_COLOR)
#undef SKYGATE_STORE_UI_COLOR
    return object;
}

QJsonObject renderPaletteObject(const SkyThemeRenderPalette& palette)
{
    QJsonObject object;
#define SKYGATE_STORE_RENDER_COLOR(colorName)                                                                          \
    object.insert(QStringLiteral(#colorName), palette.colorName.name(QColor::HexArgb));
    SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_STORE_RENDER_COLOR)
#undef SKYGATE_STORE_RENDER_COLOR
    return object;
}

QJsonObject themeJsonObject(const SkyThemeDefinition& theme)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), theme.id);
    object.insert(QStringLiteral("displayName"), theme.displayName);
    object.insert(QStringLiteral("ui"), uiPaletteObject(theme.ui));
    object.insert(QStringLiteral("render"), renderPaletteObject(theme.render));
    return object;
}

QByteArray toJsonBytes(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// Produces a unique color per index so a palette entry that is written under
// the wrong JSON key or read into the wrong member cannot compare equal.
QColor distinctColor(const qsizetype index)
{
    const auto value = static_cast<int>(index);
    return QColor((value * 37 + 11) % 256, (value * 61 + 23) % 256, (value * 97 + 41) % 256, 32 + (value * 29) % 224);
}

SkyThemeDefinition distinctPaletteTheme()
{
    SkyThemeDefinition theme;
    theme.id = "test-theme";
    theme.displayName = "Test Theme";

    qsizetype index = 0;
#define SKYGATE_ASSIGN_UI_COLOR(colorName) theme.ui.colorName = distinctColor(index++);
    SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_ASSIGN_UI_COLOR)
#undef SKYGATE_ASSIGN_UI_COLOR
#define SKYGATE_ASSIGN_RENDER_COLOR(colorName) theme.render.colorName = distinctColor(index++);
    SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_ASSIGN_RENDER_COLOR)
#undef SKYGATE_ASSIGN_RENDER_COLOR

    return theme;
}

void verifyUiPalette(const SkyThemeUiPalette& expected, const SkyThemeUiPalette& actual)
{
    const QList<NamedColor> expectedEntries = uiPaletteEntries(expected);
    const QList<NamedColor> actualEntries = uiPaletteEntries(actual);
    QCOMPARE(actualEntries.size(), expectedEntries.size());
    for (qsizetype index = 0; index < expectedEntries.size(); ++index) {
        QCOMPARE(actualEntries.at(index).first, expectedEntries.at(index).first);
        const QColor& expectedColor = expectedEntries.at(index).second;
        const QColor& actualColor = actualEntries.at(index).second;
        QVERIFY2(
            actualColor == expectedColor,
            qPrintable(QStringLiteral("round trip changed ui palette entry %1 (expected %2, got %3)")
                           .arg(
                               expectedEntries.at(index).first,
                               expectedColor.name(QColor::HexArgb),
                               actualColor.name(QColor::HexArgb)
                           ))
        );
    }
}

void verifyRenderPalette(const SkyThemeRenderPalette& expected, const SkyThemeRenderPalette& actual)
{
    const QList<NamedColor> expectedEntries = renderPaletteEntries(expected);
    const QList<NamedColor> actualEntries = renderPaletteEntries(actual);
    QCOMPARE(actualEntries.size(), expectedEntries.size());
    for (qsizetype index = 0; index < expectedEntries.size(); ++index) {
        QCOMPARE(actualEntries.at(index).first, expectedEntries.at(index).first);
        const QColor& expectedColor = expectedEntries.at(index).second;
        const QColor& actualColor = actualEntries.at(index).second;
        QVERIFY2(
            actualColor == expectedColor,
            qPrintable(QStringLiteral("round trip changed render palette entry %1 (expected %2, got %3)")
                           .arg(
                               expectedEntries.at(index).first,
                               expectedColor.name(QColor::HexArgb),
                               actualColor.name(QColor::HexArgb)
                           ))
        );
    }
}

QJsonObject uiSectionOf(const QJsonObject& themeObject)
{
    return themeObject.value(QStringLiteral("ui")).toObject();
}

QJsonObject renderSectionOf(const QJsonObject& themeObject)
{
    return themeObject.value(QStringLiteral("render")).toObject();
}

}  // namespace

class SkyThemeJsonCodecTests final : public QObject {
    Q_OBJECT

private slots:
    void parseObjectReturnsDocumentObject();
    void parseObjectRejectsMalformedJson();
    void parseObjectRejectsNonObjectDocuments();
    void requiredStringTrimsText();
    void requiredStringRejectsMissingAndBlankValues();
    void requiredStringRejectsNonStringValues();
    void parseThemeRoundTripsBuiltInDefault();
    void parseThemeRoundTripsEveryPaletteEntry();
    void parseThemeTrimsIdentityFields();
    void parseThemeIgnoresUnknownSectionsAndKeys();
    void parseThemeRejectsMissingIdentityFields();
    void parseThemeRejectsMissingSections();
    void parseThemeRejectsMissingPaletteEntries();
    void parseThemeRejectsInvalidPaletteEntries();
    void parseThemeRejectsMalformedDocuments();
};

void SkyThemeJsonCodecTests::parseObjectReturnsDocumentObject()
{
    const QByteArray manifestBytes = R"({
        "defaultThemeId": "default",
        "themes": [
            {"id": "default", "resourcePath": ":/themes/default.json"},
            {"id": "night-vision", "resourcePath": ":/themes/night-vision.json"}
        ]
    })";

    const std::optional<QJsonObject> manifest =
        SkyThemeJsonCodec::parseObject(manifestBytes, QStringLiteral("theme manifest /themes/manifest.json"));

    QVERIFY(manifest.has_value());
    QCOMPARE(manifest->value(QStringLiteral("defaultThemeId")).toString(), QString("default"));
    QCOMPARE(manifest->value(QStringLiteral("themes")).toArray().size(), 2);

    QVERIFY(SkyThemeJsonCodec::parseObject("{}", QStringLiteral("theme /themes/empty.json")).has_value());
}

void SkyThemeJsonCodecTests::parseObjectRejectsMalformedJson()
{
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Failed to parse theme /themes/truncated\\.json"));
    QVERIFY(
        !SkyThemeJsonCodec::parseObject(QByteArrayLiteral("{\"id\": "), QStringLiteral("theme /themes/truncated.json"))
             .has_value()
    );

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Failed to parse theme /themes/empty\\.json"));
    QVERIFY(!SkyThemeJsonCodec::parseObject(QByteArray(), QStringLiteral("theme /themes/empty.json")).has_value());

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Failed to parse theme /themes/trailing\\.json"));
    QVERIFY(!SkyThemeJsonCodec::parseObject(
                 QByteArrayLiteral("{\"id\": \"default\"} trailing"), QStringLiteral("theme /themes/trailing.json")
    )
                 .has_value());
}

void SkyThemeJsonCodecTests::parseObjectRejectsNonObjectDocuments()
{
    const QList<QByteArray> documents{
        QByteArrayLiteral("[]"),
        QByteArrayLiteral(R"([{"id": "default"}])"),
        QByteArrayLiteral(R"("default")"),
        QByteArrayLiteral("42"),
        QByteArrayLiteral("true"),
        QByteArrayLiteral("null"),
    };

    for (const QByteArray& document : documents) {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Failed to parse theme /themes/scalar\\.json"));
        QVERIFY(!SkyThemeJsonCodec::parseObject(document, QStringLiteral("theme /themes/scalar.json")).has_value());
    }
}

void SkyThemeJsonCodecTests::requiredStringTrimsText()
{
    QJsonObject descriptor;
    descriptor.insert(QStringLiteral("id"), QStringLiteral("  night-vision \t"));

    const std::optional<QString> id =
        SkyThemeJsonCodec::requiredString(descriptor, "id", QStringLiteral("theme descriptor"));

    QVERIFY(id.has_value());
    QCOMPARE(id.value(), QString("night-vision"));
}

void SkyThemeJsonCodecTests::requiredStringRejectsMissingAndBlankValues()
{
    const QString description = QStringLiteral("theme descriptor");

    QJsonObject descriptor;
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("theme descriptor is missing a value for key id"));
    QVERIFY(!SkyThemeJsonCodec::requiredString(descriptor, "id", description).has_value());

    descriptor.insert(QStringLiteral("id"), QString());
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("theme descriptor is missing a value for key id"));
    QVERIFY(!SkyThemeJsonCodec::requiredString(descriptor, "id", description).has_value());

    descriptor.insert(QStringLiteral("id"), QStringLiteral(" \t\n "));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("theme descriptor is missing a value for key id"));
    QVERIFY(!SkyThemeJsonCodec::requiredString(descriptor, "id", description).has_value());
}

void SkyThemeJsonCodecTests::requiredStringRejectsNonStringValues()
{
    const QString description = QStringLiteral("theme descriptor");
    const QList<QJsonValue> nonStringValues{
        QJsonValue(QJsonValue::Null),
        QJsonValue(42),
        QJsonValue(3.5),
        QJsonValue(true),
        QJsonValue(QJsonArray{}),
        QJsonValue(QJsonObject{}),
    };

    for (const QJsonValue& value : nonStringValues) {
        QJsonObject descriptor;
        descriptor.insert(QStringLiteral("id"), value);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("theme descriptor is missing a value for key id"));
        QVERIFY(!SkyThemeJsonCodec::requiredString(descriptor, "id", description).has_value());
    }
}

void SkyThemeJsonCodecTests::parseThemeRoundTripsBuiltInDefault()
{
    const SkyThemeDefinition original = SkyThemeDefinition::builtInDefault();
    const std::optional<SkyThemeDefinition> parsed =
        SkyThemeJsonCodec::parseTheme(toJsonBytes(themeJsonObject(original)), QStringLiteral("/themes/default.json"));

    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->id, original.id);
    QCOMPARE(parsed->displayName, original.displayName);
    verifyUiPalette(original.ui, parsed->ui);
    verifyRenderPalette(original.render, parsed->render);

    QCOMPARE(parsed->ui.headerGradientEnd.alpha(), 0);
    QCOMPARE(parsed->ui.progressBarSweepEnd, QColor("#4a82df22"));
    QCOMPARE(parsed->render.selectionMarkerFill, QColor(255, 77, 77, 24));
}

void SkyThemeJsonCodecTests::parseThemeRoundTripsEveryPaletteEntry()
{
    const SkyThemeDefinition original = distinctPaletteTheme();
    const std::optional<SkyThemeDefinition> parsed = SkyThemeJsonCodec::parseTheme(
        toJsonBytes(themeJsonObject(original)), QStringLiteral("/themes/test-theme.json")
    );

    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->id, QString("test-theme"));
    QCOMPARE(parsed->displayName, QString("Test Theme"));
    verifyUiPalette(original.ui, parsed->ui);
    verifyRenderPalette(original.render, parsed->render);
}

void SkyThemeJsonCodecTests::parseThemeTrimsIdentityFields()
{
    SkyThemeDefinition original = SkyThemeDefinition::builtInDefault();
    original.id = QStringLiteral("  night-vision \n");
    original.displayName = QStringLiteral("\tNight Vision  ");

    const std::optional<SkyThemeDefinition> parsed = SkyThemeJsonCodec::parseTheme(
        toJsonBytes(themeJsonObject(original)), QStringLiteral("/themes/night-vision.json")
    );

    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->id, QString("night-vision"));
    QCOMPARE(parsed->displayName, QString("Night Vision"));
}

void SkyThemeJsonCodecTests::parseThemeIgnoresUnknownSectionsAndKeys()
{
    const SkyThemeDefinition original = SkyThemeDefinition::builtInDefault();
    QJsonObject document = themeJsonObject(original);

    // Theme documents may carry sections owned by other consumers; the codec
    // only reads the identity fields and the two palettes.
    QJsonObject options;
    options.insert(QStringLiteral("layers"), QJsonArray{QStringLiteral("horizon")});
    document.insert(QStringLiteral("options"), options);
    QJsonObject visibility;
    visibility.insert(QStringLiteral("horizon"), false);
    visibility.insert(QStringLiteral("ecliptic"), true);
    document.insert(QStringLiteral("visibility"), visibility);
    document.insert(QStringLiteral("version"), 2);

    QJsonObject uiSection = uiSectionOf(document);
    uiSection.insert(QStringLiteral("windowBackgroundHover"), QStringLiteral("#123456"));
    uiSection.insert(QStringLiteral("futureUiColor"), QStringLiteral("#abcdef"));
    document.insert(QStringLiteral("ui"), uiSection);

    QJsonObject renderSection = renderSectionOf(document);
    renderSection.insert(QStringLiteral("futureRenderColor"), QStringLiteral("#fedcba"));
    document.insert(QStringLiteral("render"), renderSection);

    const std::optional<SkyThemeDefinition> parsed =
        SkyThemeJsonCodec::parseTheme(toJsonBytes(document), QStringLiteral("/themes/extended.json"));

    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->id, original.id);
    QCOMPARE(parsed->displayName, original.displayName);
    verifyUiPalette(original.ui, parsed->ui);
    verifyRenderPalette(original.render, parsed->render);
}

void SkyThemeJsonCodecTests::parseThemeRejectsMissingIdentityFields()
{
    const SkyThemeDefinition base = SkyThemeDefinition::builtInDefault();

    QJsonObject missingId = themeJsonObject(base);
    missingId.remove(QStringLiteral("id"));
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("theme /themes/missing-id\\.json is missing a value for key id")
    );
    QVERIFY(
        !SkyThemeJsonCodec::parseTheme(toJsonBytes(missingId), QStringLiteral("/themes/missing-id.json")).has_value()
    );

    QJsonObject missingDisplayName = themeJsonObject(base);
    missingDisplayName.remove(QStringLiteral("displayName"));
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("theme /themes/missing-name\\.json is missing a value for key displayName")
    );
    QVERIFY(!SkyThemeJsonCodec::parseTheme(toJsonBytes(missingDisplayName), QStringLiteral("/themes/missing-name.json"))
                 .has_value());

    QJsonObject blankId = themeJsonObject(base);
    blankId.insert(QStringLiteral("id"), QStringLiteral("   "));
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("theme /themes/blank-id\\.json is missing a value for key id")
    );
    QVERIFY(!SkyThemeJsonCodec::parseTheme(toJsonBytes(blankId), QStringLiteral("/themes/blank-id.json")).has_value());

    QJsonObject nonStringDisplayName = themeJsonObject(base);
    nonStringDisplayName.insert(QStringLiteral("displayName"), 7);
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("theme /themes/numeric-name\\.json is missing a value for key displayName")
    );
    QVERIFY(
        !SkyThemeJsonCodec::parseTheme(toJsonBytes(nonStringDisplayName), QStringLiteral("/themes/numeric-name.json"))
             .has_value()
    );
}

void SkyThemeJsonCodecTests::parseThemeRejectsMissingSections()
{
    QJsonObject noSections;
    noSections.insert(QStringLiteral("id"), QStringLiteral("default"));
    noSections.insert(QStringLiteral("displayName"), QStringLiteral("Default"));
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Theme /themes/no-sections\\.json is missing ui/render sections")
    );
    QVERIFY(
        !SkyThemeJsonCodec::parseTheme(toJsonBytes(noSections), QStringLiteral("/themes/no-sections.json")).has_value()
    );

    const SkyThemeDefinition base = SkyThemeDefinition::builtInDefault();

    QJsonObject uiNotObject = themeJsonObject(base);
    uiNotObject.insert(QStringLiteral("ui"), QJsonArray{});
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Theme /themes/ui-array\\.json is missing ui/render sections")
    );
    QVERIFY(
        !SkyThemeJsonCodec::parseTheme(toJsonBytes(uiNotObject), QStringLiteral("/themes/ui-array.json")).has_value()
    );

    QJsonObject renderNotObject = themeJsonObject(base);
    renderNotObject.insert(QStringLiteral("render"), QStringLiteral("colors"));
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Theme /themes/render-string\\.json is missing ui/render sections")
    );
    QVERIFY(!SkyThemeJsonCodec::parseTheme(toJsonBytes(renderNotObject), QStringLiteral("/themes/render-string.json"))
                 .has_value());
}

void SkyThemeJsonCodecTests::parseThemeRejectsMissingPaletteEntries()
{
    const SkyThemeDefinition base = SkyThemeDefinition::builtInDefault();

    QJsonObject missingUiColor = themeJsonObject(base);
    QJsonObject uiSection = uiSectionOf(missingUiColor);
    uiSection.remove(QStringLiteral("windowBackground"));
    missingUiColor.insert(QStringLiteral("ui"), uiSection);
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Theme /themes/missing-ui-color\\.json is missing color key windowBackground")
    );
    QVERIFY(!SkyThemeJsonCodec::parseTheme(toJsonBytes(missingUiColor), QStringLiteral("/themes/missing-ui-color.json"))
                 .has_value());

    QJsonObject missingRenderColor = themeJsonObject(base);
    QJsonObject renderSection = renderSectionOf(missingRenderColor);
    renderSection.remove(QStringLiteral("skyGradientTop"));
    missingRenderColor.insert(QStringLiteral("render"), renderSection);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Theme /themes/missing-render-color\\.json is missing color key skyGradientTop")
    );
    QVERIFY(!SkyThemeJsonCodec::parseTheme(
                 toJsonBytes(missingRenderColor), QStringLiteral("/themes/missing-render-color.json")
    )
                 .has_value());

    QJsonObject nullUiColor = themeJsonObject(base);
    QJsonObject nullUiSection = uiSectionOf(nullUiColor);
    nullUiSection.insert(QStringLiteral("windowBackground"), QJsonValue::Null);
    nullUiColor.insert(QStringLiteral("ui"), nullUiSection);
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Theme /themes/null-ui-color\\.json is missing color key windowBackground")
    );
    QVERIFY(!SkyThemeJsonCodec::parseTheme(toJsonBytes(nullUiColor), QStringLiteral("/themes/null-ui-color.json"))
                 .has_value());
}

void SkyThemeJsonCodecTests::parseThemeRejectsInvalidPaletteEntries()
{
    const SkyThemeDefinition base = SkyThemeDefinition::builtInDefault();

    QJsonObject invalidUiColor = themeJsonObject(base);
    QJsonObject uiSection = uiSectionOf(invalidUiColor);
    uiSection.insert(QStringLiteral("windowBackground"), QStringLiteral("not-a-color"));
    invalidUiColor.insert(QStringLiteral("ui"), uiSection);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Theme /themes/invalid-ui-color\\.json has invalid color for key windowBackground")
    );
    QVERIFY(!SkyThemeJsonCodec::parseTheme(toJsonBytes(invalidUiColor), QStringLiteral("/themes/invalid-ui-color.json"))
                 .has_value());

    QJsonObject numericUiColor = themeJsonObject(base);
    QJsonObject numericUiSection = uiSectionOf(numericUiColor);
    numericUiSection.insert(QStringLiteral("windowBackground"), 17);
    numericUiColor.insert(QStringLiteral("ui"), numericUiSection);
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression("Theme /themes/numeric-ui-color\\.json is missing color key windowBackground")
    );
    QVERIFY(!SkyThemeJsonCodec::parseTheme(toJsonBytes(numericUiColor), QStringLiteral("/themes/numeric-ui-color.json"))
                 .has_value());

    QJsonObject invalidRenderColor = themeJsonObject(base);
    QJsonObject renderSection = renderSectionOf(invalidRenderColor);
    renderSection.insert(QStringLiteral("skyGradientTop"), QStringLiteral("not-a-color"));
    invalidRenderColor.insert(QStringLiteral("render"), renderSection);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression("Theme /themes/invalid-render-color\\.json has invalid color for key skyGradientTop")
    );
    QVERIFY(!SkyThemeJsonCodec::parseTheme(
                 toJsonBytes(invalidRenderColor), QStringLiteral("/themes/invalid-render-color.json")
    )
                 .has_value());
}

void SkyThemeJsonCodecTests::parseThemeRejectsMalformedDocuments()
{
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Failed to parse theme /themes/broken\\.json"));
    QVERIFY(!SkyThemeJsonCodec::parseTheme(QByteArrayLiteral("{\"id\": "), QStringLiteral("/themes/broken.json"))
                 .has_value());

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Failed to parse theme /themes/array\\.json"));
    QVERIFY(!SkyThemeJsonCodec::parseTheme("[]", QStringLiteral("/themes/array.json")).has_value());

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Failed to parse theme /themes/empty\\.json"));
    QVERIFY(!SkyThemeJsonCodec::parseTheme(QByteArray(), QStringLiteral("/themes/empty.json")).has_value());
}

QTEST_GUILESS_MAIN(SkyThemeJsonCodecTests)

#include "SkyThemeJsonCodecTests.moc"
