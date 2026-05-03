#include "SkyTheme.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QVariantMap>

#include <array>
#include <utility>

void initSkygateThemeResources()
{
    Q_INIT_RESOURCE(themes);
}

namespace skygate::ui::internal {

namespace {

Q_LOGGING_CATEGORY(skygateThemeLog, "skygate.theme")

[[nodiscard]] SkyThemeDefinition makeBuiltInDefaultTheme()
{
    SkyThemeDefinition theme;
    theme.id = "default";
    theme.displayName = "Default";

    theme.ui.windowBackground = QColor("#171b30");
    theme.ui.cardBackground = QColor("#1d2138");
    theme.ui.cardBackgroundBottom = QColor("#1a1e33");
    theme.ui.cardBorder = QColor("#474d70");
    theme.ui.dividerColor = QColor("#343955");
    theme.ui.headerGradientStart = QColor("#111731");
    theme.ui.headerGradientEnd = QColor("#00111731");
    theme.ui.textPrimary = QColor("#f2f4ff");
    theme.ui.textSecondary = QColor("#9ca3c5");
    theme.ui.textMuted = QColor("#878dad");
    theme.ui.formLabelText = QColor("#d4d8ee");
    theme.ui.sectionTitleText = QColor("#d8dcf2");
    theme.ui.monospaceText = QColor("#7f87ad");
    theme.ui.toolbarPanelBackground = QColor("#7f0b1428");
    theme.ui.toolbarPanelBorder = QColor("#335177");
    theme.ui.toolbarButtonText = QColor("#e8f4ff");
    theme.ui.toolbarButtonBackground = QColor("#1e3d63");
    theme.ui.toolbarButtonBackgroundHover = QColor("#335b89");
    theme.ui.toolbarButtonBackgroundPressed = QColor("#2a4a72");
    theme.ui.toolbarButtonBorder = QColor("#75bde8");
    theme.ui.toolbarToggleText = QColor("#eaf7ff");
    theme.ui.toolbarToggleBackground = QColor("#1a3352");
    theme.ui.toolbarToggleBackgroundHover = QColor("#315881");
    theme.ui.toolbarToggleBackgroundPressed = QColor("#27476d");
    theme.ui.toolbarToggleBorder = QColor("#6fbde6");
    theme.ui.toolbarDropdownBackground = QColor("#102745");
    theme.ui.toolbarDropdownBorder = QColor("#4f79a8");
    theme.ui.toolbarItemBackgroundHover = QColor("#1c3b60");
    theme.ui.toolbarItemBackgroundSelected = QColor("#2f5f94");
    theme.ui.toolbarPrimaryText = QColor("#e8f4ff");
    theme.ui.toolbarSecondaryText = QColor("#bfd6f5");
    theme.ui.inputText = QColor("#f1f3ff");
    theme.ui.inputPlaceholderText = QColor("#8187ab");
    theme.ui.inputBackground = QColor("#232742");
    theme.ui.inputBackgroundDisabled = QColor("#1b1e33");
    theme.ui.inputBorder = QColor("#4d5378");
    theme.ui.inputBorderFocus = QColor("#628fd9");
    theme.ui.inputSelection = QColor("#4068b4");
    theme.ui.inputSelectionText = QColor("#f4f6ff");
    theme.ui.inputIndicator = QColor("#9ca3c5");
    theme.ui.listItemPrimaryText = QColor("#eef2ff");
    theme.ui.listItemSecondaryText = QColor("#9ca3c5");
    theme.ui.listItemSectionText = QColor("#9ca3c5");
    theme.ui.listItemBackgroundHover = QColor("#272c46");
    theme.ui.listItemBackgroundSelected = QColor("#365e9c");
    theme.ui.emptyStateText = QColor("#bfd6f5");
    theme.ui.actionButtonPrimaryTop = QColor("#386cb4");
    theme.ui.actionButtonPrimaryTopHover = QColor("#3d79c3");
    theme.ui.actionButtonPrimaryTopPressed = QColor("#2d62a8");
    theme.ui.actionButtonPrimaryBottom = QColor("#305f9d");
    theme.ui.actionButtonPrimaryBottomHover = QColor("#366cad");
    theme.ui.actionButtonPrimaryBottomPressed = QColor("#29599a");
    theme.ui.actionButtonPrimaryBorder = QColor("#5f97e5");
    theme.ui.actionButtonSecondaryTop = QColor("#232741");
    theme.ui.actionButtonSecondaryTopHover = QColor("#262b47");
    theme.ui.actionButtonSecondaryTopPressed = QColor("#20243d");
    theme.ui.actionButtonSecondaryBottom = QColor("#1d2137");
    theme.ui.actionButtonSecondaryBottomHover = QColor("#21253d");
    theme.ui.actionButtonSecondaryBottomPressed = QColor("#1a1e33");
    theme.ui.actionButtonSecondaryBorder = QColor("#3e4465");
    theme.ui.actionButtonDisabledTop = QColor("#252943");
    theme.ui.actionButtonDisabledBottom = QColor("#1d2035");
    theme.ui.actionButtonDisabledBorder = QColor("#353955");
    theme.ui.actionButtonText = QColor("#f2f4ff");
    theme.ui.actionButtonTextDisabled = QColor("#8f95b2");
    theme.ui.sectionButtonActiveTop = QColor("#3867a9");
    theme.ui.sectionButtonActiveTopHover = QColor("#3e70b8");
    theme.ui.sectionButtonActiveTopPressed = QColor("#355f9f");
    theme.ui.sectionButtonActiveBottom = QColor("#30598f");
    theme.ui.sectionButtonActiveBottomHover = QColor("#365f9d");
    theme.ui.sectionButtonActiveBottomPressed = QColor("#2c548f");
    theme.ui.sectionButtonActiveBorder = QColor("#5d91dc");
    theme.ui.sectionButtonInactiveTop = QColor("#20243d");
    theme.ui.sectionButtonInactiveTopHover = QColor("#242943");
    theme.ui.sectionButtonInactiveTopPressed = QColor("#20233a");
    theme.ui.sectionButtonInactiveBottom = QColor("#1b1f33");
    theme.ui.sectionButtonInactiveBottomHover = QColor("#1f2237");
    theme.ui.sectionButtonInactiveBottomPressed = QColor("#1a1d31");
    theme.ui.sectionButtonInactiveBorder = QColor("#242843");
    theme.ui.sectionButtonTextActive = QColor("#f3f5ff");
    theme.ui.sectionButtonTextInactive = QColor("#eef0fd");
    theme.ui.closeButtonText = QColor("#edf1ff");
    theme.ui.closeButtonBackground = QColor("#1f2339");
    theme.ui.closeButtonBackgroundHover = QColor("#292f4c");
    theme.ui.closeButtonBackgroundPressed = QColor("#232843");
    theme.ui.closeButtonBorder = QColor("#596084");
    theme.ui.quoteBackground = QColor("#232742");
    theme.ui.quoteBorder = QColor("#4c5278");
    theme.ui.quoteText = QColor("#edf0fe");
    theme.ui.logoPanelBackground = QColor("#151a36");
    theme.ui.logoPanelBorder = QColor("#6676ab");
    theme.ui.logoPanelAccentBorder = QColor("#9dafeb");
    theme.ui.progressBarTrack = QColor("#181b2d");
    theme.ui.progressBarTrackBorder = QColor("#3b4060");
    theme.ui.progressBarSweepStart = QColor("#4a82df00");
    theme.ui.progressBarSweepMid = QColor("#4a82df");
    theme.ui.progressBarSweepEnd = QColor("#4a82df22");
    theme.ui.footerBackground = QColor("#0b1428");
    theme.ui.footerLiveText = QColor("#7fe39f");
    theme.ui.footerPausedText = QColor("#f2b0b0");
    theme.ui.footerLocationText = QColor("#a9c4ff");
    theme.ui.footerInfoText = QColor("#9ab0d6");
    theme.ui.footerTimeText = QColor("#d7e3ff");
    theme.ui.footerTimeHoverBackground = QColor("#10203d");
    theme.ui.footerTimeHoverBorder = QColor("#3a5885");
    theme.ui.errorText = QColor("#f2b0b0");
    theme.ui.checkBackground = QColor("#232742");
    theme.ui.checkBorder = QColor("#52587d");
    theme.ui.checkBorderChecked = QColor("#628fd9");
    theme.ui.checkMarkColor = QColor("#d7e6ff");

    theme.render.skyGradientTop = QColor("#081022");
    theme.render.skyGradientBottom = QColor("#040912");
    theme.render.gridAltitudeLine = QColor(112, 146, 194, 70);
    theme.render.gridAzimuthLine = QColor(102, 138, 184, 58);
    theme.render.constellationLine = QColor(146, 205, 255, 132);
    theme.render.horizonLine = QColor(255, 170, 92, 220);
    theme.render.eclipticLine = QColor(255, 204, 112, 150);
    theme.render.celestialEquatorLine = QColor(132, 196, 255, 120);
    theme.render.circumpolarBoundaryLine = QColor(188, 162, 255, 120);
    theme.render.cardinalNorthLine = QColor(130, 216, 255, 132);
    theme.render.cardinalEastLine = QColor(255, 208, 136, 132);
    theme.render.cardinalSouthLine = QColor(255, 158, 158, 132);
    theme.render.cardinalWestLine = QColor(156, 232, 198, 132);
    theme.render.bodySun = QColor(255, 214, 128, 230);
    theme.render.bodyMoon = QColor(162, 245, 255, 235);
    theme.render.bodyPlanet = QColor(255, 188, 140, 220);
    theme.render.bodyStar = QColor(188, 214, 255, 210);
    theme.render.bodyConstellation = QColor(160, 244, 200, 205);
    theme.render.bodyDeepSkyObject = QColor(126, 230, 202, 185);
    theme.render.labelSun = QColor(255, 224, 135, 235);
    theme.render.labelMoon = QColor(152, 247, 255, 245);
    theme.render.labelPlanet = QColor(255, 196, 148, 235);
    theme.render.labelConstellation = QColor(201, 220, 255, 230);
    theme.render.labelDeepSkyObject = QColor(168, 242, 218, 225);
    theme.render.labelDefault = QColor(201, 220, 255, 230);
    theme.render.overlayTooltipBackground = QColor("#cc0a1222");
    theme.render.overlayTooltipBorder = QColor("#6b8fc8");
    theme.render.overlayTooltipText = QColor("#e6f0ff");
    theme.render.overlayLabelBackground = QColor("#aa071328");
    theme.render.overlayCardinalLabelBackground = QColor("#880a1222");
    theme.render.overlayLabelBorder = QColor("#c9dcff");
    theme.render.overlayLabelText = QColor("#c9dcff");
    theme.render.selectionMarkerFill = QColor("#18ff4d4d");
    theme.render.selectionMarkerBorder = QColor("#ff6b6b");
    theme.render.selectionMarkerOuterBorder = QColor("#66ff6b6b");
    theme.render.selectionMarkerCenter = QColor("#ffb3b3");

    return theme;
}

[[nodiscard]] std::optional<QColor> parseColor(
    const QJsonObject& object,
    const char* key,
    const QString& sourceDescription
)
{
    const QJsonValue value = object.value(QString::fromUtf8(key));
    if (!value.isString()) {
        qCWarning(skygateThemeLog).noquote()
            << "Theme" << sourceDescription << "is missing color key" << key;
        return std::nullopt;
    }

    const QColor color(value.toString());
    if (!color.isValid()) {
        qCWarning(skygateThemeLog).noquote()
            << "Theme" << sourceDescription << "has invalid color for key" << key;
        return std::nullopt;
    }

    return color;
}

template <typename Palette>
[[nodiscard]] bool parsePalette(
    Palette& palette,
    const QJsonObject& object,
    const QString& sourceDescription,
    const auto& assigners
)
{
    for (const auto& [key, assign] : assigners) {
        const std::optional<QColor> color = parseColor(object, key, sourceDescription);
        if (!color.has_value()) {
            return false;
        }
        assign(palette, color.value());
    }

    return true;
}

template <typename Palette>
using PaletteAssigner = void (*)(Palette&, const QColor&);

[[nodiscard]] const auto& uiAssigners()
{
    static const auto kAssigners = std::to_array<
        std::pair<const char*, PaletteAssigner<SkyThemeUiPalette>>
    >({
#define SKYGATE_UI_ASSIGNER(name) {#name, [](SkyThemeUiPalette& palette, const QColor& color) { palette.name = color; }},
            SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_UI_ASSIGNER)
#undef SKYGATE_UI_ASSIGNER
    });
    return kAssigners;
}

[[nodiscard]] const auto& renderAssigners()
{
    static const auto kAssigners = std::to_array<
        std::pair<const char*, PaletteAssigner<SkyThemeRenderPalette>>
    >({
#define SKYGATE_RENDER_ASSIGNER(name) {#name, [](SkyThemeRenderPalette& palette, const QColor& color) { palette.name = color; }},
            SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_RENDER_ASSIGNER)
#undef SKYGATE_RENDER_ASSIGNER
    });
    return kAssigners;
}

