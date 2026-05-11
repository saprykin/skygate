#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <optional>
#include <vector>

namespace skygate::ui::internal {

#define SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(X) \
    X(windowBackground) \
    X(cardBackground) \
    X(cardBackgroundBottom) \
    X(cardBorder) \
    X(dividerColor) \
    X(headerGradientStart) \
    X(headerGradientEnd) \
    X(textPrimary) \
    X(textSecondary) \
    X(textMuted) \
    X(formLabelText) \
    X(sectionTitleText) \
    X(monospaceText) \
    X(toolbarPanelBackground) \
    X(toolbarPanelBorder) \
    X(toolbarButtonText) \
    X(toolbarButtonBackground) \
    X(toolbarButtonBackgroundHover) \
    X(toolbarButtonBackgroundPressed) \
    X(toolbarButtonBorder) \
    X(toolbarToggleText) \
    X(toolbarToggleBackground) \
    X(toolbarToggleBackgroundHover) \
    X(toolbarToggleBackgroundPressed) \
    X(toolbarToggleBorder) \
    X(toolbarDropdownBackground) \
    X(toolbarDropdownBorder) \
    X(toolbarItemBackgroundHover) \
    X(toolbarItemBackgroundSelected) \
    X(toolbarPrimaryText) \
    X(toolbarSecondaryText) \
    X(inputText) \
    X(inputPlaceholderText) \
    X(inputBackground) \
    X(inputBackgroundDisabled) \
    X(inputBorder) \
    X(inputBorderFocus) \
    X(inputSelection) \
    X(inputSelectionText) \
    X(inputIndicator) \
    X(listItemPrimaryText) \
    X(listItemSecondaryText) \
    X(listItemSectionText) \
    X(listItemBackgroundHover) \
    X(listItemBackgroundSelected) \
    X(emptyStateText) \
    X(actionButtonPrimaryTop) \
    X(actionButtonPrimaryTopHover) \
    X(actionButtonPrimaryTopPressed) \
    X(actionButtonPrimaryBottom) \
    X(actionButtonPrimaryBottomHover) \
    X(actionButtonPrimaryBottomPressed) \
    X(actionButtonPrimaryBorder) \
    X(actionButtonSecondaryTop) \
    X(actionButtonSecondaryTopHover) \
    X(actionButtonSecondaryTopPressed) \
    X(actionButtonSecondaryBottom) \
    X(actionButtonSecondaryBottomHover) \
    X(actionButtonSecondaryBottomPressed) \
    X(actionButtonSecondaryBorder) \
    X(actionButtonDisabledTop) \
    X(actionButtonDisabledBottom) \
    X(actionButtonDisabledBorder) \
    X(actionButtonText) \
    X(actionButtonTextDisabled) \
    X(sectionButtonActiveTop) \
    X(sectionButtonActiveTopHover) \
    X(sectionButtonActiveTopPressed) \
    X(sectionButtonActiveBottom) \
    X(sectionButtonActiveBottomHover) \
    X(sectionButtonActiveBottomPressed) \
    X(sectionButtonActiveBorder) \
    X(sectionButtonInactiveTop) \
    X(sectionButtonInactiveTopHover) \
    X(sectionButtonInactiveTopPressed) \
    X(sectionButtonInactiveBottom) \
    X(sectionButtonInactiveBottomHover) \
    X(sectionButtonInactiveBottomPressed) \
    X(sectionButtonInactiveBorder) \
    X(sectionButtonTextActive) \
    X(sectionButtonTextInactive) \
    X(closeButtonText) \
    X(closeButtonBackground) \
    X(closeButtonBackgroundHover) \
    X(closeButtonBackgroundPressed) \
    X(closeButtonBorder) \
    X(quoteBackground) \
    X(quoteBorder) \
    X(quoteText) \
    X(logoPanelBackground) \
    X(logoPanelBorder) \
    X(logoPanelAccentBorder) \
    X(progressBarTrack) \
    X(progressBarTrackBorder) \
    X(progressBarSweepStart) \
    X(progressBarSweepMid) \
    X(progressBarSweepEnd) \
    X(footerBackground) \
    X(footerLiveText) \
    X(footerPausedText) \
    X(footerLocationText) \
    X(footerInfoText) \
    X(footerTimeText) \
    X(footerTimeHoverBackground) \
    X(footerTimeHoverBorder) \
    X(errorText) \
    X(checkBackground) \
    X(checkBorder) \
    X(checkBorderChecked) \
    X(checkMarkColor)

#define SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(X) \
    X(skyGradientTop) \
    X(skyGradientBottom) \
    X(gridAltitudeLine) \
    X(gridAzimuthLine) \
    X(constellationLine) \
    X(horizonLine) \
    X(eclipticLine) \
    X(celestialEquatorLine) \
    X(circumpolarBoundaryLine) \
    X(cardinalNorthLine) \
    X(cardinalEastLine) \
    X(cardinalSouthLine) \
    X(cardinalWestLine) \
    X(bodySun) \
    X(bodyMoon) \
    X(bodyPlanet) \
    X(bodyStar) \
    X(bodyConstellation) \
    X(bodyDeepSkyObject) \
    X(labelSun) \
    X(labelMoon) \
    X(labelPlanet) \
    X(labelConstellation) \
    X(labelDeepSkyObject) \
    X(labelDefault) \
    X(overlayTooltipBackground) \
    X(overlayTooltipBorder) \
    X(overlayTooltipText) \
    X(overlayLabelBackground) \
    X(overlayCardinalLabelBackground) \
    X(overlayLabelBorder) \
    X(overlayLabelText) \
    X(selectionMarkerFill) \
    X(selectionMarkerBorder) \
    X(selectionMarkerOuterBorder) \
    X(selectionMarkerCenter)

struct SkyThemeUiPalette final {
#define SKYGATE_DECLARE_UI_COLOR_MEMBER(name) QColor name;
    SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_DECLARE_UI_COLOR_MEMBER)
#undef SKYGATE_DECLARE_UI_COLOR_MEMBER
};

