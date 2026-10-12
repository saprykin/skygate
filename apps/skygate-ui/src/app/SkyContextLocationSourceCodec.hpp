#pragma once

#include "SkyContextLocationSource.hpp"

#include <QString>
#include <QStringList>

#include <optional>

namespace skygate::ui::internal {

class SkyContextLocationSourceCodec final {
public:
    [[nodiscard]] static QString toString(SkyContextLocationSource locationSource);
    [[nodiscard]] static std::optional<SkyContextLocationSource> fromString(const QString& value);
    [[nodiscard]] static QStringList availableOptions();
    [[nodiscard]] static SkyContextLocationSource defaultSource();
    [[nodiscard]] static bool isAvailable(SkyContextLocationSource locationSource);
};

}  // namespace skygate::ui::internal
