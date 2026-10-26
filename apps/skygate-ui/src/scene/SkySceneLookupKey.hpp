#pragma once

#include "AsciiText.hpp"

#include <QString>

#include <string>

namespace skygate::ui::internal {

class SkySceneLookupKey final {
public:
    [[nodiscard]] static QString normalized(const QString& value)
    {
        return QString::fromStdString(skygate::core::AsciiText::normalizedLookupKey(value.toStdString()));
    }

    [[nodiscard]] static QString normalized(const std::string& value)
    {
        return QString::fromStdString(skygate::core::AsciiText::normalizedLookupKey(value));
    }
};

}  // namespace skygate::ui::internal
