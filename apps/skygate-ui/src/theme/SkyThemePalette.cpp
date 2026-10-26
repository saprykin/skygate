#include "SkyThemePalette.hpp"
#include "SkyThemeDefinition.hpp"

namespace skygate::ui::internal {

SkyThemePalette::SkyThemePalette(QObject* parent) : QObject(parent), m_definition(SkyThemeDefinition::builtInDefault())
{
}

QString SkyThemePalette::id() const
{
    return m_definition.id;
}

#define SKYGATE_DEFINE_UI_COLOR_GETTER(name)                                                                           \
    QColor SkyThemePalette::name() const noexcept                                                                      \
    {                                                                                                                  \
        return m_definition.ui.name;                                                                                   \
    }
SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_DEFINE_UI_COLOR_GETTER)
#undef SKYGATE_DEFINE_UI_COLOR_GETTER

#define SKYGATE_DEFINE_RENDER_COLOR_GETTER(name)                                                                       \
    QColor SkyThemePalette::name() const noexcept                                                                      \
    {                                                                                                                  \
        return m_definition.render.name;                                                                               \
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

}  // namespace skygate::ui::internal