[[nodiscard]] std::optional<QByteArray> readResourceBytes(
    const QString& resourcePath,
    const QString& description
)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(skygateThemeLog).noquote() << "Failed to open" << description << resourcePath;
        return std::nullopt;
    }
    return file.readAll();
}

void ensureThemeResourcesInitialized()
{
    static QMutex mutex;
    static bool initialized = false;

    const QMutexLocker locker(&mutex);
    if (initialized) {
        return;
    }

    initSkygateThemeResources();
    initialized = true;
}

}  // namespace

SkyThemePalette::SkyThemePalette(QObject* parent)
    : QObject(parent)
    , m_definition(makeBuiltInDefaultTheme())
{
}

QString SkyThemePalette::id() const
{
    return m_definition.id;
}

QString SkyThemePalette::displayName() const
{
    return m_definition.displayName;
}

#define SKYGATE_DEFINE_UI_COLOR_GETTER(name) \
    QColor SkyThemePalette::name() const noexcept \
    { \
        return m_definition.ui.name; \
    }
SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_DEFINE_UI_COLOR_GETTER)
#undef SKYGATE_DEFINE_UI_COLOR_GETTER

#define SKYGATE_DEFINE_RENDER_COLOR_GETTER(name) \
    QColor SkyThemePalette::name() const noexcept \
    { \
        return m_definition.render.name; \
    }
SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_DEFINE_RENDER_COLOR_GETTER)
#undef SKYGATE_DEFINE_RENDER_COLOR_GETTER

const SkyThemeDefinition& SkyThemePalette::definition() const noexcept
{
    return m_definition;
}

void SkyThemePalette::setDefinition(const SkyThemeDefinition& definition)
{
    m_definition = definition;
    emit themeChanged();
}

std::optional<SkyThemeDefinition> SkyThemeJsonCodec::parseTheme(
    const QByteArray& jsonBytes,
    const QString& sourceDescription
)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(skygateThemeLog).noquote()
            << "Failed to parse theme" << sourceDescription << parseError.errorString();
        return std::nullopt;
    }

    const QJsonObject rootObject = document.object();
    const QString id = rootObject.value("id").toString().trimmed();
    const QString displayName = rootObject.value("displayName").toString().trimmed();
    if (id.isEmpty() || displayName.isEmpty()) {
        qCWarning(skygateThemeLog).noquote()
            << "Theme" << sourceDescription << "is missing id or displayName";
        return std::nullopt;
    }

    const QJsonValue uiValue = rootObject.value("ui");
    const QJsonValue renderValue = rootObject.value("render");
    if (!uiValue.isObject() || !renderValue.isObject()) {
        qCWarning(skygateThemeLog).noquote()
            << "Theme" << sourceDescription << "is missing ui/render sections";
        return std::nullopt;
    }

    SkyThemeDefinition theme;
    theme.id = id;
    theme.displayName = displayName;
    if (!parsePalette(theme.ui, uiValue.toObject(), sourceDescription, uiAssigners())) {
        return std::nullopt;
    }
    if (!parsePalette(theme.render, renderValue.toObject(), sourceDescription, renderAssigners())) {
        return std::nullopt;
    }
    return theme;
}

