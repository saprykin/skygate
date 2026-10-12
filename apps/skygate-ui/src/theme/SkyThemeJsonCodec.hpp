#pragma once

#include "SkyThemeDefinition.hpp"

#include <QByteArray>
#include <QString>

#include <optional>

namespace skygate::ui::internal {

class SkyThemeJsonCodec final {
public:
    [[nodiscard]] static std::optional<SkyThemeDefinition>
    parseTheme(const QByteArray& jsonBytes, const QString& sourceDescription);
};

}  // namespace skygate::ui::internal
