#pragma once

#include "SkyThemeDefinition.hpp"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace skygate::ui::internal {

class SkyThemeJsonCodec final {
public:
    // Parses one theme document into its JSON object. description names the
    // document in the diagnostics, for example "theme /themes/night.json".
    [[nodiscard]] static std::optional<QJsonObject>
    parseObject(const QByteArray& jsonBytes, const QString& description);

    // Reads a string value as trimmed text and rejects empty and non-string
    // values. description names the document the key belongs to.
    [[nodiscard]] static std::optional<QString>
    requiredString(const QJsonObject& object, const char* key, const QString& description);

    [[nodiscard]] static std::optional<SkyThemeDefinition>
    parseTheme(const QByteArray& jsonBytes, const QString& sourceDescription);
};

}  // namespace skygate::ui::internal
