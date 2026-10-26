#pragma once

#include "ProjectionType.hpp"

#include <QString>
#include <QStringList>

#include <optional>

namespace skygate::ui::internal {

class SkyContextProjectionTypeCodec final {
public:
    [[nodiscard]] static QString toString(skygate::core::ProjectionType projectionType);
    [[nodiscard]] static std::optional<skygate::core::ProjectionType> fromString(const QString& value);
    [[nodiscard]] static QStringList availableOptions();
};

}  // namespace skygate::ui::internal
