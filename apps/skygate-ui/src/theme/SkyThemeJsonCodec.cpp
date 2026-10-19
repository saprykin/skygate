#include "SkyThemeJsonCodec.hpp"
#include "SkyThemeDefinition.hpp"
#include "SkyThemeRenderPalette.hpp"
#include "SkyThemeUiPalette.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>

#include <array>
#include <utility>

namespace skygate::ui::internal {
namespace {

Q_LOGGING_CATEGORY(skygateThemeLog, "skygate.theme")

[[nodiscard]] std::optional<QColor>
parseColor(const QJsonObject& object, const char* key, const QString& sourceDescription)
{
    const QJsonValue value = object.value(QString::fromUtf8(key));
    if (!value.isString()) {
        qCWarning(skygateThemeLog).noquote() << "Theme" << sourceDescription << "is missing color key" << key;
        return std::nullopt;
    }

    const QColor color(value.toString());
    if (!color.isValid()) {
        qCWarning(skygateThemeLog).noquote() << "Theme" << sourceDescription << "has invalid color for key" << key;
        return std::nullopt;
    }

    return color;
}

template <typename Palette>
[[nodiscard]] bool
parsePalette(Palette& palette, const QJsonObject& object, const QString& sourceDescription, const auto& assigners)
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
    static const auto kAssigners = std::to_array<std::pair<const char*, PaletteAssigner<SkyThemeUiPalette>>>({
#define SKYGATE_UI_ASSIGNER(name)                                                                                      \
    {#name, [](SkyThemeUiPalette& palette, const QColor& color) { palette.name = color; }},
        SKYGATE_UI_THEME_UI_COLOR_PROPERTIES(SKYGATE_UI_ASSIGNER)
#undef SKYGATE_UI_ASSIGNER
    });
    return kAssigners;
}

[[nodiscard]] const auto& renderAssigners()
{
    static const auto kAssigners = std::to_array<std::pair<const char*, PaletteAssigner<SkyThemeRenderPalette>>>({
#define SKYGATE_RENDER_ASSIGNER(name)                                                                                  \
    {#name, [](SkyThemeRenderPalette& palette, const QColor& color) { palette.name = color; }},
        SKYGATE_UI_THEME_RENDER_COLOR_PROPERTIES(SKYGATE_RENDER_ASSIGNER)
#undef SKYGATE_RENDER_ASSIGNER
    });
    return kAssigners;
}

}  // namespace

std::optional<QJsonObject> SkyThemeJsonCodec::parseObject(const QByteArray& jsonBytes, const QString& description)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(skygateThemeLog).noquote() << "Failed to parse" << description << parseError.errorString();
        return std::nullopt;
    }

    return document.object();
}

std::optional<QString>
SkyThemeJsonCodec::requiredString(const QJsonObject& object, const char* key, const QString& description)
{
    const QString text = object.value(QString::fromUtf8(key)).toString().trimmed();
    if (text.isEmpty()) {
        qCWarning(skygateThemeLog).noquote() << description << "is missing a value for key" << key;
        return std::nullopt;
    }

    return text;
}

std::optional<SkyThemeDefinition>
SkyThemeJsonCodec::parseTheme(const QByteArray& jsonBytes, const QString& sourceDescription)
{
    const QString documentDescription = QStringLiteral("theme %1").arg(sourceDescription);
    const std::optional<QJsonObject> rootObject = parseObject(jsonBytes, documentDescription);
    if (!rootObject.has_value()) {
        return std::nullopt;
    }

    const std::optional<QString> id = requiredString(rootObject.value(), "id", documentDescription);
    const std::optional<QString> displayName = requiredString(rootObject.value(), "displayName", documentDescription);
    if (!id.has_value() || !displayName.has_value()) {
        return std::nullopt;
    }

    const QJsonValue uiValue = rootObject->value("ui");
    const QJsonValue renderValue = rootObject->value("render");
    if (!uiValue.isObject() || !renderValue.isObject()) {
        qCWarning(skygateThemeLog).noquote() << "Theme" << sourceDescription << "is missing ui/render sections";
        return std::nullopt;
    }

    SkyThemeDefinition theme;
    theme.id = id.value();
    theme.displayName = displayName.value();
    if (!parsePalette(theme.ui, uiValue.toObject(), sourceDescription, uiAssigners())) {
        return std::nullopt;
    }
    if (!parsePalette(theme.render, renderValue.toObject(), sourceDescription, renderAssigners())) {
        return std::nullopt;
    }
    return theme;
}

}  // namespace skygate::ui::internal
