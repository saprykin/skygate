#pragma once

#include <QString>

namespace skygate::ui::internal {

class SkySettingsKeyBuilder final {
public:
    [[nodiscard]] static QString key(const QString& name);
};

}  // namespace skygate::ui::internal
