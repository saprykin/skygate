#pragma once

#include <QString>

namespace skygate::ui::internal {

class SkyContextSettings final {
public:
    [[nodiscard]] static QString key(const QString& name);
};

}  // namespace skygate::ui::internal