SkyThemeRepository::SkyThemeRepository(const QString& manifestResourcePath)
    : m_fallbackTheme(makeBuiltInDefaultTheme())
{
    ensureThemeResourcesInitialized();
    loadManifest(manifestResourcePath);
    ensureDefaultTheme();
}

const SkyThemeDefinition& SkyThemeRepository::themeById(const QString& id) const noexcept
{
    const QString requestedId = id.trimmed();
    if (!requestedId.isEmpty()) {
        for (const auto& theme : m_themes) {
            if (theme.id == requestedId) {
                return theme;
            }
        }

        qCWarning(skygateThemeLog).noquote()
            << "Unknown theme id" << requestedId << "- using default theme";
    }

    return defaultTheme();
}

const SkyThemeDefinition& SkyThemeRepository::defaultTheme() const noexcept
{
    if (m_defaultThemeIndex >= 0 && m_defaultThemeIndex < static_cast<qsizetype>(m_themes.size())) {
        return m_themes[static_cast<std::size_t>(m_defaultThemeIndex)];
    }
    return m_fallbackTheme;
}

QVariantList SkyThemeRepository::themeOptions() const
{
    QVariantList options;
    options.reserve(static_cast<qsizetype>(m_themes.size()));
    for (const auto& theme : m_themes) {
        QVariantMap entry;
        entry.insert("id", theme.id);
        entry.insert("label", theme.displayName);
        options.push_back(entry);
    }

    if (options.isEmpty()) {
        QVariantMap entry;
        entry.insert("id", m_fallbackTheme.id);
        entry.insert("label", m_fallbackTheme.displayName);
        options.push_back(entry);
    }
    return options;
}

