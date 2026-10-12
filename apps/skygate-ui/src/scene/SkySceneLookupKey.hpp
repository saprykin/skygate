#pragma once

#include <QString>

#include <string>

namespace skygate::ui::internal {

class SkySceneLookupKey final {
public:
    [[nodiscard]] static QString normalized(const QString& value)
    {
        return value.trimmed().toCaseFolded();
    }

    [[nodiscard]] static QString normalized(const std::string& value)
    {
        return normalized(QString::fromStdString(value));
    }
};

}  // namespace skygate::ui::internal