struct SkyThemeRenderPalette final {
#define SKYGATE_DECLARE_RENDER_COLOR_MEMBER(name) QColor name;
    SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_DECLARE_RENDER_COLOR_MEMBER)
#undef SKYGATE_DECLARE_RENDER_COLOR_MEMBER
};

struct SkyThemeDefinition final {
    QString id;
    QString displayName;
    SkyThemeUiPalette ui;
    SkyThemeRenderPalette render;
};

struct SkyThemeDescriptor final {
    QString id;
    QString resourcePath;
};

struct SkyThemeOption final {
    QString id;
    QString label;
};

class SkyThemePalette final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id READ id NOTIFY themeChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY themeChanged)
#define SKYGATE_DECLARE_UI_COLOR_PROPERTY(name) Q_PROPERTY(QColor name READ name NOTIFY themeChanged)
    SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_DECLARE_UI_COLOR_PROPERTY)
#undef SKYGATE_DECLARE_UI_COLOR_PROPERTY
#define SKYGATE_DECLARE_RENDER_COLOR_PROPERTY(name) Q_PROPERTY(QColor name READ name NOTIFY themeChanged)
    SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_DECLARE_RENDER_COLOR_PROPERTY)
#undef SKYGATE_DECLARE_RENDER_COLOR_PROPERTY

public:
    explicit SkyThemePalette(QObject* parent = nullptr);

    [[nodiscard]] QString id() const;
    [[nodiscard]] QString displayName() const;
#define SKYGATE_DECLARE_UI_COLOR_GETTER(name) [[nodiscard]] QColor name() const noexcept;
    SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_DECLARE_UI_COLOR_GETTER)
#undef SKYGATE_DECLARE_UI_COLOR_GETTER
#define SKYGATE_DECLARE_RENDER_COLOR_GETTER(name) [[nodiscard]] QColor name() const noexcept;
    SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_DECLARE_RENDER_COLOR_GETTER)
#undef SKYGATE_DECLARE_RENDER_COLOR_GETTER

    [[nodiscard]] const SkyThemeDefinition& definition() const noexcept;
    void setDefinition(const SkyThemeDefinition& definition);

signals:
    void themeChanged();

private:
    SkyThemeDefinition m_definition;
};

class SkyThemeJsonCodec final {
public:
    [[nodiscard]] static std::optional<SkyThemeDefinition> parseTheme(
        const QByteArray& jsonBytes,
        const QString& sourceDescription
    );
};

class SkyThemeRepository final {
public:
    explicit SkyThemeRepository(
        const QString& manifestResourcePath = QStringLiteral(":/themes/manifest.json")
    );

    [[nodiscard]] const SkyThemeDefinition& themeById(const QString& id) const noexcept;
    [[nodiscard]] const SkyThemeDefinition& defaultTheme() const noexcept;
    [[nodiscard]] QVariantList themeOptions() const;

private:
    [[nodiscard]] std::vector<SkyThemeOption> themeOptionData() const;
    void loadManifest(const QString& manifestResourcePath);
    void ensureDefaultTheme();

private:
    std::vector<SkyThemeDescriptor> m_descriptors;
    std::vector<SkyThemeDefinition> m_themes;
    SkyThemeDefinition m_fallbackTheme;
    qsizetype m_defaultThemeIndex = 0;
};

}  // namespace skygate::ui::internal