void SkyThemeRepository::loadManifest(const QString& manifestResourcePath)
{
    const std::optional<QByteArray> manifestBytes = readResourceBytes(
        manifestResourcePath,
        "theme manifest"
    );
    if (!manifestBytes.has_value()) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument manifestDocument = QJsonDocument::fromJson(
        manifestBytes.value(),
        &parseError
    );
    if (parseError.error != QJsonParseError::NoError || !manifestDocument.isObject()) {
        qCWarning(skygateThemeLog).noquote()
            << "Failed to parse theme manifest"
            << manifestResourcePath
            << parseError.errorString();
        return;
    }

    const QJsonObject manifestObject = manifestDocument.object();
    const QString defaultThemeId = manifestObject.value("defaultThemeId").toString().trimmed();
    const QJsonArray themeArray = manifestObject.value("themes").toArray();
    for (const QJsonValue& themeValue : themeArray) {
        if (!themeValue.isObject()) {
            continue;
        }

        const QJsonObject themeObject = themeValue.toObject();
        const QString id = themeObject.value("id").toString().trimmed();
        const QString resourcePath = themeObject.value("resourcePath").toString().trimmed();
        if (id.isEmpty() || resourcePath.isEmpty()) {
            qCWarning(skygateThemeLog).noquote()
                << "Skipping invalid theme descriptor in manifest"
                << manifestResourcePath;
            continue;
        }

        const std::optional<QByteArray> themeBytes = readResourceBytes(resourcePath, "theme");
        if (!themeBytes.has_value()) {
            continue;
        }

        const std::optional<SkyThemeDefinition> parsedTheme = SkyThemeJsonCodec::parseTheme(
            themeBytes.value(),
            resourcePath
        );
        if (!parsedTheme.has_value()) {
            continue;
        }

        if (parsedTheme->id != id) {
            qCWarning(skygateThemeLog).noquote()
                << "Theme manifest id mismatch for"
                << resourcePath
                << "- expected"
                << id
                << "got"
                << parsedTheme->id;
            continue;
        }

        if (parsedTheme->id == defaultThemeId) {
            m_defaultThemeIndex = static_cast<qsizetype>(m_themes.size());
        }
        m_descriptors.push_back(SkyThemeDescriptor {
            .id = id,
            .resourcePath = resourcePath
        });
        m_themes.push_back(parsedTheme.value());
    }
}

void SkyThemeRepository::ensureDefaultTheme()
{
    if (m_themes.empty()) {
        m_themes.push_back(m_fallbackTheme);
        m_defaultThemeIndex = 0;
        return;
    }

    if (m_defaultThemeIndex < 0 || m_defaultThemeIndex >= static_cast<qsizetype>(m_themes.size())) {
        for (qsizetype index = 0; index < static_cast<qsizetype>(m_themes.size()); ++index) {
            if (m_themes[static_cast<std::size_t>(index)].id == m_fallbackTheme.id) {
                m_defaultThemeIndex = index;
                return;
            }
        }

        m_defaultThemeIndex = 0;
    }
}

}  // namespace skygate::ui::internal
