#pragma once

#include "SkyThemeDefinition.hpp"
#include "SkyThemeRenderPalette.hpp"
#include "SkyThemeUiPalette.hpp"

#include <QColor>
#include <QObject>
#include <QString>

namespace skygate::ui::internal {

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

}  // namespace skygate::ui::internal
