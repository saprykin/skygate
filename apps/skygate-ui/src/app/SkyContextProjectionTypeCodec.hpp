#pragma once

#include "ProjectionType.hpp"

#include <QString>

#include <optional>

namespace skygate::ui::internal {

class SkyContextProjectionTypeCodec final {
public:
    [[nodiscard]] static QString toString(skygate::core::ProjectionType projectionType);
    [[nodiscard]] static std::optional<skygate::core::ProjectionType> fromString(const QString& value);
};

}  // namespace skygate::ui::internal
